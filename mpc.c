#include "mpc.h"

// State Type
static mpc_state_t mpc_state_invalid(void)
{
    mpc_state_t s;
    s.pos = -1;
    s.row = -1;
    s.col = -1;

    return s;
}

static mpc_state_t mpc_state_new(void)
{
    mpc_state_t s;
    s.pos = 0;
    s.row = 0;
    s.col = 0;

    return s;
}

static mpc_state_t* mpc_state_copy(mpc_state_t s)
{
    mpc_state_t* r = malloc(sizeof(mpc_state_t));
    memcpy(r, &s, sizeof(mpc_state_t));

    return r;
}

static mpc_err_t* mpc_err_new(const char* filename, mpc_state_t s, const char* expected, char recieved)
{
    mpc_err_t* x = malloc(sizeof(mpc_err_t));
    x->filename = malloc(strlen(filename) + 1);
    strcpy(x->filename, filename);
    x->state = s;
    x->expected_num = 1;
    x->expected = malloc(sizeof(char*));
    x->expected[0] = malloc(strlen(expected) + 1);
    strcpy(x->expected[0], expected);
    x->failure = NULL;
    x->recieved = recieved;

    return x;
}
