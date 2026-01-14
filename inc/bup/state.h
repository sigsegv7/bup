/*
 * Copyright (c) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#ifndef BUP_STATE_H
#define BUP_STATE_H 1

#include <stdio.h>
#include "bup/tokbuf.h"
#include "bup/token.h"
#include "bup/ptrbox.h"
#include "bup/symbol.h"

#define DEFAULT_ASMOUT "bupgen.asm"
#define SCOPE_STACK_MAX 8

/*
 * Represents the compiler state
 *
 * @in_fd: Input file descriptor
 * @putback: Putback buffer
 * @tbuf:    Token buffer
 * @ptrbox:  Global pointer box
 * @symtab:  Global symbol table
 * @line_num: Current line number
 * @out_fp:   Output file pointer
 * @scope_stack: Used to keep track of scope
 * @scope_depth: How deep in scope we are
 */
struct bup_state {
    int in_fd;
    char putback;
    struct token_buf tbuf;
    struct ptrbox ptrbox;
    struct symbol_table symtab;
    size_t line_num;
    FILE *out_fp;
    tt_t scope_stack[SCOPE_STACK_MAX];
    uint8_t scope_depth;
};

/*
 * Initialize the compiler state
 *
 * @input_path: Path of input source file
 * @res: Result is written here
 *
 * Returns zero on success
 */
int bup_state_init(const char *input_path, struct bup_state *res);

/*
 * Destroy the compiler state
 *
 * @state: Compiler state
 */
void bup_state_destroy(struct bup_state *state);

#endif  /* !BUP_STATE_H */
