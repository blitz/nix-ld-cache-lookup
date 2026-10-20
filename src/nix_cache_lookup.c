#define _GNU_SOURCE

#include <assert.h>
#include <elf.h>
#include <link.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/stat.h>

/*
 * The descriptor is a sequence of NUL-terminated (needed, path-list)
 * string pairs, where each path-list element is either
 * =<absolute-path> for a directly resolved library or ?<dir> for a
 * directory the loader must still search itself (used for
 * $ORIGIN-relative and glibc-hwcaps directories).
 *
 * So far, I haven't found a good example of ?<dir> where the
 * directory actually exists...
 */

#define MAX_CACHE_LEN 1024
#define MAX_LOCATIONS 16

#if 1
#define LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define LOG(...) ((void)0)
#endif

struct so_entry {
  const char *so_name;
  const char *location[MAX_LOCATIONS];
};

/*
 * We store the cache in a preparsed form here. This also allows is to
 * ignore ?<dir> entries pointing to directories that don't exist.
 */
static struct so_entry cache[MAX_CACHE_LEN];

static void parse_entry(const char *key, const char *value,
                        struct so_entry *out) {
  LOG("Parsing locations for: %s\n", key);
  out->so_name = key;

  /* strtok modifies the value it parses. */
  char *copied_value = strdup(value);
  size_t cur_location = 0;

  for (char *p = strtok(copied_value, ":"); p != NULL; p = strtok(NULL, ":")) {
    struct stat st;
    int rc;
    LOG("TOK: %s\n", p);

    /* If the directory doesn't exist, there is no point looking into it later.
     */
    if (p[0] == '?' && (rc = stat(p + 1, &st)) != 0) {
      LOG("SKIP: %s (%d)\n", p, rc);
      continue;
    }

    /* TODO Bounds checks */
    if (cur_location >= MAX_LOCATIONS) {
      abort();
    }
    out->location[cur_location++] = p;
  }

  LOG("Parse entry with %zu parts\n", cur_location);
}

unsigned int la_version(unsigned int version) {
  Elf64_Phdr *phdr = (Elf64_Phdr *)getauxval(AT_PHDR);

  // Retrieve the number of program headers
  unsigned long phnum = getauxval(AT_PHNUM);

  LOG("Found %lu phdrs at %p\n", phnum, phdr);
  // Determine load base for PIE binaries
  ElfW(Addr) load_base = 0;
  for (long i = 0; i < phnum; i++) {
    if (phdr[i].p_type == PT_PHDR) {
      load_base = (ElfW(Addr))phdr - phdr[i].p_vaddr;
      break;
    }
  }
  LOG("Load base: %zx\n", (size_t)load_base);

  const char *cache_data;
  size_t cache_len;

  // Parse PT_NOTE segments
  for (long i = 0; i < phnum; i++) {
    if (phdr[i].p_type == PT_NOTE) {
      ElfW(Addr) note_start = load_base + phdr[i].p_vaddr;
      ElfW(Addr) note_end = note_start + phdr[i].p_memsz;

      ElfW(Nhdr) *note = (ElfW(Nhdr) *)note_start;

      while ((ElfW(Addr))note < note_end) {
        char *name = (char *)note + sizeof(ElfW(Nhdr));
        void *desc = name + ((note->n_namesz + 3) & ~3);
        size_t len = note->n_descsz;

        LOG("Note name: %s, Type: 0x%x, Length: %zu\n", name, note->n_type,
            len);

        if (note->n_type == 0x63a86cb6U) {
          cache_data = desc;
          cache_len = len;
          goto found;
        }

        note = (ElfW(Nhdr) *)((char *)desc + ((note->n_descsz + 3) & ~3));
      }
    }
  }
  return LAV_CURRENT;

found:

    ;
  size_t cur_cache_entry = 0;

  for (const char *cur = cache_data; cur < cache_data + cache_len;) {
    const char *key = cur;
    size_t key_len = strlen(cur);

    if (key_len == 0)
      break;

    /* TODO Assert bounds */
    const char *val = cur + key_len + 1;
    size_t val_len = strlen(val);

    parse_entry(key, val, &cache[cur_cache_entry++]);

    cur = val + val_len + 1;
  }

  LOG("DONE\n");
  return LAV_CURRENT;
}

char *la_objsearch(const char *name, uintptr_t *cookie, unsigned int flag) {
  if (flag != LA_SER_ORIG) {
    return (char *)name;
  }

  for (size_t i_cache = 0; i_cache < MAX_CACHE_LEN && cache[i_cache].so_name;
       i_cache++) {
    if (strcmp(name, cache[i_cache].so_name) != 0) {
      continue;
    }

    for (size_t i_location = 0;
         i_location < MAX_LOCATIONS && cache[i_cache].location[i_location];
         i_location++) {
      char type = cache[i_cache].location[i_location][0];
      const char *loc = cache[i_cache].location[i_location] + 1;

      switch (type) {
      case '=':
        LOG("FOUND: %s -> %s\n", name, loc);
        return (char *)loc;
      case '?':
        LOG("TODO: %s -> %s\n", name, loc);
        abort();
        break;
      }
    }
  }

  LOG("NOTFOUND: %s\n", name);
  return (char *)name;
}
