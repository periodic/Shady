#include "dr_api.h"
#include "drsyms.h"
#include "drwrap.h"
#include "hashtable.h"

static const int heap_pre_redzone_size = 8;
static const int heap_post_redzone_size = 16;
static const int sentinel_val = 0xdeadbeef;

static hashtable_t mallocd_ptrs[1];

static void exit_fn() {
  drsym_exit();
  drwrap_exit();
}

static void fill_sentinel(void *_a, int n) {
  int *a = (int*)_a;
  int i;
  for (i = 0; i < n; ++i) {
    a[i] = sentinel_val;
  }
}

static void before_malloc(void *wrapctx, OUT void **user_data) {
  void *arg = drwrap_get_arg(wrapctx, 0);
  int sz = (int)arg;
  dr_printf ("malloc called with size of %d\n", sz);
  sz += (sz % sizeof (int));
  dr_printf("rounded up to %d\n", sz);
  int new_sz = sz + heap_pre_redzone_size + heap_post_redzone_size;
  drwrap_set_arg(wrapctx, 0, (void*)new_sz);

  /* save original size request */
  *(int*)user_data = sz;
}

static void after_malloc(void *wrapctx, void *user_data) {
  void *ret = drwrap_get_retval(wrapctx);
  printf ("malloc returning with ptr %p\n", ret);
  if (ret == NULL) {
    /* TODO: we could try "saving" them here */
    return;
  }

  fill_sentinel(ret, heap_pre_redzone_size / sizeof (int));
  int orig_sz = (int)user_data;
  char *new_retval = (char*)ret + heap_pre_redzone_size;
  fill_sentinel(new_retval + orig_sz, heap_post_redzone_size / sizeof (int));

  drwrap_set_retval(wrapctx, new_retval);

  /* We save user base ptr / size */
  dr_printf ("adding %p to hashtable\n", new_retval);
  hashtable_add(mallocd_ptrs, new_retval, (void*)orig_sz);
}

static void before_calloc(void *wrapctx, OUT void **user_data) {
  void *n_arg = drwrap_get_arg(wrapctx, 0);
  void *sz_arg = drwrap_get_arg(wrapctx, 1);
  size_t n = (size_t)n_arg;
  size_t sz = (size_t)sz_arg;

  dr_printf("calloc called with args (%u, %u)\n", n, sz);
}

static void after_calloc(void *wrapctx, void *user_data) {
  // TODO
}

static void before_free(void *wrapctx, OUT void **user_data) {
  void *arg = drwrap_get_arg(wrapctx, 0);
  if (arg == NULL) {
    return; /* This is defined as a no-op */
  }
  dr_printf ("free called with %p\n", arg);

  void *lookup = hashtable_lookup(mallocd_ptrs, arg);
  if (lookup == NULL) {
    /* We "skip" free by setting arg to NULL */
    dr_printf("skipping\n");
    drwrap_set_arg(wrapctx, 0, NULL);
  } else {
    int orig_sz = (int)lookup;
    dr_printf("filling at %p of sz %d\n", arg, orig_sz);
    fill_sentinel(arg, orig_sz / sizeof(int)); /* cover user region with sentinel */
    char *real_base = (char*)arg - heap_pre_redzone_size;    
    dr_printf("setting free val to %p\n", real_base);
    drwrap_set_arg(wrapctx, 0, real_base);
    hashtable_remove(mallocd_ptrs, arg);
  }
}

static void before_realloc(void *wrapctx, OUT void **user_data) {
  void *ptr = drwrap_get_arg(wrapctx, 0);
  void *sz_arg = drwrap_get_arg(wrapctx, 1);
  int sz = (int)sz_arg;
  dr_printf("realloc called with (%p, %d)\n", ptr, sz);

  if (ptr == NULL && sz == 0) {
    // TODO:  Is this a no-op? Can we just return NULL?
    return;
  }
  if (ptr == NULL) {
    // TODO: this is really a malloc(sz)
    return;
  }
  if (sz == 0) {
    // TODO: this is really a free(ptr)
    return;
  }
  /* At this point we know this is a real realloc. We need to update
     args to handle redzones. */
  void *lookup = hashtable_lookup(mallocd_ptrs, ptr);
  if (lookup == NULL) {
    // TODO: what if we don't know about this ptr?
    return;
  } else {
    int real_sz = sz + heap_pre_redzone_size + heap_post_redzone_size;
    char *real_base = (char*)ptr - heap_pre_redzone_size;
    dwrap_set_arg(wrapctx, 0, real_base);
    drwrap_set_arg(wrapctx, 1, (void*)sz);
    *(int*)user_data = sz;
  }
}

static void after_realloc(void *wrapctx, void *user_data) {
  int sz = (int)user_data;
  if (sz < 0) {
    // TODO: we should use sz < 0 codes to signal that this is a weird
    // realloc 
  }
}

static void before_test_fn(void *wrapctx, OUT void **user_data) {
  dr_printf("test_fn CALLED\n");
  void *_arg = drwrap_get_arg(wrapctx, 0);
  int arg = (int)_arg;
  printf ("arg is %d\n", arg);
  drwrap_set_arg(wrapctx, 0, (void*)(arg + 1));
}

static void module_load_fn(void *drcontext, const module_data_t *mod,
                           bool loaded) {
  size_t modoffs;
  /* random stuff to try drsyms module */
  if (drsym_lookup_symbol(mod->full_path, "my_test_fn", &modoffs, 0)
      == DRSYM_SUCCESS) {
    app_pc addr = mod->start + modoffs;
    dr_printf ("Wrapping my_test_fn at %p\n", (void*)addr);
    drwrap_wrap(addr, before_test_fn, NULL);
  }

  app_pc malloc_pc = (app_pc)dr_get_proc_address(mod->start, "malloc");
  if (malloc_pc != NULL) {
    drwrap_wrap(malloc_pc, before_malloc, after_malloc);
  }

  app_pc calloc_pc = (app_pc)dr_get_proc_address(mod->start, "calloc");
  if (calloc_pc != NULL) {
    drwrap_wrap(calloc_pc, before_calloc, after_calloc);
  }

  app_pc free_pc = (app_pc)dr_get_proc_address(mod->start, "free");
  if (free_pc != NULL) {
    drwrap_wrap(free_pc, before_free, NULL);
  }

  app_pc realloc_pc = (app_pc)dr_get_proc_address(mod->start, "realloc");
  if (realloc_pc != NULL) {
    drwrap_wrap(realloc_pc, before_realloc, after_realloc);
  }
}

DR_EXPORT
void dr_init(client_id_t id) {
  int result = drwrap_init();
  drsym_init(0);
  dr_register_exit_event(exit_fn);
  dr_register_module_load_event(module_load_fn);

  hashtable_init_ex(mallocd_ptrs,
                    4, /* 16 buckets initially */
                    HASH_INTPTR, /* keys are ptrs */
                    0, /* don't duplicate string keys */
                    0, /* don't synchronize */
                    NULL, /* no free function (TODO) */
                    NULL, /* use default key hash fn */
                    NULL /* use default key cmp fn */
                    );
}
