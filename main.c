#include "uxn.h"
#include <stdio.h>
#include <time.h>
#include <unistd.h>

/*
Copyright (c) 2021 Devine Lu Linvega
Copyright (c) 2021 Maxime André

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

/*********** Config ***********/

#define DEBUG 0
#define SHOW_PROGRESSION 0
#define HALT_ON_FIRST_SOLUTION 0

/******************************/

#define LIT (0x80) /* Quick hack.... */

#define OPS                                                                    \
  X(BRK), X(INC), X(POP), X(DUP), X(NIP), X(SWP), X(OVR), X(ROT), X(EQU),      \
      X(NEQ), X(GTH), X(LTH), X(JMP), X(JCN), X(JSR), X(STH), X(LDZ), X(STZ),  \
      X(LDR), X(STR), X(LDA), X(STA), X(DEI), X(DEO), X(ADD), X(SUB), X(MUL),  \
      X(DIV), X(AND), X(ORA), X(EOR), X(SFT)

#define X(x) x
enum { OPS } ops;
#undef X

#define X(x) #x
static const char *op_names[] = {OPS};
#undef X

#define MODE_SHORT 0x20
#define MODE_RETURN 0x40
#define MODE_KEEP 0x80

void disassemble(Uint8 *program, Uint16 max_len) {
  int i;
  for (i = 0; i < max_len; i++) {
    Uint8 o = program[i];
    printf("%s", op_names[o & 0x1F]);
    if (o & MODE_SHORT)
      printf("2");
    if (o & MODE_KEEP)
      printf("k");
    if (o & MODE_RETURN)
      printf("r");
    printf(" ");
  }
}

static int error(char *msg, const char *err) {
  fprintf(stderr, "Error %s: %s\n", msg, err);
  return 0;
}

static void console_deo(Device *d, Uint8 port) {
  if (port == 0x1)
    d->vector = peek16(d->dat, 0x0);
  if (port > 0x7)
    write(port - 0x7, (char *)&d->dat[port], 1);
}

static Uint8 nil_dei(Device *d, Uint8 port) { return d->dat[port]; }

static void nil_deo(Device *d, Uint8 port) {
  if (port == 0x1)
    d->vector = peek16(d->dat, 0x0);
}

/* static const char *errors[] = {"underflow", "overflow", "division by zero"};
 */

int uxn_halt(Uxn *u, Uint8 error, char *name, int id) {
  /*fprintf(stderr, "Halted: %s %s#%04x, at 0x%04x\n", name, errors[error - 1],
   * id, u->ram.ptr);*/
  return 0;
}

