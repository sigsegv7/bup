/*
 * Copyright (C) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#ifndef BUP_MU_H
#define BUP_MU_H 1

#include <stdbool.h>
#include "bup/state.h"
#include "bup/types.h"
#include "bup/symbol.h"

/*
 * Represents valid machine size types
 */
typedef enum {
    MSIZE_BAD,
    MSIZE_BYTE,
    MSIZE_WORD,
    MSIZE_DWORD,
    MSIZE_QWORD,
    MSIZE_MAX
} msize_t;

/*
 * Convert a program type into a machine size
 * type
 *
 * @type: Program type to convert
 */
static inline msize_t
type_to_msize(bup_type_t type)
{
    switch (type) {
    case BUP_TYPE_U8:   return MSIZE_BYTE;
    case BUP_TYPE_U16:  return MSIZE_WORD;
    case BUP_TYPE_U32:  return MSIZE_DWORD;
    case BUP_TYPE_U64:  return MSIZE_QWORD;
    default:            return MSIZE_BAD;
    }

    return MSIZE_BAD;
}

/*
 * Generate a label of a specific name
 *
 * @state:      Compiler state
 * @name:       Name of label to create
 * @is_global:  If true, label shall be global
 *
 * Returns zero on success
 */
int mu_cg_label(struct bup_state *state, const char *name, bool is_global);

/*
 * Create a global variable of a specific type
 *
 * @state: Compiler state
 * @name:  Label name
 * @size:  Label size
 * @sect:  Label section
 * @imm:   Value to initialize to
 * @is_global: If true, label should be global
 */
int mu_cg_globvar(
    struct bup_state *state, const char *name,
    msize_t size, bin_section_t sect, ssize_t imm, bool is_global
);

/*
 * Generate a 'ret'
 *
 * @state: Compiler state
 *
 * Returns zero on success
 */
int mu_cg_ret(struct bup_state *state);

/*
 * Generate a return with a return value register
 * filled
 *
 * @state: Compiler state
 * @size:  Machine size
 * @imm:     Value to return
 *
 * Returns zero on success
 */
int mu_cg_retimm(struct bup_state *state, msize_t size, ssize_t imm);

/*
 * Inject a line of assembly into the output source
 *
 * @state: Compiler state
 * @line:  Line of assembly to inject
 *
 * Returns zero on success
 */
int mu_cg_inject(struct bup_state *state, char *line);

/*
 * Generate a jump to a specific label
 *
 * @state: Comiler state
 * @label: Label to jump to
 *
 * Returns zero on success
 */
int mu_cg_jmp(struct bup_state *state, char *label);

/*
 * Generate a compare against an imm to test that it
 * is not zero
 *
 * @state: Compiler state
 * @label: Label to jump to if zero
 * @imm:   Immediate to compare
 *
 * Returns zero on success
 */
int mu_cg_icmpnz(struct bup_state *state, const char *label, ssize_t imm);

/*
 * Emit a call to a label
 *
 * @state: Compiler state
 * @label: Label to call
 *
 * Returns zero on success
 */
int mu_cg_call(struct bup_state *state, const char *label);

/*
 * Generate a structure from a struct symbol
 *
 * @state: Compiler state
 * @name: Instance name
 * @symbol: Structure symbol
 *
 * Returns zero on success
 */
int mu_cg_struct(struct bup_state *state, const char *name, struct symbol *symbol);

/*
 * Load an imm into a variable
 *
 * @state: Compiler state
 * @size:  Load size
 * @label: Label to load into
 * @imm:   Value to load
 *
 * Returs zero on success
 */
int mu_cg_istorevar(
    struct bup_state *state, msize_t size,
    const char *label, ssize_t imm
);

#endif  /* !BUP_MU_H */
