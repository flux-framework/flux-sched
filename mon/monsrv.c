/* monsrv.c - monitoring plugin */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json.h>
#include <dlfcn.h>
#include <flux/core.h>

#include "src/common/libutil/log.h"
#include "src/common/libutil/shortjson.h"
#include "src/common/libutil/xzmalloc.h"



	static const char *mon_conf_dir = "conf.mon.source";
	const int red_timeout_msec = 2;



/*vvvvvvvvvv::CONTEXT::vvvvvvvvvv*/

	/*:: Context Struct ::*/
typedef struct {	/* Struct holds all allocated data for the modules */
	int epoch;
	flux_t h;
	bool master;
	int rank;
	char *rankstr;
	zhash_t *rcache;
	zhash_t *pcache;
	zlist_t *cache;
} ctx_t;

	/*:: Free Context ::*/
static void freectx (ctx_t *ctx)
{					/* Free's context and all of its allocations */
	zhash_destroy (&ctx->rcache);
	zhash_destroy (&ctx->pcache);
	zlist_destroy (&ctx->cache);
	free (ctx->rankstr);
	free (ctx);
}

	/*:: Get/Allocate Context ::*/
static ctx_t *getctx (flux_t h)
{					/* Either makes a context if one does not exist, or returns reference */
	ctx_t *ctx = (ctx_t *)flux_aux_get (h, "monsrv");

	if (!ctx) {
		ctx = xzmalloc (sizeof (*ctx));
		ctx->h = h;
		ctx->master = flux_treeroot (h);
		ctx->rank = flux_rank (h);
		if (asprintf (&ctx->rankstr, "%d", ctx->rank) < 0)
			oom (); 
		if (!(ctx->rcache = zhash_new ()))
			oom ();
		if (!(ctx->pcache = zhash_new ()))
			oom ();
		if (!(ctx->cache = zlist_new ()))
			oom ();
		flux_aux_set (h, "monsrv", ctx, (FluxFreeFn)freectx);
	}
	return ctx;
}

/*^^^^^^^^^^::CONTEXT::^^^^^^^^^^*/


/*vvvvvvvvvv::PLUGINS::vvvvvvvvvv*/

	/*:: Plugin Function Prototypes ::*/
typedef void (*plg_init)	(ctx_t *ctx);
typedef void (*plg_source)	(ctx_t  *ctx);
typedef void (*plg_sink)	(flux_t h, void *item, int batchnum, void *arg);
typedef void (*plg_reduce)	(flux_t h, zlist_t *items, int batchnum, void *arg);


	/*:: Plugin Struct ::*/
typedef struct {
					/* Plugin Context Data */
	int			epoch;				/* Plugin start-time */
	char		*plgstr;			/* Plugin name */
	
					/* Plugin Reduction Data */
	int			red_timeout_msec;	/* Plugin reduction timeout */
	zlist_t		*rcache;			/* Plugin reduction cache */
	
					/* Function pointers recieved via init function */
	plg_source source;
	plg_sink   sink;
	plg_reduce reduce;
} plg_t;


/*^^^^^^^^^^::PLUGINS::^^^^^^^^^^*/


/*vvvvvvvvvv::MODULE::vvvvvvvvvv*/

	/*:: Plugin Management Functions ::*/
static void load_plg (flux_h, const char *path)
{					/* Insert Plugin */
	JSON plg = Jnew ();
	char *key;
	int fd, len;
	uint8_t *buf;
	void *dso;

	if (asprintf (&key, "mon.plg.%s.so", name) < 0)
		oom ();
	if (kvs_get (h, key, &plg) == 0)
		errn_exit (EEXIST, "%s", key);
	Jadd_obj (plg, "args", args);
	if ((fd = open (path, O_RDONLY)) < 0)
		err_exit ("%s", path);
	if ((len = read_all (fd, &buf)) < 0)
		err_exit ("%s", path);
	(void)close (fd);
	util_json_object_add_data (plg, "data", buf, len);
	if (kvs_put (h, key, plg) < 0)
		err_exit ("kvs_put %s", key);

	if (kvs_commit (h) < 0)
		err_exit ("kvs_commit");

	free (key);
	free (buf);
	Jput (plg);
}

