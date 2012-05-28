#include "dr_api.h"
#include "drsyms.h"
#include "drwrap.h"

static int malloc_count;
static const int heap_pre_redzone_size = 8;
static const int heap_post_redzone_size = 16;
static const int sentinel_val = 0xdeadbeef;

static void exit_fn() {
  dr_printf("I saw %d calls to malloc.\n", malloc_count);
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
  printf ("malloc called with size of %d\n", sz);
  int new_sz = sz + heap_pre_redzone_size + heap_post_redzone_size;
  drwrap_set_arg(wrapctx, 0, (void*)new_sz);

  /* save original size request */
  *(int*)user_data = sz;
  malloc_count++;
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
}

static void before_free(void *wrapctx, OUT void **user_data) {
  void *arg = drwrap_get_arg(wrapctx, 0);
  if (arg == NULL) {
    return; /* This is defined as a no-op */
  }
  char *real_base = (char*)arg - heap_pre_redzone_size;
  drwrap_set_arg(wrapctx, 0, real_base);
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

  app_pc free_pc = (app_pc)dr_get_proc_address(mod->start, "free");
  if (free_pc != NULL) {
    drwrap_wrap(free_pc, before_free, NULL);
  }
}

DR_EXPORT
void dr_init(client_id_t id) {
  int result = drwrap_init();
  drsym_init(0);
  dr_register_exit_event(exit_fn);
  dr_register_module_load_event(module_load_fn);
}
