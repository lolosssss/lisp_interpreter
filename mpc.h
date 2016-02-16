/*
 * mpc - Micro Parser Combinator library for c
 *
 * https://github.com/orangeduck/mpc
 */

#ifndef __MPC_H
#define __MPC_H

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <errno.h>

// State Type
typedef struct {
    int pos;
    int row;
    int col;
} mpc_state_t;


// Error Type
typedef struct {
    mpc_state_t state;
    int expected_num;
    char *filename;
    char *failure;
    char **expected;
    char recieved;
} mpc_err_t;

void mpc_err_delete(mpc_err_t *e);
char* mpc_err_string(mpc_err_t *e);
void mpc_err_print(mpc_err_t *e);
void mpc_err_print_to(mpc_err_t *e, FILE *f);


// Input Type
enum {
    MPC_INPUT_STRING = 0,
    MPC_INPUT_FILE = 1,
    MPC_INPUT_PIPE = 2
};

typedef struct {
    int type;
    char *filename;
    mpc_state_t state;

    char *string;
    char *buffer;
    FILE *file;

    int backtrack;
    int marks_num;
    mpc_state_t *marks;
    char *lasts;
    char last;
} mpc_input_t;


// Parsing
typedef void mpc_val_t;

typedef union {
    mpc_err_t *error;
    mpc_val_t *output;
} mpc_result_t;

struct mpc_parser_t;
typedef struct mpc_parser_t mpc_parser_t;

int mpc_parse(const char *filename, const char *string, mpc_parser_t *p, mpc_result_t *r);
int mpc_parse_file(const char *filename, FILE *file, mpc_parser_t *p, mpc_result_t *r);
int mpc_parse_pipe(const char *filename, FILE *pipe, mpc_parser_t *p, mpc_result_t *r);
int mpc_parse_contents(const char *filename, mpc_parser_t *p, mpc_result_t *r);


// Function Types
typedef void (*mpc_dtor_t)(mpc_val_t *);
typedef mpc_val_t *(*mpc_ctor_t)(void);
typedef mpc_val_t *(*mpc_apply_t)(mpc_val_t *);
typedef mpc_val_t *(*mpc_apply_to_t)(mpc_val_t *, void *);
typedef mpc_val_t *(*mpc_fold_t)(int, mpc_val_t **);


// Building a Parser
mpc_parser_t *mpc_new(const char *name);
mpc_parser_t *mpc_define(mpc_parser_t *p, mpc_parser_t *a);
mpc_parser_t *mpc_undefine(mpc_parser_t *p);

void mpc_delete(mpc_parser_t *p);
void mpc_cleanup(int n, ...);

#endif  // __MPC_H