static void rm_plg	(flux_t h, void *plg_t)
{					//* Remove Plugin */
}

	/*:: Plugin Control Functions ::*/ 
static void init_plg	(flux_t h, void *plg_t)
{					//* Initialize Plugin */
}

static void sink_plg (flux_t h, void *item, int batchnum, void *arg)
{
    ctx_t *ctx = arg;
    JSON o = item;
    JSON oo, data, odata;
    const char *name;
    int epoch;
    char *key;

    if (!Jget_str (o, "name", &name) || !Jget_int (o, "epoch", &epoch)
                                     || !Jget_obj (o, "data", &data))
        return;
    if (ctx->master) {  // sink to the kvs
        if (asprintf (&key, "mon.%s.%d", name, epoch) < 0)
            oom ();
        if (kvs_get (h, key, &oo) == 0) {
            if (Jget_obj (oo, "data", &odata))
                Jmerge (data, odata);
            Jput (oo);
        }
        kvs_put (h, key, o);
        kvs_commit (h);
        free (key);
    } else {            // push upstream
        flux_request_send (h, o, "%s", "mon.push");
    }
    Jput (o);
}

static void reduce_plg (flux_t h, zlist_t *items, int batchnum, void *arg)
{
    zlist_t *tmp; 
    int e1, e2;
    JSON o1, o2, d1, d2;

    if (!(tmp = zlist_new ()))
        oom ();
    while ((o1 = zlist_pop (items))) {
        if (zlist_append (tmp, o1) < 0)
            oom ();
    }
    while ((o1 = zlist_pop (tmp))) {
        if (!Jget_int (o1, "epoch", &e1) || !Jget_obj (o1, "data", &d1)) {
            Jput (o1);
            continue;
        }
        o2 = zlist_first (items);
        while (o2) {
            if (Jget_int (o2, "epoch", &e2) && e1 == e2)
                break;
            o2 = zlist_next (items);
        }
        if (o2) {
            if (Jget_obj (o2, "data", &d2))
                Jmerge (d2, d1);
            Jput (o1);
        } else {
            if (zlist_append (items, o1) < 0)
                oom ();
        }
    }
}


	/*:: Callback Functions ::*/
static void ins_request_cb(flux_t h, int typemask, zmsg_t **zmsg, void *arg)
{					/* Insert Plugin */
	ctx_t *ctx = arg;
	JSON event = NULL;
	const char *path;
	void *dso;

	if (flux_msg_decode (*zmsg, NULL, &event) < 0 || event == NULL
				|| !Jget_int  (event, "epoch", &ctx->epoch)
				|| !Jget_str  (event, "path",  &path)) {
		flux_log (h, LOG_ERR, "%s: bad message", __FUNCTION__);
		goto done;
	}

	dlerror ();
	if (!(dso = dlopen (path, RTLD_NOW | RTLD_LOCAL))) {
		msg ("%s", dlerror ());
		errno = ENOENT;
		goto done;
	}

	/* May be referenced for later work 
	mod_main = dlsym (dso, "mod_main");
	*/

	const char **plg_namep = dlsym (dso, "plg_name"); //const char *plgname = "name"
	const char **plg_main = dlsym (dso, "plg_main"); //const char *plgname = "name"	

	
	
	if (!plg_main || !plg_namep || !*plg_namep) {
		err ("%s: mod_main or mod_name undefined", path);
		dlclose (dso);
		errno = ENOENT;
		goto done;
	}

	plg_t plugin = new plg_t;
	plugin->epoch = ctx->epoch;
	plugin->name = *plg_namep;
	plugin->rcache = zlist_new();

done:
	Jput (event);
}

static void rm_request_cb(flux_t h, int typemask, zmsg_t **zmsg, void *arg)
{					/* Remove Plugin */
    ctx_t *ctx = arg;
    JSON event = NULL;
    //int rc = 0;
    const char *path;

    if (flux_msg_decode (*zmsg, NULL, &event) < 0 || event == NULL
                || !Jget_int  (event, "epoch", &ctx->epoch)
                || !Jget_str  (event, "path",  &path)) {
        flux_log (h, LOG_ERR, "%s: bad message", __FUNCTION__);
        goto done;
    }

    zhash_destroy (&ctx->rcache);
    zhash_destroy (&ctx->pcache);
    zlist_destroy (&ctx->cache);
    free (ctx->rankstr);
    free (ctx);

done:
    Jput (event);
}

