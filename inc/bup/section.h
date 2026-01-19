/*
 * Copyright (c) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#ifndef BUP_SECTION_H
#define BUP_SECTION_H 1

/*
 * Represents valid program sections
 */
typedef enum {
    SECTION_DISABLED,
    SECTION_NONE,
    SECTION_TEXT,
    SECTION_DATA,
    SECTION_BSS,
    SECTION_MAX
} bin_section_t;

static inline char *
section_to_name(bin_section_t type)
{
    switch (type) {
    case SECTION_TEXT:  return ".text";
    case SECTION_DATA:  return ".data";
    case SECTION_BSS:   return ".bss";
    default:            return ".unknown";
    }

    return ".unknown";
}

#endif  /* !BUP_SECTION_H */
