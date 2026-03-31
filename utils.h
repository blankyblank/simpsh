#ifndef MALLOC_H
#define MALLOC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
/* malloc and free or other small inlined functions */

static inline void
read_assn(const char *assn, char **name, char **value) {
  char *eq;
  int l;
  eq = (char *)strchr(assn, '=');
  if (!eq) {
    *name = strdup(assn);
    *value = NULL;
  } else {
    l = eq - assn;
    *name = strndup(assn, l);
    *value = strdup(eq + 1);
  }
}


static inline size_t arrlen(char **arr) {
  size_t n = 0;
  while (arr[n]) n++;
  return n;
}

static inline int is_ws(char c) {
  return c == ' ' || c == '\t' || c == '\n';
}

static inline const char *skip_ws(const char *s) {
  while (is_ws(*s)) s++;
  return s;
}

static inline char *
s_realloc(char *buf, size_t *bufsize) {
  char *t;

  *bufsize *= 2;
  t = realloc(buf, *bufsize);
  if (!t) {
    free(buf);
    return NULL;
  }
  return t;
}

static inline void
freeptr(char **args) {
  if (args != NULL) {
    int i = 0;
    while (args[i] != NULL) {
      free(args[i]);
      i++;
    }
    free(args);
  }
}

static inline unsigned int
hash(const char *s, unsigned int buckets) {
  unsigned int h = 0;
  while (*s) {
    h = h * 31 + (unsigned char)*s;
    s++;
  }
  return h % buckets;
}

/* ============== HASH TABLE MACROS ============== */
/*
 * HASH_LOOKUP - Find an entry in a hash table
 * @table:      The hash table array
 * @buckets:    Number of buckets
 * @name:       Key to search for (const char *)
 * @key_field:  Name of key field in struct (e.g., name)
 * @type:       Struct type (e.g., alias)
 * @out:        Output variable to store result
 *
 * Usage:
 *   alias *result;
 *   HASH_LOOKUP(alias_tab, ALIAS_BUCKETS, name, name, alias, result);
 *   if (result) { ... }
 */

#define HASH_LOOKUP(table, buckets, name, key_field, type, out) \
  do { \
    unsigned int _i = hash(name, buckets); \
    type *_e = table[_i]; \
    while (_e) { \
      if (strcmp(_e->key_field, name) == 0) \
        break; \
      _e = _e->next; \
    } \
    out = _e; \
  } while (0)

/*
 * HASH_INSERT - Insert or update an entry in a hash table
 * @table:      The hash table array
 * @buckets:    Number of buckets
 * @name:       Key name (const char *)
 * @val:        Value string (const char *)
 * @type:       Struct type
 * @key_field:  Name of key field in struct
 * @val_field:  Name of value field in struct
 *
 * If entry exists: frees old value, sets new value
 * If entry doesn't exist: creates new node at bucket head
 *
 * Usage:
 *   HASH_INSERT(alias_tab, ALIAS_BUCKETS, name, value, alias, name, value);
 */
#define HASH_INSERT(table, buckets, name, val, type, key_field, val_field) \
  do { \
    type *_e = NULL; \
    HASH_LOOKUP(table, buckets, name, key_field, type, _e); \
    if (_e) { \
      free(_e->val_field); \
      _e->val_field = strdup(val); \
    } else { \
      _e = malloc(sizeof(type)); \
      _e->key_field = strdup(name); \
      _e->val_field = strdup(val); \
      unsigned int _i = hash(name, buckets); \
      _e->next = table[_i]; \
      table[_i] = _e; \
    } \
  } while (0)

/*
 * HASH_DELETE - Remove an entry from a hash table
 * @table:      The hash table array
 * @buckets:    Number of buckets
 * @name:       Key to delete
 * @type:       Struct type
 * @key_field:  Name of key field in struct
 * @val_field:  Name of value field in struct
 *
 * Usage:
 *   HASH_DELETE(alias_tab, ALIAS_BUCKETS, name, alias, name, value);
 */
#define HASH_DELETE(table, buckets, name, type, key_field, val_field) \
  do { \
    unsigned int _i = hash(name, buckets); \
    type **_prev = &table[_i]; \
    type *_e = table[_i]; \
    while (_e) { \
      if (strcmp(_e->key_field, name) == 0) { \
        *(_prev) = _e->next; \
        free(_e->key_field); \
        free(_e->val_field); \
        free(_e); \
        break; \
      } \
      _prev = &_e->next; \
      _e = _e->next; \
    } \
  } while (0)

// vim: set filetype=c:
#endif // !MALLOC_H
