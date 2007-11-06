#include <stdlib.h>
#include <stdio.h>
#include "except.h"

struct exception_context the_exception_context[1];

void init_exception_context(struct exception_context *e)
{
  e->last  = NULL;
  e->alloc = NULL;
}

void resource(void *r, void (*f)(void *))
{
  /* Add a managed resource to the current state. */
  struct resource__state *res;

  if(!the_exception_context->last) {
    fputs("ERROR: resource called without a context.\n", stderr);
    exit(EXIT_FAILURE);
  }

  if(!(res = malloc(sizeof(struct resource__state))))
    throw(E_NOMEM, (void*)sizeof(struct resource__state));

  res->res  = r;
  res->rel  = f;
  res->next = the_exception_context->alloc;
  the_exception_context->alloc = res;
  the_exception_context->last->resources++;
}

void release(void *r)
{
  /* Release a resource from the current lexical state. */
  struct resource__state *last = NULL, *res;
  struct exception__state *state;
  unsigned int count;

  if(!the_exception_context->last) {
    fputs("ERROR: release called without a context.\n", stderr);
    exit(EXIT_FAILURE);
  }

  res = the_exception_context->alloc;
  for(state = the_exception_context->last; state; state = state->next) {
    for(count = state->resources; count > 0 && res && res->res != r;
        last = res, res = res->next) count--;
    if(!res) throw(E_BADFREE, r);
    if(res->res == r) break;
  }

  if(last) last->next = res->next;
  else the_exception_context->alloc = res->next;
  state->resources--; 
  res->rel(res->res);
  free(res);
}

void throw(enum EXCEPTION_TYPE type, void *value)
{
  /* Release everything allocated since the last try block began. */
  struct resource__state *temp, *res;
  unsigned int count;

  if(!the_exception_context->last) {
    fputs("ERROR: Uncaught exception.\n", stderr);
    exit(EXIT_FAILURE);
  }

  for(res = the_exception_context->alloc,
      count = the_exception_context->last->resources;
      count > 0 && res; count--) {
    res->rel(res->res);
    temp = res->next;
    free(res);
    res = temp;
  }

  the_exception_context->last->exception->type  = type;
  the_exception_context->last->exception->value = value;
  longjmp(the_exception_context->last->env, 1);
}
