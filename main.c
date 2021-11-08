#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "uxn.h"

/*
Copyright (c) 2021 Devine Lu Linvega

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/


static int
error(char *msg, const char *err)
{
	fprintf(stderr, "Error %s: %s\n", msg, err);
	return 0;
}

static void
console_deo(Device *d, Uint8 port)
{
	if(port == 0x1)
		d->vector = peek16(d->dat, 0x0);
	if(port > 0x7)
		write(port - 0x7, (char *)&d->dat[port], 1);
}


static Uint8
nil_dei(Device *d, Uint8 port)
{
	return d->dat[port];
}

static void
nil_deo(Device *d, Uint8 port)
{
	if(port == 0x1) d->vector = peek16(d->dat, 0x0);
}

/* static const char *errors[] = {"underflow", "overflow", "division by zero"}; */

int
uxn_halt(Uxn *u, Uint8 error, char *name, int id)
{
  /*fprintf(stderr, "Halted: %s %s#%04x, at 0x%04x\n", name, errors[error - 1], id, u->ram.ptr);*/
	return 0;
}

static int
uxn_reset_full(Uxn *u, Uint16 max_length)    /* max_length is here only to have the same prototype as uxn_reset_fast */
{
  if(!uxn_boot(u))
    return error("Boot", "Failed");
  
  /* system   */ uxn_port(u, 0x0, nil_dei, nil_deo);
  /* console  */ uxn_port(u, 0x1, nil_dei, console_deo);
  /* empty    */ uxn_port(u, 0x2, nil_dei, nil_deo);
  /* empty    */ uxn_port(u, 0x3, nil_dei, nil_deo);
  /* empty    */ uxn_port(u, 0x4, nil_dei, nil_deo);
  /* empty    */ uxn_port(u, 0x5, nil_dei, nil_deo);
  /* empty    */ uxn_port(u, 0x6, nil_dei, nil_deo);
  /* empty    */ uxn_port(u, 0x7, nil_dei, nil_deo);
  /* empty    */ uxn_port(u, 0x8, nil_dei, nil_deo);
  /* empty    */ uxn_port(u, 0x9, nil_dei, nil_deo);
  /* empty    */ uxn_port(u, 0xa, nil_dei, nil_deo);
  /* empty    */ uxn_port(u, 0xb, nil_dei, nil_deo);
  /* empty    */ uxn_port(u, 0xc, nil_dei, nil_deo);
  /* empty    */ uxn_port(u, 0xd, nil_dei, nil_deo);
  /* empty    */ uxn_port(u, 0xe, nil_dei, nil_deo);
  /* empty    */ uxn_port(u, 0xf, nil_dei, nil_deo);
  return 1;
}

static void
uxn_reset_fast(Uxn *u, Uint8 max_length)
{
  int i;
  u->wst.ptr = 0;
  u->wst.kptr = 0;
  u->wst.error = 0;
  u->rst.ptr = 0;
  u->rst.kptr = 0;
  u->rst.error = 0;
  u->dev[0].dat[0xf] = 0;
}

#define PUSH8(u, a) { u->wst.dat[u->wst.ptr++] = a; }
#define POP8(u) ( u->wst.dat[--u->wst.ptr] )
#define COUNT8(u) ( u->wst.ptr )
#define PUSH16(u, a) { PUSH8(u, a >> 8); PUSH8(u, a); }
#define POP16(u) ( POP8(u) + (POP8(u) << 8) )
#define COUNT16(u) ( COUNT8(u) / 2 )
#define LOAD(u, p, l) { int i; for(i = 0; i < l; i++) u->ram.dat[PAGE_PROGRAM+i] = p[i]; }
#define RST_IS_EMPTY(u) ( u->rst.ptr == 0 )

#define DEBUG
#ifdef DEBUG
#define DUMP_WST(u) { int i; printf(" [ "); for(i = 0; i < u->wst.ptr; i++) printf("%02x ", u->wst.dat[i]); printf(" ]"); }
#define DUMP_PROGRAM(u, l) { int i; for(i = 0; i < max_length; i++) printf("%02x ", u->ram.dat[PAGE_PROGRAM+i]); }
#define PRINT(m) { printf("%s", m); }
#else
#define DUMP_WST(u) { }
#define DUMP_PROGRAM(u, l) { }
#define PRINT(m) { }
#endif

#define UXN_RESET uxn_reset_fast

static int
check(Uxn *u, Uint8 *program, Uint16 max_length)
{
  #define TESTS 4
  const Uint8 inputs[TESTS][2] = {
    {10, 3},
    {8, 4},
    {53, 45},
    {136, 31}};
  const Uint8 outputs[TESTS] = {
    10%3,
    8%4,
    53%45,
    136%31
  };
  int i;

  for(i = 0; i < TESTS; i++) {
  
    UXN_RESET(u, max_length);
    LOAD(u, program, max_length);
    PUSH8(u, inputs[i][0]);
    PUSH8(u, inputs[i][1]);

  
    DUMP_PROGRAM(u, max_length);
    DUMP_WST(u);
    PRINT(" --> ");
    
    uxn_eval(u, PAGE_PROGRAM);
    
    DUMP_WST(u);
    PRINT("\n");
    
    
    if(COUNT8(u) != 1)
      return 0;
    if(POP8(u) != outputs[i])
      return 0;
    if(!RST_IS_EMPTY(u))
      return 0;
  }
  return 1;
}

static void
inc(Uint8 *program, Uint16 max_length)
{
  int i, j;
  program[0]++;
  for(i = 0; i < max_length; i++) {
    if(program[i] == 0)
      program[i+1]++;
    else
      break;
    if(i == 2) {
      for(j = 0; j < max_length; j++)
	printf("%02x ", program[j]);
      printf("\n");
    }
  }
}

#define LIT (0x80)
#define JMP (0x0c)
#define JCN (0x0d)
#define JSR (0x0e)

static int
check_program(Uint8 *program, Uint16 max_length)
{
  Uint8 op;
  int i;
  for(i = 0; i < max_length; i++) {
    if(program[i] == LIT)
      i++;
    op = program[i] & 0x1f;
    if(op >= JMP && op <= JSR)
      return 0;
  }
  return 1;
}

static void
bruteforce(Uxn *u, Uint16 max_length)
{
  int i = 0;
  Uint8 program[max_length];
  for(i = 0; i < max_length; i++)
    program[i] = 0;
  while(1) {
    if(check(u, program, max_length)) {
      printf("\\o/\n");
      for(i = 0; i < max_length; i++)
	printf("%02x ", program[i]);
      printf("\n");
      return;
    }

    if(u->ram.dat[0x100] == 0x9b && u->ram.dat[0x101] == 0x1a && u->ram.dat[0x102] == 0x19) {
      printf("Soluce :\n");
      printf("wst ptr = #%02x\n", u->wst.ptr);
      for(i = 0; i < u->wst.ptr; i++)
	printf("#%02x ", u->wst.dat[i]);
      printf("\n");
      return;
      
    }

    /*for(i = 0; i < max_length; i++)
      printf("%02x ", program[i]);
      printf("\n");*/
    
    if(program[max_length - 1] == 255)
      return;

    do {
      inc(program, max_length);
    } while(!check_program(program, max_length));
  } 
}



int
main(int argc, char **argv)
{
	Uxn u;
	Uint16 max_length = 4;
	uxn_reset_full(&u, max_length);  /* At 1 full reset to initalize the devices (fast reset doesn't initialize them) */
	bruteforce(&u, max_length);
	return 0; 

}
