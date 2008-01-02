#include <stdlib.h>
#include <stdio.h>
#include "except.h"

/* Modify this to suit your context model. Be sure to initialize it. */
struct exception_context the_exception_context[1] = {
  {NULL, NULL, {E_NONE, NULL}, 0}
};

void resource(void *r, void (*f)(void *))
{
  /* Add a managed resource to the current state. */
  struct resource__state *res;

#ifdef DEBUG
  for(; res && res->res != r; res = res->next);
  if(res) {
    fprintf(stderr, "%d is already stored in the resource heap!\n", (int)r);
    abort();
  }
#endif

  if(!(res = malloc(sizeof(struct resource__state))))
    throw(E_NOMEM, (void*)sizeof(struct resource__state));

  res->res  = r;
  res->rel  = f;
  res->next = the_exception_context->alloc;
  the_exception_context->alloc = res;

  /* Only increment it if we have a state to save to. */
  if(the_exception_context->last) the_exception_context->last->resources++;
}

void release_pop(void *r, unsigned int rel)
{
  /* Release a resource from the current lexical state. */
  struct resource__state *last = NULL, *res;
  struct exception__state *state = the_exception_context->last;
  unsigned int count;

  res = the_exception_context->alloc;
  if(state) {
    /* Search for the correct state. */
    for(; state; state = state->next) {
      for(count = state->resources;
          count > 0 && res && res->res != r;
          last = res, res = res->next) count--;
      if(!res || res->res == r) break;
    }
  }

  if(!state) {
    /* No current state; Search the entire global state.   *
     * This _can_ be reached if we can't locate the state. */
    for(; res && res->res != r; last = res, res = res->next);
  }

  if(!res) throw(E_BADFREE, r);
  if(last) last->next = res->next;
  else the_exception_context->alloc = res->next;
  if(state) state->resources--;
  if(rel) res->rel(res->res);
  free(res);
}

void throw(enum EXCEPTION_TYPE type, void *value)
{
  /* Release everything allocated since the last try block began. */
  struct resource__state *temp, *res;
  unsigned int count;

  if(!the_exception_context->last) {
    fputs("ERROR: Uncaught exception.\n", stderr);
    abort();
  }

  for(res = the_exception_context->alloc,
      count = the_exception_context->last->resources;
      count > 0 && res; count--) {
    if(value != res->res) res->rel(res->res);
    temp = res->next;
    free(res);
    res = temp;
  }

  the_exception_context->alloc = res;
  the_exception_context->passthrough.type  = type;
  the_exception_context->passthrough.value = value;
  longjmp(the_exception_context->last->env, 1);
}

/* Clean up _every_ known resource.                                   *
 * Not for use in application code unless you know what you're doing. */
void release_all()
{
  struct resource__state *temp, *res;

  for(res = the_exception_context->alloc; res; res = temp) {
    res->rel(res->res);
    temp = res->next;
    free(res);
  }
}
