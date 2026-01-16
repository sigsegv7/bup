/*
 * Copyright (c) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#ifndef BUP_SYMBOL_H
#define BUP_SYMBOL_H 1

#include <sys/queue.h>
#include <stdint.h>
#include <stddef.h>
#include "bup/types.h"

/* Forward declaration */
struct ast_node;

/* Symbol ID */
typedef size_t sym_id_t;

/*
 * Represents valid symbol types
 */
typedef enum {
    SYMBOL_NONE,
    SYMBOL_VAR,
    SYMBOL_FUNC,
    SYMBOL_STRUCT,
} sym_type_t;

/*
 * Represents a program symbol
 *
 * @name: Symbol name
 * @id: Symbol ID
 * @type: Symbol type
 * @data_type: Data type
 * @is_global: If set, symbol is global
 * @field_count: Number of fields (if structure)
 * @fields: Fields (if structure)
 * @field link: Field queue link
 * @link: Queue link
 */
struct symbol {
    char *name;
    sym_id_t id;
    sym_type_t type;
    struct datum_type data_type;
    uint8_t is_global : 1;
    size_t field_count;
    struct symbol *parent;
    TAILQ_HEAD(, symbol) fields;
    TAILQ_ENTRY(symbol) field_link;
    TAILQ_ENTRY(symbol) link;
};

#define FIELD_FOREACH(SYMBOL, VAR)   \
    TAILQ_FOREACH(                   \
        (VAR),                       \
        &(SYMBOL)->fields,           \
        field_link                   \
    )

/*
 * Represents the program symbol table
 *
 * @symbol_count: Number of symbols in table
 * @symbols: List of symbols present
 */
struct symbol_table {
    size_t symbol_count;
    TAILQ_HEAD(, symbol) symbols;
};

/*
 * Initialize the symbol table
 *
 * @symtab: Symbol table to initialize
 *
 * Returns zero on success
 */
int symbol_table_init(struct symbol_table *symtab);

/*
 * Look up a symbol using its ID
 *
 * @symtab: Symbol table to search in
 * @id: Symbol ID to lookup
 *
 * Returns NULL on failure
 */
struct symbol *symbol_from_id(struct symbol_table *symtab, sym_id_t id);

/*
 * Obtain a symbol using its name
 *
 * @symtab: Symbol table to look up from
 * @name:   Name to look up
 *
 * Returns NULL on failure
 */
struct symbol *symbol_from_name(struct symbol_table *symtab, const char *name);

/*
 * Obtain a sub-symbol using its name
 *
 * @symbol: Symbol parent to look up from
 * @name:   Name to look up
 *
 * Returns NULL on failure
 */
struct symbol *symbol_field_from_name(struct symbol *symbol, const char *name);

/*
 * Allocate a new symbol
 *
 * @symtab: Symbol table to add symbol to
 * @name: Name of new symbol
 * @type: Symbol data type
 * @res: Symbol result is written here
 *
 * Returns zero on success
 */
int symbol_new(
    struct symbol_table *symtab, const char *name,
    bup_type_t type, struct symbol **res
);

/*
 * Allocate a new field symbol (sub-symbol)
 *
 * @symbol: Symbol to add to
 * @name:   Name of sub-symbol
 * @type:   Symbol data type
 * @res:    Symbol result is written here
 */
int symbol_field_new(
    struct symbol *symbol, const char *name,
    bup_type_t type, struct symbol **res
);

/*
 * Destroy a symbol table
 *
 * @symtab: Symbol table to destroy
 */
void symbol_table_destroy(struct symbol_table *symtab);

#endif  /* !BUP_SYMBOL_H */