static int conf_cb (const char *path, kvsdir_t dir, void *arg, int errnum)
{
    ctx_t *ctx = arg;
    kvsitr_t itr;
    int entries = 1;

    if (errnum == 0) {
        if (!(itr = kvsitr_create (dir)))
            oom ();
        while (kvsitr_next (itr))
            entries++;
        kvsitr_destroy (itr);
    }
    if (entries > 0) {
        if (flux_event_subscribe (ctx->h, "hb") < 0) {
            flux_log (ctx->h, LOG_ERR, "flux_event_subscribe: %s",
                      strerror (errno));
        }
    } else {
        if (flux_event_unsubscribe (ctx->h, "hb") < 0) {
            flux_log (ctx->h, LOG_ERR, "flux_event_subscribe: %s",
                      strerror (errno));
        }
    }
    return 0;
}



/*^^^^^^^^^^::MODULE::^^^^^^^^^^*/











/*  Cut stuff...
static red_t rcache_lookup (ctx_t *ctx, const char *name)
{
    return zhash_lookup (ctx->rcache, name);
}

static red_t rcache_add (ctx_t *ctx, const char *name)
{
    flux_t h = ctx->h;
    red_t r = flux_red_create (h, mon_sink, ctx);

    flux_red_set_reduce_fn (r, mon_reduce);
    if (ctx->master) {
        flux_red_set_flags (r, FLUX_RED_TIMEDFLUSH);
        flux_red_set_timeout_msec (r, red_timeout_msec);
    } else {
        flux_red_set_flags (r, FLUX_RED_HWMFLUSH);
    }
    zhash_insert (ctx->rcache, name, r);
    zhash_freefn (ctx->rcache, name, (zhash_free_fn *)flux_red_destroy);
    return r;
}

static int ins_request_cb (flux_t h, int typemask, zmsg_t **zmsg, void *arg)
{
    ctx_t *ctx = arg;
    JSON event = NULL;
    int rc = 0;

    if (flux_msg_decode (*zmsg, NULL, &event) < 0 || event == NULL
                || !Jget_int  (event, "epoch", &ctx->epoch)) {
        flux_log (h, LOG_ERR, "%s: bad message", __FUNCTION__);
        goto done;
    }
    mon_plugin_ins(h, "NULL");
done:
    Jput (event);
    return rc;
}

static int rm_request_cb (flux_t h, int typemask, zmsg_t **zmsg, void *arg)
{
    ctx_t *ctx = arg;
    JSON event = NULL;
    int rc = 0;

    if (flux_msg_decode (*zmsg, NULL, &event) < 0 || event == NULL
                || !Jget_int  (event, "epoch", &ctx->epoch)) {
        flux_log (h, LOG_ERR, "%s: bad message", __FUNCTION__);
        goto done;
    }
    mon_plugin_rm(h, "NULL");
done:
    Jput (event);
    return rc;
}

static int push_request_cb (flux_t h, int typemask, zmsg_t **zmsg, void *arg)
{
    ctx_t *ctx = arg;
    JSON request = NULL;
    const char *name;
    red_t r;
    int epoch;
    int rc = 0;

    if (flux_msg_decode (*zmsg, NULL, &request) < 0 || request == NULL
            || !Jget_str (request, "name", &name)
            || !Jget_int (request, "epoch", &epoch)) {
        flux_log (ctx->h, LOG_ERR, "%s: bad message", __FUNCTION__);
        goto done;
    }
    if (!(r = rcache_lookup (ctx, name)))
        r = rcache_add (ctx, name);
    Jget (request);
    flux_red_append (r, request, epoch);
done:
    Jput (request);
    return rc;
}

static void mon_init (ctx_t *ctx, const char *name)
{
    red_t r;

    JSON data = Jnew ();
    JSON o = Jnew ();
    Jadd_int (o, "epoch", ctx->epoch);
    Jadd_str  (o, "name", name);
    Jadd_obj (o, "data", data);        
    Jadd_int (data, ctx->rankstr, ctx->rank);
    if (!(r = rcache_lookup (ctx, name)))
        r = rcache_add (ctx, name);
    flux_red_append (r, o, ctx->epoch);
}

static void poll_one (ctx_t *ctx, const char *name, const char *tag)
{
    red_t r;
    JSON res;

    if ((res = flux_rpc (ctx->h, NULL, "%s", tag))) {
        JSON data = Jnew ();
        JSON o = Jnew ();
        Jadd_int (o, "epoch", ctx->epoch);
        Jadd_str  (o, "name", name);
        Jadd_obj (o, "data", data);        
        Jadd_obj (data, ctx->rankstr, res);
        Jput (res);
        if (!(r = rcache_lookup (ctx, name)))
            r = rcache_add (ctx, name);
        flux_red_append (r, o, ctx->epoch);
    }
}

static void poll_all (ctx_t *ctx)
{
    kvsdir_t dir = NULL;
    kvsitr_t itr = NULL;
    const char *name, *tag;

    if (kvs_get_dir (ctx->h, &dir, "%s", mon_conf_dir) < 0)
        goto done;
    if (!(itr = kvsitr_create (dir)))
        oom ();
    while ((name = kvsitr_next (itr))) {
        JSON ent = NULL;
        if (kvsdir_get (dir, name, &ent) == 0 && Jget_str (ent, "tag", &tag)) {
            poll_one (ctx, name, tag);
        }
        Jput (ent);
    }
done:
    if (itr)
        kvsitr_destroy (itr);
    if (dir)
        kvsdir_destroy (dir);
}

static int hb_cb (flux_t h, int typemask, zmsg_t **zmsg, void *arg)
{
    mon_source(arg);
    ctx_t *ctx = arg;
    JSON event = NULL;
    int rc = 0;

    mon_init(ctx, "Init");

    if (flux_msg_decode (*zmsg, NULL, &event) < 0 || event == NULL
                || !Jget_int  (event, "epoch", &ctx->epoch)) {
        flux_log (h, LOG_ERR, "%s: bad message", __FUNCTION__);
        goto done;
    }
    poll_all (ctx);
done:
    Jput (event);
    return rc;
}


static int hb_cb (flux_t h, int typemask, zmsg_t **zmsg, void *arg)
{
    mon_source(arg);
    ctx_t *ctx = arg;
    JSON event = NULL;
    int rc = 0;

    mon_init(ctx, "Init");

    if (flux_msg_decode (*zmsg, NULL, &event) < 0 || event == NULL
                || !Jget_int  (event, "epoch", &ctx->epoch)) {
        flux_log (h, LOG_ERR, "%s: bad message", __FUNCTION__);
        goto done;
    }
    poll_alfluxl (ctx);
done:
    Jput (event);
    return rc;
}

/* Detect the presence (or absence) of content in our conf KVS space.
 * We will ignore hb events to reduce overhead if there is no content.
 */



