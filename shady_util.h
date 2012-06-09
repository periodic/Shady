#include "dr_api.h"

// Pointer helpers.
bool is_stack_address(dr_mcontext_t*, app_pc);
app_pc align_ptr(app_pc);

// Printing helpers.
#define char_buf_size 128
char* opnd_string(opnd_t);
void instr_print(void*, instr_t *);
bool instr_is_stack_op(instr_t *instr);
