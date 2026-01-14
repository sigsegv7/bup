/*
 * Copyright (c) 2026, Ian Moffett.
 * Provided under the BSD-3 clause.
 */

#ifndef BUP_STATE_H
#define BUP_STATE_H 1

/*
 * Represents the compiler state
 *
 * @in_fd: Input file descriptor
 * @putback: Putback buffer
 */
struct bup_state {
    int in_fd;
    char putback;
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
