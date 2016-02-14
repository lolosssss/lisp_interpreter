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

static mpc_state_t *mpc_state_copy(mpc_state_t s)
{
    mpc_state_t *r = malloc(sizeof(mpc_state_t));
    memcpy(r, &s, sizeof(mpc_state_t));

    return r;
}

// Error Type

static mpc_err_t *mpc_err_new(const char *filename, mpc_state_t s, const char *expected, char recieved)
{
    mpc_err_t *x = malloc(sizeof(mpc_err_t));
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

static mpc_err_t *mpc_err_fail(const char *filename, mpc_state_t s, const char *failure)
{
    mpc_err_t *x = malloc(sizeof(mpc_err_t));
    x->filename = malloc(strlen(filename) + 1);
    strcpy(x->filename, filename);
    x->state = s;
    x->expected_num = 0;
    x->expected = NULL;
    x->failure = malloc(strlen(failure) + 1);
    strcpy(x->failure, failure);
    x->recieved = ' ';

    return x;
}

void mpc_err_delete(mpc_err_t *x)
{
    int i;
    for (i = 0; i < x->expected_num; i++)
    {
        free(x->expected[i]);
    }

    free(x->expected);
    free(x->filename);
    free(x->failure);
    free(x);
}

static int mpc_err_contains_expected(mpc_err_t *x, char *expected)
{
    int i;
    for (i = 0; i < x->expected_num; i++)
    {
        if (strcmp(x->expected[i], expected) == 0)
            return 1;
    }

    return 0;
}

static void mpc_err_add_expected(mpc_err_t *x, char *expected)
{
    x->expected_num++;
    x->expected = realloc(x->expected, sizeof(char*) * x->expected_num);
    x->expected[x->expected_num - 1] = malloc(strlen(expected) + 1);
    strcpy(x->expected[x->expected_num - 1], expected);
}

static void mpc_err_clear_expected(mpc_err_t *x, char *expected)
{
    int i;
    for (i = 0; i < x->expected_num; i++)
    {
        free(x->expected[i]);
    }
    x->expected_num = 1;
    x->expected = realloc(x->expected, sizeof(char*) * x->expected_num);
    x->expected[0] = malloc(strlen(expected) + 1);
    strcpy(x->expected[0], expected);
}

void mpc_err_print(mpc_err_t *x)
{
    mpc_err_print_to(x, stdout);
}

void mpc_err_print_to(mpc_err_t *x, FILE *f)
{
    char *str = mpc_err_string(x);
    fprintf(f, "%s", str);
    free(str);
}

void mpc_err_string_cat(char *buffer, int *pos, int *max, char *fmt, ...)
{
    int left = ((*max) - (*pos));
    va_list va;
    va_start(va, fmt);
    if (left < 0)
        left = 0;
    (*pos) += vsprintf(buffer + (*pos), fmt, va);
    va_end(va);
}

static char char_unescape_buffer[3];
static char *mpc_err_char_unescape(char c)
{
    char_unescape_buffer[0] = '\'';
    char_unescape_buffer[1] = ' ';
    char_unescape_buffer[2] = '\'';

    switch (c)
    {
        case '\a': return "bell";
        case '\b': return "backspace";
        case '\f': return "formfeed";
        case '\r': return "carriage return";
        case '\v': return "vertical tab";
        case '\0': return "end of input";
        case '\n': return "newline";
        case '\t': return "tab";
        case ' ' : return "space";
        default:
            char_unescape_buffer[1] = c;
            return char_unescape_buffer;
    }
}

char *mpc_err_string(mpc_err_t *x)
{
    char *buffer = calloc(1, 1024);
    int max = 1023;
    int pos = 0;
    int i;

    if (x->failure)
    {
        mpc_err_string_cat(buffer, &pos, &max,
                           "%s: error: %s\n",
                           x->filename, x->failure);
        return buffer;
    }

    mpc_err_string_cat(buffer, &pos, &max,
                       "%s:%i:%i: error: expected ",
                       x->filename, x->state.row+1, x->state.col+1);

    if (x->expected_num == 0)
    {
        mpc_err_string_cat(buffer, &pos, &max, "ERROR: NOTHING EXPECTED");
    }
    if (x->expected_num == 1)
    {
        mpc_err_string_cat(buffer, &pos, &max, "%s", x->expected[0]);
    }
    if (x->expected_num >= 2)
    {
        for (i = 0; i < x->expected_num - 2; i++)
        {
            mpc_err_string_cat(buffer, &pos, &max, "%s, ", x->expected[i]);
        }

        mpc_err_string_cat(buffer, &pos, &max, "%s or %s",
                           x->expected[x->expected_num - 2],
                           x->expected[x->expected_num - 1]);
    }

    mpc_err_string_cat(buffer, &pos, &max, " at ");
    mpc_err_string_cat(buffer, &pos, &max, mpc_err_char_unescape(x->recieved));
    mpc_err_string_cat(buffer, &pos, &max, "\n");

    return realloc(buffer, strlen(buffer) + 1);
}

static mpc_err_t *mpc_err_or(mpc_err_t **x, int n)
{
    int i, j;
    mpc_err_t *e = malloc(sizeof(mpc_err_t));
    e->state = mpc_state_invalid();
    e->expected_num = 0;
    e->expected = NULL;
    e->failure = NULL;
    e->filename = malloc(strlen(x[0]->filename) + 1);
    strcpy(e->filename, x[0]->filename);

    for (i = 0; i < n; i++)
    {
        if (x[i]->state.pos > e->state.pos)
        {
            e->state = x[i]->state;
        }
    }

    for (i = 0; i < n; i++)
    {
        if (x[i]->state.pos < e->state.pos)
            continue;

        if (x[i]->failure)
        {
            e->failure = malloc(strlen(x[i]->failure) + 1);
            strcpy(e->failure, x[i]->failure);
            break;
        }

        e->recieved = x[i]->recieved;

        for (j = 0; j < x[i]->expected_num; j++)
        {
            if (!mpc_err_contains_expected(e, x[i]->expected[j]))
            {
                mpc_err_add_expected(e, x[i]->expected[j]);
            }
        }
    }

    for (i = 0; i < n; i++)
    {
        mpc_err_delete(x[i]);
    }

    return e;
}

static mpc_err_t *mpc_err_repeat(mpc_err_t *x, const char *prefix)
{
    int i;
    char *expect = malloc(strlen(prefix) + 1);

    strcpy(expect, prefix);

    if (x->expected_num == 1)
    {
        expect = realloc(expect, strlen(expect) + strlen(x->expected[0]) + 1);
        strcat(expect, x->expected[0]);
    }

    if (x->expected_num > 1)
    {
        for (i = 0; i < x->expected_num - 2; i++)
        {
            expect = realloc(expect, strlen(expect) + strlen(x->expected[i]) + strlen(", ") + 1);
            strcat(expect, x->expected[i]);
            strcat(expect, ", ");
        }

        expect = realloc(expect, strlen(expect) + strlen(x->expected[x->expected_num - 2]) + strlen(" or ") + 1);
        strcat(expect, x->expected[x->expected_num - 2]);
        strcat(expect, " or ");
        expect = realloc(expect, strlen(expect) + strlen(x->expected[x->expected_num - 1]) + 1);
        strcat(expect, x->expected[x->expected_num - 1]);
    }

    mpc_err_clear_expected(x, expect);
    free(expect);

    return x;
}

static mpc_err_t *mpc_err_many1(mpc_err_t *x)
{
    return mpc_err_repeat(x, "one or more of ");
}

static mpc_err_t *mpc_err_count(mpc_err_t *x, int n)
{
    mpc_err_t *y;
    int digits = n / 10 + 1;
    char *prefix = malloc(digits + strlen(" of ") + 1);
    sprintf(prefix, "%i of ", n);
    y = mpc_err_repeat(x, prefix);
    free(prefix);

    return y;
}
