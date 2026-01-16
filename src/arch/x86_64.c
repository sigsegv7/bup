/*
 * Copyright (C) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#include <stdio.h>
#include <errno.h>
#include "bup/state.h"
#include "bup/mu.h"
#include "bup/trace.h"

typedef int8_t reg_id_t;

#define regmask(id)     \
    (1 << (id))

#define out_of_regs(state)      \
    trace_error(                \
        (state),                \
        "out of registers!\n"   \
    )

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

/* Size table */
static const char *sztab[] = {
    [MSIZE_BYTE] = "byte",
    [MSIZE_WORD] = "word",
    [MSIZE_DWORD] = "dword",
    [MSIZE_QWORD] = "qword"
};

/* Program section lookup table */
static const char *sectab[] = {
    [SECTION_NONE] =    "none",
    [SECTION_TEXT] =    ".text",
    [SECTION_DATA] =    ".data",
    [SECTION_BSS]  =    ".bss"
};

/* General purpose register table */
static const char *gpregtab[] = {
    "r8", "r9",
    "r10", "r11",
    "r12", "r13",
    "r14", "r15",
};

/* Bitmap used to allocate registers */
static uint8_t gpreg_bitmap = 0;

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

        state->cur_section = section;
    }
}

/*
 * Convert a general purpose register ID to a
 * string name
 *
 * @id: ID to convert to name
 */
static inline const char *
cg_gpreg_name(uint8_t id)
{
    if (id >= 8) {
        return "bad";
    }

    return gpregtab[id];
}

/*
 * Free a mask of general purpose registers
 */
static inline void
cg_free_gpreg(uint8_t mask)
{
    gpreg_bitmap &= ~mask;
}

/*
 * Allocate a general purpose register and return
 * the ID on success, otherwise a less than zero
 * value on failure.
 */
static reg_id_t
cg_alloc_gpreg(void)
{
    for (uint8_t i = 0; i < 8; ++i) {
        if ((gpreg_bitmap & (1 << i)) == 0) {
            gpreg_bitmap |= (1 << i);
            return i;
        }
    }

    return -1;
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

int
mu_cg_istorevar(struct bup_state *state, msize_t size,
    const char *label, ssize_t imm)
{
    if (state == NULL || label == NULL) {
        errno = -EINVAL;
        return -1;
    }

    if (size >= MSIZE_MAX) {
        errno = -EINVAL;
        return -1;
    }

    fprintf(
        state->out_fp,
        "\tmov %s [rel %s], %zd\n",
        sztab[size],
        label,
        imm
    );

    return 0;
}

int
mu_cg_call(struct bup_state *state, const char *label)
{
    if (state == NULL || label == NULL) {
        errno = -EINVAL;
        return -1;
    }

    fprintf(
        state->out_fp,
        "\tcall %s\n",
        label
    );

    return 0;
}

int
mu_cg_icmpnz(struct bup_state *state, const char *label, ssize_t imm)
{
    reg_id_t reg;
    const char *name;

    if (state == NULL || label == NULL) {
        errno = -EINVAL;
        return -1;
    }

    if ((reg = cg_alloc_gpreg()) < 0) {
        out_of_regs(state);
        return -1;
    }

    name = cg_gpreg_name(reg);
    fprintf(
        state->out_fp,
        "\tmov %s, %zd\n"
        "\tor %s, %s\n"
        "\tjz %s\n",
        name, imm,
        name, name,
        label
    );

    cg_free_gpreg(regmask(reg));
    return 0;
}

int
mu_cg_struct(struct bup_state *state, const char *name, struct symbol *symbol)
{
    struct symbol *field;
    struct datum_type *dtype;
    msize_t size;

    if (state == NULL || symbol == NULL) {
        errno = -EINVAL;
        return -1;
    }

    FIELD_FOREACH(symbol, field) {
        dtype = &field->data_type;
        size = type_to_msize(dtype->type);
        if (size == MSIZE_BAD) {
            continue;
        }

        fprintf(
            state->out_fp,
            "%s.%s: %s 0\n",
            name,
            field->name,
            dsztab[size]
        );
    }

    return 0;
}
