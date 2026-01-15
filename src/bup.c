/*
 * Copyright (c) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include "bup/state.h"
#include "bup/parser.h"

#define BUP_VERSION "0.0.3"

/* Runtime flags */
static bool asm_only = false;
static const char *binfmt = "elf64";

static void
help(void)
{
    printf(
        "The bup compiler - bup gup wup!\n"
        "-------------------------------\n"
        "[-h]   Display this help menu\n"
        "[-v]   Display the version\n"
        "[-a]   Output ASM file only [do not assemble]\n"
        "Usage: bup <flags, ...> <files, ...>\n"
    );
}

static void
version(void)
{
    printf(
        "Copyright (c) 2026 Ian Moffett\n"
        "Bup compiler v%s\n",
        BUP_VERSION
    );
}

static int
compile(const char *path)
{
    struct bup_state state;
    char cmd[64];

    if (bup_state_init(path, &state) < 0) {
        printf("fatal: failed to initialize state\n");
        return -1;
    }

    if (parser_parse(&state) < 0) {
        printf("AAAA\n");
        return -1;
    }

    bup_state_destroy(&state);
    if (!asm_only) {
        snprintf(
            cmd,
            sizeof(cmd),
            "nasm -f%s %s",
            binfmt,
            DEFAULT_ASMOUT
        );

        system(cmd);
        remove(DEFAULT_ASMOUT);
    }

    return 0;
}

int
main(int argc, char **argv)
{
    int opt;

    if (argc < 2) {
        printf("fatal: expected argument\n");
        help();
        return -1;
    }

    while ((opt = getopt(argc, argv, "hva")) != -1) {
        switch (opt) {
        case 'h':
            help();
            return -1;
        case 'v':
            version();
            return -1;
        case 'a':
            asm_only = true;
            break;
        }
    }

    while (optind < argc) {
        if (compile(argv[optind++]) < 0) {
            break;
        }
    }

    return 0;
}
