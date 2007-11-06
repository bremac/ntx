#include <stdlib.h>
#include <stdio.h>
#include "except.h"

struct exception_context the_exception_context[1];

void init_exception_context(struct exception_context *e)
{
  e->last  = NULL;
}

void resource(void *r, void (*f)(void *))
{
  struct resource__state *res;

  if(!the_exception_context->last) {
    fputs("ERROR: resource called without a context.\n", stderr);
    exit(EXIT_FAILURE);
  }

  if(!(res = malloc(sizeof(struct resource__state))))
    throw(E_NOMEM, (void*)sizeof(struct resource__state));

  res->res  = r;
  res->rel  = f;
  res->next = the_exception_context->last->alloc;
  the_exception_context->last->alloc = res;
}

void release(void *r)
{
  /* Release a resource from the _current_ (not lexical) state. */
  struct resource__state *last = NULL, *res;

  if(!the_exception_context->last) {
    fputs("ERROR: release called without a context.\n", stderr);
    exit(EXIT_FAILURE);
  }

  for(res = the_exception_context->last->alloc;
      res && res->res != r; last = res, res = res->next) ;
  if(!res) throw(E_BADFREE, r);

  if(last) last->next = res->next;
  else the_exception_context->last->alloc = res->next;
  res->rel(res->res);
  free(res);
}

void throw(enum EXCEPTION_TYPE type, void *value)
{
  /* Release everything allocated since the last try block began. */
  struct resource__state *temp, *res;

  if(!the_exception_context->last) {
    fputs("ERROR: Uncaught exception.\n", stderr);
    exit(EXIT_FAILURE);
  }

  res = the_exception_context->last->alloc;
  while(res) {
    res->rel(res->res);
    temp = res->next;
    free(res);
    res = temp;
  }

  the_exception_context->last->exception->type  = type;
  the_exception_context->last->exception->value = value;
  longjmp(the_exception_context->last->env, 1);
}
