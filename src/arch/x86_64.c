/*
 * Copyright (C) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#include <stdio.h>
#include <errno.h>
#include "bup/state.h"
#include "bup/mu.h"

/* Return size lookup table */
static const char *rettab[] = {
    [MSIZE_BYTE] = "al",
    [MSIZE_WORD] = "ax",
    [MSIZE_DWORD] = "eax",
    [MSIZE_QWORD] = "rax"
};

/* Define size lookup table */
static const char *dsztab[] = {
    [MSIZE_BYTE] = "db",
    [MSIZE_WORD] = "dw",
    [MSIZE_DWORD] = "dd",
    [MSIZE_QWORD] = "dq"
};

/* Program section lookup table */
static const char *sectab[] = {
    [SECTION_NONE] =    "none",
    [SECTION_TEXT] =    ".text",
    [SECTION_DATA] =    ".data",
    [SECTION_BSS]  =    ".bss"
};

/*
 * Ensure that the current section is of a specific type
 *
 * @state:   Compiler state
 * @section: Section to assert
 */
static inline void
cg_assert_section(struct bup_state *state, bin_section_t section)
{
    if (state == NULL || section >= SECTION_MAX) {
        return;
    }

    if (section != state->cur_section) {
        fprintf(
            state->out_fp,
            "[section %s]\n",
            sectab[section]
        );
    }
}

int
mu_cg_label(struct bup_state *state, const char *name, bool is_global)
{
    if (state == NULL || name == NULL) {
        errno = -EINVAL;
        return -1;
    }

    cg_assert_section(state, SECTION_TEXT);
    if (is_global) {
        fprintf(
            state->out_fp,
            "[global %s]\n",
            name
        );
    }

    fprintf(
        state->out_fp,
        "%s:\n",
        name
    );

    return 0;
}

int
mu_cg_ret(struct bup_state *state)
{
    if (state == NULL) {
        errno = -EINVAL;
        return -1;
    }

    fprintf(
        state->out_fp,
        "\tret\n"
    );

    return 0;
}

int
mu_cg_retimm(struct bup_state *state, msize_t size, ssize_t imm)
{
    if (state == NULL || size >= MSIZE_MAX) {
        errno = -EINVAL;
        return -1;
    }

    fprintf(
        state->out_fp,
        "\tmov %s, %zd\n"
        "\tret\n",
        rettab[size],
        imm
    );

    return 0;
}

int
mu_cg_inject(struct bup_state *state, char *line)
{
    if (state == NULL || line == NULL) {
        errno = -EINVAL;
        return -1;
    }

    fprintf(
        state->out_fp,
        "\t%s\n",
        line
    );

    return 0;
}

int
mu_cg_jmp(struct bup_state *state, char *label)
{
    if (state == NULL || label == NULL) {
        errno = -EINVAL;
        return -1;
    }

    fprintf(
        state->out_fp,
        "\tjmp %s\n",
        label
    );

    return 0;
}

int
mu_cg_globvar(struct bup_state *state, const char *name, msize_t size,
    bin_section_t sect, ssize_t imm, bool is_global)
{
    if (state == NULL || name == NULL) {
        errno = -EINVAL;
        return -1;
    }

    if (sect >= SECTION_MAX || size >= MSIZE_MAX) {
        errno = -EINVAL;
        return -1;
    }

    /* Put it in the section and global if we can */
    cg_assert_section(state, sect);
    if (is_global) {
        fprintf(
            state->out_fp,
            "[global %s]\n",
            name
        );
    }

    fprintf(
        state->out_fp,
        "%s: %s %zd\n",
        name,
        dsztab[size],
        imm
    );

    return 0;
}
