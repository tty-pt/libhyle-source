#include "hyle-source/store.h"
#include "hyle-source/hyle_source.h"

/* In-memory store (volatile, no filesystem I/O) */
static int mem_scan(hyle_source_store_t *s, const hyle_source_def_t *d)
{
	(void)s;
	(void)d;
	return 0;
}

static int mem_load(hyle_source_store_t *s, const hyle_source_def_t *d,
                    const char *i, unsigned *r)
{
	(void)s;
	(void)d;
	(void)i;
	if (r)
		*r = 0;
	return 0;
}

static int mem_put(hyle_source_store_t *s, const hyle_source_def_t *d,
                   const char *i, unsigned h)
{
	(void)s;
	(void)d;
	(void)i;
	(void)h;
	return 0;
}

static int mem_put_field(hyle_source_store_t *s, const hyle_source_def_t *d,
                         const char *i, const char *f, const char *v)
{
	(void)s;
	(void)d;
	(void)i;
	(void)f;
	(void)v;
	return 0;
}

static int mem_del(hyle_source_store_t *s, const hyle_source_def_t *d,
                   const char *i)
{
	(void)s;
	(void)d;
	(void)i;
	return 0;
}

static const hyle_source_store_ops_t mem_ops = {
	.scan = mem_scan,
	.load = mem_load,
	.put = mem_put,
	.put_field = mem_put_field,
	.del = mem_del,
};

const hyle_source_store_ops_t *hyle_source_store_mem_ops(void) { return &mem_ops; }

hyle_source_store_t hyle_source_store_mem(void)
{
	hyle_source_store_t s = { &mem_ops, NULL };
	return s;
}
