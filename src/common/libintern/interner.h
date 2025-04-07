#ifndef INTERNER_C_LIBRARY_H
#define INTERNER_C_LIBRARY_H

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif
typedef struct {
    size_t id;
} interner_t;

typedef struct {
    const char *str;
    size_t len;
} intern_string_view_t;

typedef struct {
    // Details of this are considered private and prone to change, do not touch
    interner_t group_id;
    uint64_t id;
} intern_string_t;

/// get an ID for a new intern group, strings within a group will be deduplicated, across groups
/// they may not be
interner_t interner_new ();

/// Get an interned string identifier from group id and a char * and length, the string need not be
/// null terminated
intern_string_t dense_interner_get_str (interner_t group_id, intern_string_view_t s);

/// get the hash of this interned string
size_t intern_str_hash (intern_string_t s);

/// get a view of the string with both string and length
intern_string_view_t intern_str_view (intern_string_t s);

/// get the null-terminated string (not recommended, use lengths)
const char *intern_str_cstr (intern_string_t s);

/// comparator function that orders by ID
int intern_str_cmp_by_id (const void *l, const void *r);

/// comparator function that orders by string value
int intern_str_cmp_by_str (const void *l, const void *r);

#if defined(__cplusplus)
}
#endif
#endif  // INTERNER_C_LIBRARY_H