////////////////////////////////////////////////////////////////////
//------------------------------TEST------------------------------//


static int write_all (int fd, uint8_t *buf, int len)
{
    int n, count = 0;
    while (count < len) {
        if ((n = write (fd, buf + count, len - count)) < 0)
            return n;
        count += n;
    }
    return count;
}

static int ins_plg (ctx_t *ctx, const char *name)
{
    char *key = NULL;
    JSON plg = NULL, args;
    uint8_t *buf = NULL;
    int fd = -1, len;
    char tmpfile[] = "/tmp/flux-mon-XXXXXX"; /* FIXME: consider TMPDIR */
    int n, rc = -1;
    int errnum = 0;

    if (asprintf (&key, "mon.plg.%s.so", name) < 0)
        oom ();
    if (kvs_get (ctx->h, key, &plg) < 0 || !Jget_obj (plg, "args", &args)
            || util_json_object_get_data (plg, "data", &buf, &len) < 0) {
        errnum = EPROTO;
        goto done; /* kvs/parse error */
    }
    if ((fd = mkstemp (tmpfile)) < 0) {
        errnum = errno;
        goto done;
    }
    if (write_all (fd, buf, len) < 0) {
        errnum = errno;
        goto done;
    }
    n = close (fd);
    fd = -1;
    if (n < 0) {
        errnum = errno;
        goto done;
    }
    if (flux_insmod (ctx->h, -1, tmpfile, FLUX_MON_FLAGS_MANAGED, args) < 0) {
        errnum = errno;
        goto done_unlink;
    }
    rc = 0;
done_unlink:
    (void)unlink (tmpfile);
done:
    if (fd != -1)
        (void)close (fd);
    if (key)
        free (key);
    if (buf)
        free (buf);
    Jput (plg);
    if (errnum != 0)
        errno = errnum;
    return rc;
}

