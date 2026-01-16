/*
 * Copyright (c) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "bup/symbol.h"

static void
symbol_fields_destroy(struct symbol *symbol)
{
    struct symbol *iter;

    if (symbol == NULL) {
        return;
    }

    while (!TAILQ_EMPTY(&symbol->fields)) {
        iter = TAILQ_FIRST(&symbol->fields);
        TAILQ_REMOVE(&symbol->fields, iter, field_link);
        free(iter->name);
        free(iter);
    }
}

int
symbol_table_init(struct symbol_table *symtab)
{
    if (symtab == NULL) {
        errno = -EINVAL;
        return -1;
    }

    symtab->symbol_count = 0;
    TAILQ_INIT(&symtab->symbols);
    return 0;
}

int
symbol_new(struct symbol_table *symtab, const char *name, bup_type_t type,
    struct symbol **res)
{
    struct symbol *symbol;
    struct datum_type *dtype;

    if (symtab == NULL || name == NULL) {
        errno = -EINVAL;
        return -1;
    }

    symbol = malloc(sizeof(*symbol));
    if (symbol == NULL) {
        errno = -ENOMEM;
        return -1;
    }

    /* Initialize the symbol */
    memset(symbol, 0, sizeof(*symbol));
    symbol->id = symtab->symbol_count++;
    symbol->name = strdup(name);

    /* Initialize the datam type */
    dtype = &symbol->data_type;
    dtype->type = type;

    if (res != NULL) {
        *res = symbol;
    }

    TAILQ_INIT(&symbol->fields);
    TAILQ_INSERT_TAIL(&symtab->symbols, symbol, link);
    return 0;
}

struct symbol *
symbol_from_id(struct symbol_table *symtab, sym_id_t id)
{
    struct symbol *symbol;

    if (symtab == NULL) {
        return NULL;
    }

    TAILQ_FOREACH(symbol, &symtab->symbols, link) {
        if (symbol->id == id) {
            return symbol;
        }
    }

    return NULL;
}

struct symbol *
symbol_from_name(struct symbol_table *symtab, const char *name)
{
    struct symbol *symbol;

    if (symtab == NULL || name == NULL) {
        return NULL;
    }

    TAILQ_FOREACH(symbol, &symtab->symbols, link) {
        if (*symbol->name != *name) {
            continue;
        }

        if (strcmp(symbol->name, name) == 0) {
            return symbol;
        }
    }

    return NULL;
}

struct symbol *
symbol_field_from_name(struct symbol *symbol, const char *name)
{
    struct symbol *iter;

    if (symbol == NULL || name == NULL) {
        return NULL;
    }

    TAILQ_FOREACH(iter, &symbol->fields, field_link) {
        if (*iter->name != *name) {
            continue;
        }

        if (strcmp(iter->name, name) == 0) {
            return iter;
        }
    }

    return NULL;
}

void
symbol_table_destroy(struct symbol_table *symtab)
{
    struct symbol *symbol;

    if (symtab == NULL) {
        return;
    }

    symbol = TAILQ_FIRST(&symtab->symbols);
    while (symbol != NULL) {
        TAILQ_REMOVE(&symtab->symbols, symbol, link);
        if (symbol->name != NULL) {
            free(symbol->name);
        }
        symbol_fields_destroy(symbol);
        free(symbol);
        symbol = TAILQ_FIRST(&symtab->symbols);
    }
}

int
symbol_field_new(struct symbol *symbol, const char *name,
    bup_type_t type, struct symbol **res)
{
    struct symbol *new_symbol;
    struct datum_type *dtype;

    if (symbol == NULL || name == NULL) {
        errno = -EINVAL;
        return -1;
    }

    new_symbol = malloc(sizeof(*new_symbol));
    if (new_symbol == NULL) {
        errno = -ENOMEM;
        return -1;
    }

    /* Initialize the symbol */
    memset(new_symbol, 0, sizeof(*new_symbol));
    new_symbol->id = symbol->field_count++;
    new_symbol->name = strdup(name);

    /* Initialize the datam type */
    dtype = &new_symbol->data_type;
    dtype->type = type;

    if (res != NULL) {
        *res = new_symbol;
    }

    TAILQ_INSERT_TAIL(&symbol->fields, new_symbol, field_link);
    return 0;
}
