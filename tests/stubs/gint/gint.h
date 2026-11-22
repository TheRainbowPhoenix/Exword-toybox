#ifndef TEST_GINT_H
#define TEST_GINT_H
/* Mirror gint/defs/call.h's limited indirect-call argument types.
   A custom struct pointer must be cast to void * before GINT_CALL.
   Keep executing the callback directly for host-side smoke tests. */
typedef union { void *pv; int i; } gint_call_arg_t;
#define GINT_CALL(fn, arg) ((void)(gint_call_arg_t)(arg), fn(arg))
#define gint_world_switch(rc) (rc)
#endif
