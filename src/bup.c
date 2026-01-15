/*
 * Copyright (c) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include "bup/state.h"
#include "bup/parser.h"

#define BUP_VERSION "0.0.2"

static void
help(void)
{
    printf(
        "The bup compiler - bup gup wup!\n"
        "-------------------------------\n"
        "[-h]   Display this help menu\n"
        "[-v]   Display the version\n"
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

    if (bup_state_init(path, &state) < 0) {
        printf("fatal: failed to initialize state\n");
        return -1;
    }

    if (parser_parse(&state) < 0) {
        return -1;
    }

    bup_state_destroy(&state);
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

    while ((opt = getopt(argc, argv, "hv")) != -1) {
        switch (opt) {
        case 'h':
            help();
            return -1;
        case 'v':
            version();
            return -1;
        }
    }

    while (optind < argc) {
        if (compile(argv[optind++]) < 0) {
            break;
        }
    }

    return 0;
}
