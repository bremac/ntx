/*===
cexcept.h 1.0.0 (2000-Jun-21-Wed)
Adam M. Costello <amc@cs.berkeley.edu>

An interface for exception-handling in ANSI C, developed jointly with
Cosmin Truta <cosmin@cs.toronto.edu>.

    Copyright (c) 2000 Adam M. Costello and Cosmin Truta.  Everyone
    is hereby granted permission to do whatever they like with this
    file, provided that if they modify it they take reasonable steps to
    avoid confusing or misleading people about the authors, version,
    and terms of use of the derived file.  The copyright holders make
    no guarantees about the correctness of this file, and are not
    responsible for any damage resulting from its use.

Modified (hacked) by Brendan MacDonell 05/11/2007 to provide simple
exceptions to single-threaded programs. For the original version, see

    http://cexcept.sourceforge.net

===*/

#ifndef CEXCEPT_H
#define CEXCEPT_H

#include <stdlib.h>
#include <setjmp.h>

enum EXCEPTION_TYPE {
  E_NONE = 0, E_NOMEM, E_BADFREE, E_INVAL, E_OVRFLO, E_USER
};

typedef struct {
  enum EXCEPTION_TYPE type;
  void *value;
} exception_t;

struct exception__state {
  exception_t *exception;
  volatile unsigned int resources;
  jmp_buf env;
  struct exception__state *next;
};

struct resource__state {
  void *res;
  void (*rel)(void *);
  struct resource__state *next;
};

struct exception_context {
  struct exception__state *last;
  struct resource__state *alloc;
  exception_t passthrough;
  int caught;
};

#define catch(e) exception__catch(&(e))

#define try \
  { \
    struct exception__state exception__s; \
    int exception__i; \
    exception__s.resources = 0; \
    exception__s.next = the_exception_context->last; \
    the_exception_context->last = &exception__s; \
    for (exception__i = 0; ; exception__i = 1) \
      if (exception__i) { \
        if (setjmp(exception__s.env) == 0) { \
          if (1)

#define exception__catch(e_addr) \
          else { } \
          the_exception_context->caught = 0; \
        } \
        else the_exception_context->caught = 1; \
        the_exception_context->last->exception->type = \
               the_exception_context->passthrough.type; \
        the_exception_context->last->exception->value = \
               the_exception_context->passthrough.value; \
        the_exception_context->last = exception__s.next; \
        break; \
      } \
      else exception__s.exception = e_addr; \
  } \
  if (!the_exception_context->caught) { } \
  else

/* The exception__state is declared volatile because the resources    */
/* member is incremented or decremented to track resource usage for   */
/* automatic resource management. Otherwise, modifying its value via  */
/* from another state means its value is undefined after a longjmp.   */
/* We use the passthrough for the same reason - to ensure             */
/* well-defined behaviour through the longjmp call.                   */
/*                                                                    */
/* Try ends with if(), and Catch begins and ends with else.  This     */
/* ensures that the Try/Catch syntax is really the same as the        */
/* if/else syntax.                                                    */
/*                                                                    */
/* We use &exception__s instead of 1 to appease compilers that        */
/* warn about constant expressions inside if().  Most compilers       */
/* should still recognize that &exception__s is never zero and avoid  */
/* generating test code.                                              */
/*                                                                    */
/* We use the variable exception__i to start the loop at the bottom,  */
/* rather than jump into the loop using a switch statement, to        */
/* appease compilers that warn about jumping into loops.              */

/* Alter this to suit your context model. */
extern struct exception_context the_exception_context[1];

void init_exception_context(struct exception_context *e);
void resource(void *r, void (*f)(void *));
void release(void *r);
void throw(enum EXCEPTION_TYPE t, void *value);

#endif /* CEXCEPT_H */
