/*
 * mpc - Micro Parser Combinator library for c
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
    char* filename;
    char* failure;
    char* *expected;
    char recieved;
} mpc_err_t;

void mpc_err_delete(mpc_err_t* e);
char* mpc_err_string(mpc_err_t* e);
void mpc_err_print(mpc_err_t* e);
void mpc_err_print_to(mpc_err_t* e, FILE* f);

// Parsing
typedef void mpc_val_t;

typedef union {
    mpc_err_t* error;
    mpc_val_t* output;
} mpc_result_t;

struct mpc_parser_t;
typedef struct mpc_parser_t mpc_parser_t;

#endif  // __MPC_H
