#include <stdlib.h>
#include <stdio.h>
#include "except.h"

struct exception_context the_exception_context[1];

void init_exception_context(struct exception_context *e)
{
  e->last  = NULL;
  e->alloc  = NULL;
}

void e_register(void *r, void (*f)(void *))
{
  struct resource__state *res = malloc(sizeof(struct resource__state));

  if(!res) throw(E_NOMEM, (void*)sizeof(struct resource__state));
  if(!the_exception_context->last) {
    fputs("ERROR: e_register called without a context.\n", stderr);
    exit(EXIT_FAILURE);
  }

  res->res  = r;
  res->rel  = f;
  res->next = the_exception_context->alloc;
  the_exception_context->alloc = res;
}

void e_free(void *r)
{
  struct resource__state *last = NULL, *res = the_exception_context->alloc;

  for(; res && res->res != r; last = res, res = res->next) ;
  if(!res) throw(E_BADFREE, r);

  if(last) last->next = res->next;
  else the_exception_context->alloc = res->next;
  res->rel(res->res);
  free(res);
}

void throw(enum EXCEPTION_TYPE type, void *value)
{
  /* Release everything allocated since the last try block began. */
  struct resource__state *temp, *res = the_exception_context->alloc;

  if(!the_exception_context->last) {
    fputs("ERROR: Uncaught exception.\n", stderr);
    exit(EXIT_FAILURE);
  }

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
