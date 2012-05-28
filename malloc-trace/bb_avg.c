#include "dr_api.h"

static int num_instructions;
static int num_bbs;

static void on_exit_fn() {
  int avg_size = num_instructions / num_bbs;
  dr_printf("I saw %d total instructions in %d bbs for a (truncated) "
            "average size of %d.\n", num_instructions, num_bbs, avg_size);
}

static dr_emit_flags_t on_bb_fn(void *drcontext, void *tag, instrlist_t
                                *bb, bool for_trace, bool translating) {
  instr_t *instr;
  num_bbs++;
  for (instr = instrlist_first(bb); instr != NULL;
       instr = instr_get_next(instr)) {
    num_instructions++;
  }
  return DR_EMIT_DEFAULT;
}


DR_EXPORT
void dr_init(client_id_t id) {
  dr_register_exit_event(on_exit_fn);
  dr_register_bb_event(on_bb_fn);
}