static void sink_plg (flux_t h, void *item, int batchnum, void *arg)
{
    ctx_t *ctx = arg;
    JSON o = item;
    JSON oo, data, odata;
    const char *name;
    int epoch;
    char *key;

    if (!Jget_str (o, "name", &name) || !Jget_int (o, "epoch", &epoch)
                                     || !Jget_obj (o, "data", &data))
        return;
    if (ctx->master) {  // sink to the kvs
        if (asprintf (&key, "mon.%s.%d", name, epoch) < 0)
            oom ();
        if (kvs_get (h, key, &oo) == 0) {
            if (Jget_obj (oo, "data", &odata))
                Jmerge (data, odata);
            Jput (oo);
        }
        kvs_put (h, key, o);
        kvs_commit (h);
        free (key);
    } else {            // push upstream
        flux_request_send (h, o, "%s", "mon.push");
    }
    Jput (o);
}

static int push_request_cb (flux_t h, int typemask, zmsg_t **zmsg, void *arg)
{
    ctx_t *ctx = arg;
    JSON request = NULL;
    const char *name;
    int epoch;
    int rc = 0;

    if (flux_msg_decode (*zmsg, NULL, &request) < 0 || request == NULL
            || !Jget_str (request, "name", &name)
            || !Jget_int (request, "epoch", &epoch)) {
        flux_log (ctx->h, LOG_ERR, "%s: bad message", __FUNCTION__);
        goto done;
    }
    //if (!(r = rcache_lookup (ctx, name)))
    //    r = rcache_add (ctx, name);
    //Jget (request);
    //flux_red_append (r, request, epoch);
    mon_sink(h,request,ctx->epoch,ctx);
done:
    Jput (request);
    return rc;

}

/*
static void mon_source (flux_t h, void *item, int batchnum, void *arg)
{
    ctx_t *ctx = arg;
    JSON o = item;
    JSON oo, data, odata;
    const char *name;
    int epoch;
    char *key;

    if (!Jget_str (o, "name", &name) || !Jget_int (o, "epoch", &epoch)
                                     || !Jget_obj (o, "data", &data))
        return;
    if (ctx->master) {  // sink to the kvs
        if (asprintf (&key, "mon.%s.%d", name, epoch) < 0)
            oom ();
        if (kvs_get (h, key, &oo) == 0) {
            if (Jget_obj (oo, "data", &odata))
                Jmerge (data, odata);
            Jput (oo);
        }
        kvs_put (h, key, o);
        kvs_commit (h);
        free (key);
    } else {            // push upstream
        flux_request_send (h, o, "%s", "mon.push");
    }
    Jput (o);
}
*/

static int live_request_cb(flux_t h, int typemask, zmsg_t **zmsg, void *arg)
{
    ctx_t *ctx = arg;
    JSON request = NULL;
 //   const char *name;
 //   int epoch;
    int rc = 0;

    if  (  flux_msg_decode (*zmsg, NULL, &request) < 0
        || request == NULL
        )
    {
        goto done;
    }
    //if (!ctx->master)
    //       rc = flux_request_send(h,request,"mon.live");

    zlist_append(ctx->cache, request);

/*            || !Jget_str (request, "name", &name)
            || !Jget_int (request, "epoch", &epoch)) {
        flux_log (ctx->h, LOG_ERR, "%s: bad message", __FUNCTION__);
        goto done;
    }
    //if (!(r = rcache_lookup (ctx, name)))
    //    r = rcache_add (ctx, name);
    //Jget (request);
    //flux_red_append (r, request, epoch);
    mon_sink(h,request,ctx->epoch,ctx);
*/

done:
    //Jput (request);
    zmsg_destroy (zmsg);
    return rc;
}