static int
uxn_reset_full(Uxn *u, Uint16 max_length) /* max_length is here only to have the
                                             same prototype as uxn_reset_fast */
{
  if (!uxn_boot(u))
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

static void uxn_reset_fast(Uxn *u, Uint8 max_length) {
  u->wst.ptr = 0;
  u->wst.kptr = 0;
  u->wst.error = 0;
  u->rst.ptr = 0;
  u->rst.kptr = 0;
  u->rst.error = 0;
  u->dev[0].dat[0xf] = 0;
}

#define PUSH8(u, a)                                                            \
  { u->wst.dat[u->wst.ptr++] = a; }
#define POP8(u) (u->wst.dat[--u->wst.ptr])
#define COUNT8(u) (u->wst.ptr)
#define PUSH16(u, a)                                                           \
  {                                                                            \
    PUSH8(u, a >> 8);                                                          \
    PUSH8(u, a);                                                               \
  }
#define POP16(u) (POP8(u) + (POP8(u) << 8))
#define COUNT16(u) (COUNT8(u) / 2)
#define LOAD(u, p, l)                                                          \
  {                                                                            \
    int i;                                                                     \
    for (i = 0; i < l; i++)                                                    \
      u->ram.dat[PAGE_PROGRAM + i] = p[i];                                     \
  }
#define RST_IS_EMPTY(u) (u->rst.ptr == 0)

#if DEBUG == 1
#define DUMP_WST(u)                                                            \
  {                                                                            \
    int i;                                                                     \
    printf(" [ ");                                                             \
    for (i = 0; i < u->wst.ptr; i++)                                           \
      printf("%02x ", u->wst.dat[i]);                                          \
    printf(" ]");                                                              \
  }
#define DUMP_PROGRAM(u, l)                                                     \
  {                                                                            \
    int i;                                                                     \
    for (i = 0; i < max_length; i++)                                           \
      printf("%02x ", u->ram.dat[PAGE_PROGRAM + i]);                           \
  }
#define PRINT(m)                                                               \
  { printf("%s", m); }
#else
#define DUMP_WST(u)                                                            \
  {}
#define DUMP_PROGRAM(u, l)                                                     \
  {}
#define PRINT(m)                                                               \
  {}
#endif

#define UXN_RESET uxn_reset_fast

#define TESTS 11
static const Uint16 inputs[TESTS][2] = {
    {3, 10}, {8, 4},     {45, 53}, {136, 31}, {10, 250}, {14, 12},
    {0, 10}, {136, 138}, {10, 10}, {0, 0},    {255, 255}};

static Uint16 outputs[TESTS];

static void init_tests() {
  int i;
  for (i = 0; i < TESTS; i++) {
    outputs[i] = inputs[i][0] < inputs[i][1] ? inputs[i][0] : inputs[i][1];
    /*
    printf("[%d, %d] -> %d\n", inputs[i][0], inputs[i][1], outputs[i]);
    */
  } /*
    printf("\n");
   exit(0);
    */
}

static int check(Uxn *u, Uint8 *program, Uint16 max_length) {
  int i;

  for (i = 0; i < TESTS; i++) {

    UXN_RESET(u, max_length);
    LOAD(u, program, max_length);
    PUSH16(u, inputs[i][0]);
    PUSH16(u, inputs[i][1]);

    /* Debugging stuff */
    DUMP_PROGRAM(u, max_length);
#if DEBUG == 1
    disassemble(program, max_length);
#endif
    DUMP_WST(u);
    PRINT(" --> ");
    /*******************/

    uxn_eval(u, PAGE_PROGRAM);

    /* Debugging stuff */
    DUMP_WST(u);
    PRINT("\n");
    /*******************/

    if (COUNT16(u) != 1)
      return 0;
    if (POP16(u) != outputs[i])
      return 0;
    if (!RST_IS_EMPTY(u))
      return 0;
  }
  return 1;
}

static void inc(Uint8 *program, Uint16 max_length) {
  int i, j;
  program[0]++;
  for (i = 0; i < max_length; i++) {
    if (program[i] == 0)
      program[i + 1]++;
    else
      break;
#if SHOW_PROGRESSION == 1
    if (i == 2) {
      for (j = 0; j < max_length; j++)
        printf("%02x ", program[j]);
      printf("\n");
    }
#endif
  }
}

/* We skip programs that contain opcodes that are (maybe ?) not useful to
 * optimize little macros */
static int check_instructions(Uint8 *program, Uint16 max_length) {
  Uint8 op;
  int i;
  for (i = 0; i < max_length; i++) {
    if (program[i] == LIT) {
      i++;
      continue;
    }
    op = program[i] & 0x1f;
    if (op == DEI || op == DEO)
      return 0;
    if (op == LDZ || op == STZ || op == LDR || op == STR || op == LDA ||
        op == STA)
      return 0;
  }
  return 1;
}

static void bruteforce(Uxn *u, Uint16 max_length) {
  int i = 0;
  Uint8 program[max_length];
  for (i = 0; i < max_length; i++)
    program[i] = 0;
  while (1) {
    if (check(u, program, max_length)) {
      printf("\n\\o/\n");
      for (i = 0; i < max_length; i++)
        printf("%02x ", program[i]);
      printf("\n");
      disassemble(program, max_length);
      printf("\n");
#if HALT_ON_FIRST_SOLUTION == 1
      return;
#endif
    }

    if (program[max_length - 1] == 255)
      return;

    do {
      inc(program, max_length);
    } while (!check_instructions(program, max_length));
  }
}

int main(int argc, char **argv) {
  Uxn u;
  Uint16 max_length = 7;
  init_tests();
  uxn_reset_full(&u,
                 max_length); /* At least 1 full reset to initalize the devices
                                 (fast reset doesn't initialize them) */
  bruteforce(&u, max_length);
  return 0;
}
