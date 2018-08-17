#ifndef FLUX_RDL_H
#define FLUX_RDL_H

/*
 *  C API interface to Flux Resource Description Language.
 */

#include <stdlib.h>
#include <stdbool.h>
#include <jansson.h>

/*
 *  Forward declarations:
 */

/*
 *  struct rdl represents a handle to an in-memory copy of an RDL database:
 */
struct rdl;

/*
 *  struct resource represents a handle to a hierarchical resource
 *   within a given rdl db.
 */
struct resource;

/*
 *  Prototype for error processing function
 */
typedef void (*rdl_err_f) (void *ctx, const char *fmt, ...);


/*
 *  RDL Database functions:
 */

/*
 *  Set default rdllib error handling function to [fn] and context [ctx].
 */
void rdllib_set_default_errf (void *ctx, rdl_err_f fn);

/*
 *  Create a new rdl library handle
 */
struct rdllib * rdllib_open (void);

/*
 *  Close rdllib [l] and free all associated rdl handles
 */
void rdllib_close (struct rdllib *l);

/*
 *  Set rdllib error handling function to [fn] with context [ctx]
 *   to be passed to error function at each call.
 */
int rdllib_set_errf (struct rdllib *l, void *ctx, rdl_err_f fn);

/*
 *  Load an RDL db into library [l] from string [s] and return
 *   a new rdl handle.
 */
struct rdl * rdl_load (struct rdllib *l, const char *s);

/*
 *  Load an RDL db into library [l] from file [filename] and return
 *   a new rdl handle.
 */
struct rdl * rdl_loadfile (struct rdllib *l, const char *filename);

/*
 *  Copy an RDL handle
 */
struct rdl * rdl_copy (struct rdl *rdl);

/*
 *  Return a new RDL handle containing all resources that
 *   match expression in json_object [args].
 *
 *  JSON object supports the following keys, each of which are
 *   ANDed together
 *
 *  {
 *    'basename' : STRING,   - base name of object
 *    'name'     : NAMELIST, - match full name in NAMELIST (hostlist format)
 *    'ids'      : IDLIST,   - match resource "id" in idlist
 *    'type'     : STRING,   - match resource type name
 *    'tags'     : [ TAGS ]  - list of tags to match
 *  }
 */
struct rdl * rdl_find (struct rdl *rdl, json_t *args);

/*
 *  Destroy and deallocate an rdl handle
 */
void rdl_destroy (struct rdl *rdl);


/*
 *  Serialize an entire rdl db [rdl] and return a string. Caller
 *   is responsible for freeing memory from the string.
 */
char *rdl_serialize (struct rdl *rdl);


/*
 *  Get next hierarchy name from internal list of hierarchies in rdl.
 *   Start iteration with [last] == NULL, and pass previous result
 *   to get next value. Returns NULL after last hierarchy name is returned.
 */
const char *rdl_next_hierarchy (struct rdl *rdl, const char *last);


/*
 *   RDL Resource methods:
 */

/*
 *  Fetch a resource from the RDL db [rdl] at URI [uri], where
 *   [uri] is of the form "name[:path]", to fetch resource from
 *   optional path element [path] in hierarchy [name].
 *   (e.g. "default" or "default:/clusterA")
 */
struct resource * rdl_resource_get (struct rdl *rdl, const char *uri);

/*
 *  Free memory associated with resource object [r].
 */
void rdl_resource_destroy (struct resource *r);

/*
 *  Return the path to resource [r]
 */
const char *rdl_resource_path (struct resource *r);

/*
 *  Get the string representation of the basename for resource [r].
 */
const char *rdl_resource_basename (struct resource *r);

/*
 *  Get the string representation of the name for resource [r].
 */
const char *rdl_resource_name (struct resource *r);

/*
 *  Return resource 'size', 'available', and 'allocated' counts.
 */
size_t rdl_resource_size (struct resource *r);
size_t rdl_resource_available (struct resource *r);
size_t rdl_resource_allocated (struct resource *r);


/*
 *  Allocate/free [n] items from resource [r].
 */
int rdl_resource_alloc (struct resource *r, size_t n);
int rdl_resource_free (struct resource *r, size_t n);

/*
 *  Tag a resource with [tag] (tag only)
 */
void rdl_resource_tag (struct resource *r, const char *tag);

/*
 *  Set or get an arbitrary [tag] to an integer value [val]
 */
int rdl_resource_set_int (struct resource *r, const char *tag, int64_t val);
int rdl_resource_get_int (struct resource *r, const char *tag, int64_t *valp);

/*
 *  Remove a tag [tag] from resource object [r]
 */
void rdl_resource_delete_tag (struct resource *r, const char *tag);

/*
 *  Get representation of resource object [r] in json form
 *
 *  Format is a dictionary of name and values something like:
 *    { type: "string",
 *      basename: "string",
 *      name: "string",
 *      id:   number,
 *      properties: { list of key/value pairs },
 *      tags: {list of values},
 *    }
 */
json_t *rdl_resource_json (struct resource *r);

/*
 *  Aggregate all properties, tags, values and types from
 *   the resource hierarchy starting at [r]. Returns a json object
 *   reprenting the aggregation.
 */
json_t * rdl_resource_aggregate_json (struct resource *r);

/*
 *  Iterate over child resources in resource [r].
 */
struct resource * rdl_resource_next_child (struct resource *r);

/*
 *  Reset internal child iterator
 */
void rdl_resource_iterator_reset (struct resource *r);

/*
 *  Unlink a child with name [name] from this hierarchy at parent [r].
 */
int rdl_resource_unlink_child (struct resource *r, const char *name);

#endif /* !FLUX_RDL_H */