static int hb_cb (flux_t h, int typemask, zmsg_t **zmsg, void *arg)
{
    //mon_source(arg);
    ctx_t *ctx = arg;
    JSON event = NULL;
    int rc = 0;



    if (flux_msg_decode (*zmsg, NULL, &event) < 0 || event == NULL
                || !Jget_int  (event, "epoch", &ctx->epoch)) {
        flux_log (h, LOG_ERR, "%s: bad message", __FUNCTION__);
        goto done;
    }
    flux_log (h, LOG_DEBUG, "hb rank: %i", ctx->rank);
    //if ((res = flux_rpc (ctx->h, NULL, "%s", tag))) {
        //JSON data = Jnew ();
        JSON o = Jnew ();
        Jadd_str  (o, "rank", ctx->rankstr);
        Jadd_int (o, "epoch", ctx->epoch);
        
        JSON data = Jnew ();
        if (!ctx->master)
        {
        while (zlist_size(ctx->cache) > 0)
        {
            //zlist_pop(ctx->cache);
            
            //zlist_remove(ctx->cache,zlist_first(ctx->cache));
            //const char *rankstr;
            JSON temp = zlist_pop(ctx->cache);
            //if (temp != NULL)
            //    Jput(temp);
            //Jget_str(temp,"rankstr",&rankstr);
            Jadd_obj(data,"rankstr",temp);
            //Jput(temp);
        }
        }
        else
        {
            while (zlist_size(ctx->cache) > 0)
                zlist_remove(ctx->cache,zlist_first(ctx->cache));
        }
        //Jadd_obj(data,"temp",temp);
        //Jput(temp);
        Jadd_int(data,"size",zlist_size(ctx->cache));
        Jadd_obj (o, "data", data);

        //Jadd_obj (data, ctx->rankstr, res);
        //if (!(r = zhash_lookup (ctx->rcache, name)))
        //    r = rcache_add (ctx, name);
        //flux_red_append (r, o, ctx->epoch);
        if (!ctx->master)
            rc = flux_request_send(h,o,"mon.live");
        //mon_sink(h,o,ctx->epoch,NULL);
        Jput(data);
        Jput(o);
    //}
done:
    Jput (event);
    return rc;
}

static msghandler_t htab[] = {
    { FLUX_MSGTYPE_EVENT,   "hb",             hb_cb },

    { FLUX_MSGTYPE_EVENT,   "mon.ins",        ins_cb },
    { FLUX_MSGTYPE_EVENT,   "mon.rm",         rm_cb },

    { FLUX_MSGTYPE_REQUEST, "mon.live",       live_request_cb },
    { FLUX_MSGTYPE_REQUEST, "mon.push",       push_request_cb },
    
    { FLUX_MSGTYPE_REQUEST, "mon.source",     mon_plg_source },
    { FLUX_MSGTYPE_REQUEST, "mon.reduce",     mon_plg_reduce },
    { FLUX_MSGTYPE_REQUEST, "mon.sink",       mon_plg_sink },
};
const int htablen = sizeof (htab) / sizeof (htab[0]);

int mod_main (flux_t h, zhash_t *args)
{
    ctx_t *ctx = getctx (h);
/* Configure module code
    if (kvs_watch_dir (h, conf_cb, ctx, mon_conf_dir) < 0) {
        flux_log (ctx->h, LOG_ERR, "kvs_watch_dir: %s", strerror (errno));
        return -1;
    }
*/
    if (flux_event_subscribe (h, "hb") < 0) {
        flux_log (h, LOG_ERR, "flux_event_subscribe: %s", strerror (errno));
        return -1;
    }
    if (flux_event_subscribe (h, "mon.ins") < 0) {
        flux_log (h, LOG_ERR, "flux_event_subscribe: %s", strerror (errno));
        return -1;
    }
    if (flux_event_subscribe (h, "mon.rm") < 0) {
        flux_log (h, LOG_ERR, "flux_event_subscribe: %s", strerror (errno));
        return -1;
    }

    if (flux_msghandler_addvec (h, htab, htablen, ctx) < 0) {
        flux_log (h, LOG_ERR, "flux_msghandler_add: %s", strerror (errno));
        return -1;
    }

    if (flux_reactor_start (h) < 0) {
        flux_log (h, LOG_ERR, "flux_reactor_start: %s", strerror (errno));
        return -1;
    }

    return 0;
}

MOD_NAME ("mon");

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
