/*
 * mpc - Micro Parser Combinator library for c
 *
 * https://github.com/orangeduck/mpc
 */


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

// Input Type

/*
 * In mpc the input type has three modes of
 * operation: String, File and Pipe.
 *
 * String is easy. The whole contents are loaded
 * into a buffer and scanned through. The cursor
 * can jump around at will makeing backtracking
 * easy.
 *
 * The second is a File which is also somewhat
 * easy. The contents are never loaded into
 * memory but backtracking can still be achieved
 * by seeking in the file at different positions.
 *
 * The final mode is Pipe. This is the difficult
 * one. As we assume pipes cannot be seeked - and
 * only support a single character lookahead at
 * any point, when the input is marked for a
 * potential backtracking we start buffering any
 * input.
 *
 * This means that if we are requested to seek
 * back we can simply start reading from the
 * buffer instead of the input.
 *
 * Of course using `mpc_predictive` will disable
 * backtracking and make LL(1) grammars easy
 * to parse for all input methods.
 *
 */

static mpc_input_t *mpc_input_new_string(const char *filename, const char *string)
{
    mpc_input_t *i = malloc(sizeof(mpc_input_t));

    i->filename = malloc(strlen(filename) + 1);
    strcpy(i->filename, filename);
    i->type = MPC_INPUT_STRING;

    i->state = mpc_state_new();

    i->string = malloc(strlen(string) + 1);
    strcpy(i->string, string);
    i->buffer = NULL;
    i->file = NULL;

    i->backtrack = 1;
    i->marks_num = 0;
    i->marks = NULL;
    i->lasts = NULL;

    i->last = '\0';

    return i;
}
static mpc_input_t *mpc_input_new_pipe(const char *filename, FILE *pipe)
{
    mpc_input_t *i = malloc(sizeof(mpc_input_t));

    i->filename = malloc(strlen(filename) + 1);
    strcpy(i->filename, filename);

    i->type = MPC_INPUT_PIPE;
    i->state = mpc_state_new();

    i->string = NULL;
    i->buffer = NULL;
    i->file = pipe;

    i->backtrack = 1;
    i->marks_num = 0;
    i->marks = NULL;
    i->lasts = NULL;

    i->last = '\0';

    return i;
}

static mpc_input_t *mpc_input_new_file(const char *filename, FILE *file)
{
    mpc_input_t *i = malloc(sizeof(mpc_input_t));

    i->filename = malloc(strlen(filename) + 1);
    strcpy(i->filename, filename);
    i->type = MPC_INPUT_FILE;
    i->state = mpc_state_new();

    i->string = NULL;
    i->buffer = NULL;
    i->file = file;

    i->backtrack = 1;
    i->marks_num = 0;
    i->marks = NULL;
    i->lasts = NULL;

    i->last = '\0';

    return i;
}

static void mpc_input_delete(mpc_input_t *i)
{
    free(i->filename);

    if (i->type == MPC_INPUT_STRING)
        free(i->string);
    if (i->type == MPC_INPUT_PIPE)
        free(i->buffer);

    free(i->marks);
    free(i->lasts);
    free(i);
}

static void mpc_input_backtrack_disable(mpc_input_t *i)
{
    i->backtrack--;
}

static void mpc_input_backtrack_enable(mpc_input_t *i)
{
    i->backtrack++;
}

static void mpc_input_mark(mpc_input_t *i)
{
    if (i->backtrack < 1)
        return;

    i->marks_num++;
    i->marks = realloc(i->marks, sizeof(mpc_state_t) * i->marks_num);
    i->lasts = realloc(i->lasts, sizeof(char) * i->marks_num);
    i->marks[i->marks_num - 1] = i->state;
    i->lasts[i->marks_num - 1] = i->last;

    if (i->type == MPC_INPUT_PIPE && i->marks_num == 1)
    {
        i->buffer = calloc(1, 1);
    }
}

static void mpc_input_unmark(mpc_input_t *i)
{
    if (i->backtrack < 1)
        return;

    i->marks_num--;
    i->marks = realloc(i->marks, sizeof(mpc_state_t) * i->marks_num);
    i->lasts = realloc(i->lasts, sizeof(char) * i->marks_num);

    if (i->type == MPC_INPUT_PIPE && i->marks_num == 0)
    {
        free(i->buffer);
        i->buffer = NULL;
    }
}

static void mpc_input_rewind(mpc_input_t *i)
{
    if (i->backtrack < 1)
        return;

    i->state = i->marks[i->marks_num - 1];
    i->last = i->lasts[i->marks_num - 1];

    if (i->type == MPC_INPUT_FILE)
    {
        fseek(i->file, i->state.pos, SEEK_SET);
    }

    mpc_input_unmark(i);
}

static int mpc_input_buffer_in_range(mpc_input_t *i)
{
    return i->state.pos < (strlen(i->buffer) + i->marks[0].pos);
}

static char mpc_input_buffer_get(mpc_input_t *i)
{
    return i->buffer[i->state.pos - i->marks[0].pos];
}

static int mpc_input_terminated(mpc_input_t *i)
{
    if (i->type == MPC_INPUT_STRING && i->state.pos == strlen(i->string))
        return 1;
    if (i->type == MPC_INPUT_FILE && feof(i->file))
        return 1;
    if (i->type == MPC_INPUT_PIPE && feof(i->file))
        return 1;

    return 0;
}

static char mpc_input_getc(mpc_input_t *i)
{
    char c = '\0';

    switch (i->type)
    {
        case MPC_INPUT_STRING: return i->string[i->state.pos];
        case MPC_INPUT_FILE:   c = fgetc(i->file); return c;
        case MPC_INPUT_PIPE:
            {
                if (!i->buffer)
                {
                    c = getc(i->file);
                    return c;
                }
                if (i->buffer && mpc_input_buffer_in_range(i))
                {
                    c = mpc_input_buffer_get(i);
                    return c;
                }
                else
                {
                    c = getc(i->file);
                    return c;
                }
            }
        default: return c;
    }
}

static char mpc_input_peekc(mpc_input_t *i)
{
    char c = '\0';

    switch (i->type)
    {
        case MPC_INPUT_STRING: return i->string[i->state.pos];
        case MPC_INPUT_FILE:
            c = fgetc(i->file);
            if (feof(i->file))
                return '\0';
            fseek(i->file, -1, SEEK_CUR);
            return c;
        case MPC_INPUT_PIPE:
            if (!i->buffer)
            {
                c = getc(i->file);
                if (feof(i->file))
                    return '\0';
                ungetc(c, i->file);
                return c;
            }
            if (i->buffer && mpc_input_buffer_in_range(i))
            {
                return mpc_input_buffer_get(i);
            }
            else
            {
                c = getc(i->file);
                if (feof(i->file))
                    return '\0';
                ungetc(c, i->file);
                return c;
            }
        default:
            return c;
    }
}

static int mpc_input_failure(mpc_input_t *i, char c)
{
    switch (i->type)
    {
        case MPC_INPUT_STRING:
            break;
        case MPC_INPUT_FILE:
            fseek(i->file, -1, SEEK_CUR);
            break;
        case MPC_INPUT_PIPE:
            if (!i->buffer)
            {
                ungetc(c, i->file);
                break;
            }
            if (i->buffer && mpc_input_buffer_in_range(i))
                break;
            else
                ungetc(c, i->file);
    }

    return 0;
}

static int mpc_input_success(mpc_input_t *i, char c, char **o)
{
    if (i->type == MPC_INPUT_PIPE && i->buffer && !mpc_input_buffer_in_range(i))
    {
        i->buffer = realloc(i->buffer, strlen(i->buffer) + 2);
        i->buffer[strlen(i->buffer) + 1] = '\0';
        i->buffer[strlen(i->buffer) + 0] = c;
    }

    i->last = c;
    i->state.pos++;
    i->state.col++;

    if (c == '\n')
    {
        i->state.col = 0;
        i->state.row++;
    }

    if (o)
    {
        (*o) = malloc(2);
        (*o)[0] = c;
        (*o)[1] = '\0';
    }

    return 1;
}

static int mpc_input_any(mpc_input_t *i, char **o)
{
    char x = mpc_input_getc(i);
    if (mpc_input_terminated(i))
        return 0;

    return mpc_input_success(i, x, o);
}

static int mpc_input_char(mpc_input_t *i, char c, char **o)
{
    char x = mpc_input_getc(i);
    if (mpc_input_terminated(i))
        return 0;

    return x == c ? mpc_input_success(i, x, o) : mpc_input_failure(i, x);
}
static int mpc_input_range(mpc_input_t *i, char c, char d, char **o)
{
    char x = mpc_input_getc(i);
    if (mpc_input_terminated(i))
        return 0;

    return x >= c && x <= d ? mpc_input_success(i, x, o) : mpc_input_failure(, x);
}

static int mpc_input_oneof(mpc_input_t *i, const char *c, char **o)
{
    char x = mpc_input_getc(i);
    if (mpc_input_terminated(i))
        return 0;

    return strchr(c, x) != 0 ? mpc_input_success(i, x, o) : mpc_input_failure(i, x);
}

static int mpc_input_noneof(mpc_input_t *i, const char *c, char **o)
{
    char x = mpc_input_getc(i);
    if (mpc_input_terminated(i))
        return 0;

    return strchr(c, x) == 0 ? mpc_input_success(i, x, o) : mpc_input_failure(i, x);
}

static int mpc_input_satisfy(mpc_input_t *i, int(*cond)(char), char **o)
{
    char x = mpc_input_getc(i);
    if (mpc_input_terminated(i))
        return 0;

    return cond(x) ? mpc_input_success(i, x, o) : mpc_input_failure(i, x);
}

static int mpc_input_string(mpc_input_t *i, const char *c, char **o)
{
    char *co = NULL;
    const char *x = c;

    mpc_input_mark(i);
    while (*x)
    {
        if (mpc_input_char(i, *x, &co))
            free(co);
        else
        {
            mpc_input_rewind(i);
            return 0;
        }

        x++;
    }
    mpc_input_unmark(i);

    *o = malloc(strlen(c) + 1);
    strcpy(*o, c);

    return 1;
}

static int mpc_input_anchor(mpc_input_t *i, int (*f)(char, char))
{
    return f(i->last, mpc_input_peekc(i));
}


// Parser Type

enum {
    MPC_TYPE_UNDEFINED = 0,
    MPC_TYPE_PASS      = 1,
    MPC_TYPE_FAIL      = 2,
    MPC_TYPE_LIFT      = 3,
    MPC_TYPE_LIFT_VAL  = 4,
    MPC_TYPE_EXPECT    = 5,
    MPC_TYPE_ANCHOR    = 6,
    MPC_TYPE_STATE     = 7,
    MPC_TYPE_ANY       = 8,
    MPC_TYPE_SINGLE    = 9,
    MPC_TYPE_ONEOF     = 10,
    MPC_TYPE_NONEOF    = 11,
    MPC_TYPE_RANGE     = 12,
    MPC_TYPE_SATISFY   = 13,
    MPC_TYPE_STRING    = 14,
    MPC_TYPE_APPLY     = 15,
    MPC_TYPE_APPLY_TO  = 16,
    MPC_TYPE_PREDICT   = 17,
    MPC_TYPE_NOT       = 18,
    MPC_TYPE_MAYBE     = 19,
    MPC_TYPE_MANY      = 20,
    MPC_TYPE_MANY1     = 21,
    MPC_TYPE_COUNT     = 22,
    MPC_TYPE_OR        = 23,
    MPC_TYPE_AND       = 24
};

typedef struct { char *m; } mpc_pdata_fail_t;
typedef struct { mpc_ctor_t lf; void *x; } mpc_pdata_lift_t;
typedef struct { mpc_parser_t *x; char *m; } mpc_pdata_expect_t;
typedef struct { int (*f)(char, char); } mpc_pdata_anchor_t;
typedef struct { char x; } mpc_pdata_single_t;
typedef struct { char x; char y; } mpc_pdata_range_t;
typedef struct { int (*f)(char); } mpc_pdata_satisfy_t;
typedef struct { char *x; } mpc_pdata_string_t;
typedef struct { mpc_parser_t *x; mpc_apply_t f;} mpc_pdata_apply_t;
typedef struct { mpc_parser_t *x; mpc_apply_to_t f; void *d; } mpc_pdata_apply_to_t;
typedef struct { mpc_parser_t *x; } mpc_pdata_predict_t;
typedef struct { mpc_parser_t *x; mpc_dtor_t dx; mpc_ctor_t lf; } mpc_pdata_not_t;
typedef struct { int n; mpc_fold_t f; mpc_parser_t *x; mpc_dtor_t dx; } mpc_pdata_repeat_t;
typedef struct { int n; mpc_parser_t **xs; } mpc_pdata_or_t;
typedef struct { int n; mpc_fold_t f; mpc_parser_t **xs; mpc_dtor_t *dxs; } mpc_pdata_and_t;

typedef union {
    mpc_pdata_fail_t fail;
    mpc_pdata_lift_t lift;
    mpc_pdata_expect_t expect;
    mpc_pdata_anchor_t anchor;
    mpc_pdata_single_t single;
    mpc_pdata_range_t range;
    mpc_pdata_satisfy_t satisfy;
    mpc_pdata_string_t string;
    mpc_pdata_apply_t apply;
    mpc_pdata_apply_to_t apply_to;
    mpc_pdata_predict_t predict;
    mpc_pdata_not_t not;
    mpc_pdata_repeat_t repeat;
    mpc_pdata_and_t and;
    mpc_pdata_or_t or;
} mpc_pdata_t;

struct mpc_parser_t {
    char retained;
    char *name;
    char type;
    mpc_pdata_t data;
};


// Stack Type
typedef struct {
    int parsers_num;
    int parsers_slots;
    mpc_parser_t **parsers;
    int *states;
    int results_num;
    int results_slots;
    mpc_result_t *results;
    int *returns;
    mpc_err_t *err;
} mpc_stack_t;

static mpc_stack_t *mpc_stack_new(const char *filename)
{
    mpc_stack_t *s = malloc(sizeof(mpc_stack_t));

    s->parsers_num = 0;
    s->parsers_slots = 0;
    s->parsers = NULL;
    s->states = NULL;

    s->results_num = 0;
    s->results_slots = 0;
    s->results = NULL;
    s->returns = NULL;

    s->err = mpc_err_fail(filename, mpc_state_invalid(), "Unknown Error");

    return s;
}

static void mpc_stack_err(mpc_stack_t *s, mpc_err_t *e)
{
    mpc_err_t *errs[2];
    errs[0] = s->err;
    errs[1] = e;
    s->err = mpc_err_or(errs, 2);
}

static int mpc_stack_terminate(mpc_stack_t *s, mpc_result_t *r)
{
    int success = s->returns[0];

    if (success)
    {
        r->output = s->results[0].output;
        mpc_err_delete(s->err);
    }
    else
    {
        mpc_stack_err(s, s->results[0].error);
        r->error = s->err;
    }

    free(s->parsers);
    free(s->states);
    free(s->results);
    free(s->returns);
    free(s);

    return success;
}


// Stack Parser Stuff
static void mpc_stack_set_state(mpc_stack_t *s, int x)
{
    s->states[s->parsers_num - 1] = x;
}

static void mpc_stack_parsers_reserve_more(mpc_stack_t *s)
{
    if (s->parsers_num > s->parsers_slots)
    {
        s->parsers_slots = ceil((s->parsers_slots + 1) * 1.5);
        s->parsers = realloc(s->parsers, sizeof(mpc_parser_t *) * s->parsers_slots);
        s->states = realloc(s->states, sizeof(int) * s->parsers_slots);
    }
}

static void mpc_stack_parsers_reserve_less(mpc_stack_t *s)
{
    if (s->parsers_slots > pow(s->parsers_num + 1, 1.5))
    {
        s->parsers_slots = floor(s->parsers_slots - 1) * (1.0 / 1.5);
        s->parsers = realloc(s->parsers, sizeof(mpc_parser_t *) * s->parsers_slots);
        s->states = realloc(s->states, sizeof(int) * s->parsers_slots);
    }
}

static void mpc_stack_pushp(mpc_stack_t *s, mpc_parser_t *p)
{
    s->parsers_num++;
    mpc_stack_parsers_reserve_more(s);
    s->parsers[s->parsers_num - 1] = p;
    s->states[s->parsers_num - 1] = 0;
}

static void mpc_stack_popp(mpc_stack_t *s, mpc_parser_t **p, int *st)
{
    *p = s->parsers[s->parsers_num - 1];
    *st = s->states[s->parsers_num - 1];
    s->parsers_num--;
    mpc_stack_parsers_reserve_less(s);
}

static void mpc_stack_peepp(mpc_stack_t *s, mpc_parser_t **p, int *st)
{
    *p = s->parsers[s->parsers_num - 1];
    *st = s->states[s->parsers_num - 1];
}

static int mpc_stack_empty(mpc_stack_t *s)
{
    return s->parsers_num = 0;
}


// Stack Result Stuff
static mpc_result_t mpc_result_err(mpc_err_t *e)
{
    mpc_result_t r;
    r.error = e;

    return r;
}

static mpc_result_t mpc_result_out(mpc_val_t *x)
{
    mpc_result_t r;
    r.output = x;

    return r;
}

static void mpc_stack_results_reserve_more(mpc_stack_t *s)
{
    if (s->results_num > s->results_slots)
    {
        s->results_slots = ceil((s->results_slots + 1) * 1.5);
        s->results = realloc(s->results, sizeof(mpc_result_t) * s->results_slots);
        s->returns = realloc(s->returns, sizeof(int) *s->results_slots);
    }
}

static void mpc_stack_results_reserve_less(mpc_stack_t *s)
{
    if (s->results_slots > pow(s->results_num + 1, 1.5))
    {
        s->results_slots = floor(s->results_slots - 1) * (1.0 / 1.5);
        s->results = realloc(s->results, sizeof(mpc_result_t) * s->results_slots);
        s->returns = realloc(s->returns, sizeof(int) * s->results_slots);
    }
}

static void mpc_stack_pushr(mpc_stack_t *s, mpc_result_t x, int r)
{
    s->results_num++;
    mpc_stack_results_reserve_more(s);
    s->results[s->results_num - 1] = x;
    s->returns[s->results_num - 1] = r;
}

static int mpc_stack_popr(mpc_stack_t *s, mpc_result_t *x)
{
    int r;
    *x = s->results[s->results_num - 1];
    r = s->returns[s->results_num - 1];
    s->results_num--;
    mpc_stack_results_reserve_less(s);

    return r;
}

static int mpc_stack_peekr(mpc_stack_t *s, mpc_result_t *x)
{
    *x = s->results[s->results_num - 1];
    return s->returns[s->results_num - 1];
}

static void mpc_stack_popr_err(mpc_stack_t *s, int n)
{
    mpc_result_t x;

    while (n)
    {
        mpc_stack_popr(s, &x);
        mpc_stack_err(s, x.error);
        n--;
    }
}

static void mpc_stack_popr_out(mpc_stack_t *s, int n, mpc_dtor_t *ds)
{
    mpc_result_t x;

    while (n)
    {
        mpc_stack_popr(s, &x);
        ds[n - 1](x.output);
        n--;
    }
}

static void mpc_stack_popr_out_single(mpc_stack_t *s, int n, mpc_dtor_t dx)
{
    mpc_result_t x;

    while (n)
    {
        mpc_stack_popr(s, &x);
        dx(x.output);
        n--;
    }
}

static void mpc_stack_popr_n(mpc_stack_t *s, int n)
{
    mpc_result_t x;

    while (n)
    {
        mpc_stack_popr(s, &x);
        n--;
    }
}

static mpc_val_t *mpc_stack_merger_out(mpc_stack_t *s, int n, mpc_fold_t f)
{
    mpc_val_t *x = f(n, (mpc_val_t **)(&s->results[s->results_num - n]));
    mpc_stack_popr_n(s, n);

    return x;
}

static mpc_err_t *mpc_stack_merger_err(mpc_stack_t *s, int n)
{
    mpc_err_t *x = mpc_err_or((mpc_err_t **)(&s->results[s->results_num - n]), n);
    mpc_stack_popr_n(s, n);

    return x;
}


/**
 * TO-DO : This function can be improved.
 */

#define MPC_CONTINUE(st, x) mpc_stack_set_state(stk, st); mpc_stack_pushp(stk, x); continue
#define MPC_SUCCESS(x) mpc_stack_popp(stk, &p, &st); mpc_stack_pushr(stk, mpc_result_out(x), 1); continue
#define MPC_FAILURE(x) mpc_stack_popp(stk, &p, &st); mpc_stack_pushr(stk, mpc_result_err(x), 0); continue
#define MPC_PRIMATIVE(x, f) if (f) { MPC_SUCCESS(x); } else { MPC_FAILURE(mpc_err_fail(i->filename, i->state, "Incorrect Input")); }

int mpc_parse_input(mpc_input_t *i, mpc_parser_t *init, mpc_result_t *final)
{
    /* Stack */
    int st = 0;
    mpc_parser_t *p = NULL;
    mpc_stack_t *stk = mpc_stack_new(i->filename);

    /* Variables */
    char *s;
    mpc_result_t r;

    mpc_stack_pushp(stk, init);

    while (!mpc_stack_empty(stk))
    {
        mpc_stack_peepp(stk, &p, &st);

        switch (p->type)
        {
            /* Basic Parsers */
            case MPC_TYPE_ANY:        MPC_PRIMATIVE(s, mpc_input_any(i, &s));
            case MPC_TYPE_SINGLE:     MPC_PRIMATIVE(s, mpc_input_char(i, p->data.single.x, &s));
            case MPC_TYPE_RANGE:      MPC_PRIMATIVE(s, mpc_input_range(i, p->data.range.x, p->data.range.y, &s));
            case MPC_TYPE_ONEOF:      MPC_PRIMATIVE(s, mpc_input_oneof(i, p->data.string.x, &s));
            case MPC_TYPE_NONEOF:     MPC_PRIMATIVE(s, mpc_input_noneof(i, p->data.string.x, &s));
            case MPC_TYPE_SATISFY:    MPC_PRIMATIVE(s, mpc_input_satisfy(i, p->data.satisfy.f, &s));
            case MPC_TYPE_STRING:     MPC_PRIMATIVE(s, mpc_input_string(i, p->data.string.x, &s));

            /* Other Parsers */
            case MPC_TYPE_UNDEFINED:  MPC_FAILURE(mpc_err_fail(i->filename, i->state, "Parser Undefined!"));
            case MPC_TYPE_PASS:       MPC_SUCCESS(NULL);
            case MPC_TYPE_FAIL:       MPC_FAILURE(mpc_err_fail(i->filename, i->state, p->data.fail.m));
            case MPC_TYPE_LIFT:       MPC_SUCCESS(p->data.lift.lf());
            case MPC_TYPE_LIFT_VAL:   MPC_SUCCESS(p->data.lift.x);
            case MPC_TYPE_STATE:      MPC_SUCCESS(mpc_state_copy(i->state));
            case MPC_TYPE_ANCHOR:
                if (mpc_input_anchor(i, p->data.anchor.f))
                {
                    MPC_SUCCESS(NULL);
                }
                else
                {
                    MPC_FAILURE(mpc_err_new(i->filename, i->state, "anchor", mpc_input_peekc(i)));
                }

            /* Application Parsers */
            case MPC_TYPE_EXPECT:
                if (st == 0)
                {
                    MPC_CONTINUE(1, p->data.expect.x);
                }
                if (st == 1)
                {
                    if (mpc_stack_popr(stk, &r))
                    {
                        MPC_SUCCESS(r.output);
                    }
                    else
                    {
                        mpc_err_delete(r.error);
                        MPC_FAILURE(mpc_err_new(i->filename, i->state, p->data.expect.m, mpc_input_peekc(i)));
                    }
                }

            case MPC_TYPE_APPLY:
                if (st == 0)
                {
                    MPC_CONTINUE(1, p->data.apply.x);
                }
                if (st == 1)
                {
                    if (mpc_stack_popr(stk, &r))
                    {
                        MPC_SUCCESS(p->data.apply.f(r.output));
                    }
                    else
                    {
                        MPC_FAILURE(r.error);
                    }

                }

            case MPC_TYPE_APPLY_TO:
                if (st == 0)
                {
                    MPC_CONTINUE(1, p->data.apply_to.x);
                }
                if (st == 1)
                {
                    if (mpc_stack_popr(stk, &r))
                    {
                        MPC_SUCCESS(p->data.apply_to.f(r.output, p->data.apply_to.d));
                    }
                    else
                    {
                        MPC_FAILURE(r.error);
                    }
                }

            case MPC_TYPE_PREDICT:
                if (st == 0)
                {
                    mpc_input_backtrack_disable(i);
                    MPC_CONTINUE(1, p->data.predict.x);
                }
                if (st == 1)
                {
                    mpc_input_backtrack_enable(i);
                    mpc_stack_popp(stk, &p, &st);
                    continue;
                }

            /* Optional Parsers */
            /* TO-DO: update Not Error Message */
            case MPC_TYPE_NOT:
                if (st == 0)
                {
                    mpc_input_mark(i);
                    MPC_CONTINUE(1, p->data.not.x);
                }
                if (st == 1)
                {
                    if (mpc_stack_popr(stk, &r))
                    {
                        mpc_input_rewind(i);
                        p->data.not.dx(r.output);
                        MPC_FAILURE(mpc_err_new(i->filename, i->state, "opposite", mpc_input_peekc(i)));
                    }
                    else
                    {
                        mpc_input_unmark(i);
                        mpc_stack_err(stk, r.error);
                        MPC_SUCCESS(p->data.not.lf());
                    }
                }

            case MPC_TYPE_MAYBE:
                if (st == 0)
                {
                    MPC_CONTINUE(1, p->data.not.x);
                }
                if (st == 1)
                {
                    if (mpc_stack_popr(stk, &r))
                    {
                        MPC_SUCCESS(r.output);
                    }
                    else
                    {
                        mpc_stack_err(stk, r.error);
                        MPC_SUCCESS(p->data.not.lf());
                    }
                }

            /* Repeat Parsers */
            case MPC_TYPE_MANY:
                if (st == 0)
                {
                    MPC_CONTINUE(st + 1, p->data.repeat.x);
                }
                if (st > 0)
                {
                    if (mpc_stack_peekr(stk, &r))
                    {
                        MPC_CONTINUE(st + 1, p->data.repeat.x);
                    }
                    else
                    {
                        mpc_stack_popr(stk, &r);
                        mpc_stack_err(stk, r.error);
                        MPC_SUCCESS(mpc_stack_merger_out(stk, st - 1, p->data.repeat.f));
                    }
                }

            case MPC_TYPE_MANY1:
                if (st == 0)
                {
                    MPC_CONTINUE(st + 1, p->data.repeat.x);
                }
                if (st > 0)
                {
                    if (mpc_stack_peekr(stk, &r))
                    {
                        MPC_CONTINUE(st + 1, p->data.repeat.x);
                    }
                    else
                    {
                        if (st == 1)
                        {
                            mpc_stack_popr(stk, &r);
                            MPC_FAILURE(mpc_err_many1(r.error));
                        }
                        else
                        {
                            mpc_stack_popr(stk, &r);
                            mpc_stack_err(stk, r.error);
                            MPC_SUCCESS(mpc_stack_merger_out(stk, st - 1, p->data.repeat.f));
                        }
                    }
                }

            case MPC_TYPE_COUNT:
                if (st == 0)
                {
                    mpc_input_mark(i);
                    MPC_CONTINUE(st + 1, p->data.repeat.x);
                }
                if (st > 0)
                {
                    if (mpc_stack_peekr(stk, &r))
                    {
                        MPC_CONTINUE(st + 1, p->data.repeat.x);
                    }
                    else
                    {
                        if (st != (p->data.repeat.n + 1))
                        {
                            mpc_stack_popr(stk, &r);
                            mpc_stack_popr_out_single(stk, st - 1, p->data.repeat.dx);
                            mpc_input_rewind(i);
                            MPC_FAILURE(mpc_err_count(r.error, p->data.repeat.n));
                        }
                        else
                        {
                            mpc_stack_popr(stk, &r);
                            mpc_stack_err(stk, r.error);
                            mpc_input_unmark(i);
                            MPC_SUCCESS(mpc_stack_merger_out(stk, st - 1, p->data.repeat.f));
                        }
                    }
                }

            case MPC_TYPE_OR:
                if (p->data.or.n == 0)
                {
                    MPC_SUCCESS(NULL);
                }
                if (st == 0)
                {
                    MPC_CONTINUE(st + 1, p->data.or.xs[st]);
                }
                if (st <= p->data.or.n)
                {
                    if (mpc_stack_peekr(stk, &r))
                    {
                        mpc_stack_popr(stk, &r);
                        mpc_stack_popr_err(stk, st - 1);
                        MPC_SUCCESS(r.output);
                    }
                    if (st < p->data.or.n)
                    {
                        MPC_CONTINUE(st + 1, p->data.or.xs[st]);
                    }
                    if (st == p->data.or.n)
                    {
                        MPC_FAILURE(mpc_stack_merger_err(stk, p->data.or.n));
                    }
                }

            case MPC_TYPE_AND:
                if (p->data.or.n == 0)
                {
                    MPC_SUCCESS(p->data.and.f(0, NULL));
                }
                if (st == 0)
                {
                    mpc_input_mark(i);
                    MPC_CONTINUE(st + 1, p->data.and.xs[st]);
                }
                if (st <= p->data.and.n)
                {
                    if (!mpc_stack_peekr(stk, &r))
                    {
                        mpc_input_rewind(i);
                        mpc_stack_popr(stk, &r);
                        mpc_stack_popr_out(stk, st - 1, p->data.and.dxs);
                        MPC_FAILURE(r.error);
                    }
                    if (st < p->data.and.n)
                    {
                        MPC_CONTINUE(st + 1, p->data.and.xs[st]);
                    }
                    if (st == p->data.and.n)
                    {
                        mpc_input_unmark(i);
                        MPC_SUCCESS(mpc_stack_merger_out(stk, p->data.and.n, p->data.and.f));
                    }
                }

            /* End */

            default:
                MPC_FAILURE(mpc_err_fail(i->filename, i->state, "Unknown Parser Type Id!"));
        }
    }

    return mpc_stack_terminate(stk, final);
}

#undef MPC_CONTINUE
#undef MPC_SUCCESS
#undef MPC_FAILURE
#undef MPC_PRIMATIVE


int mpc_parse(const char *filename, const char *string, mpc_parser_t *p, mpc_result_t *r)
{
    int x;
    mpc_input_t *i = mpc_input_new_string(filename, string);
    x = mpc_parse_input(i, p, r);
    mpc_input_delete(i);

    return x;
}

int mpc_parse_file(const char *filename, FILE *file, mpc_parser_t *p, mpc_result_t *r)
{
    int x;
    mpc_input_t *i = mpc_input_new_file(filename, file);
    x = mpc_parse_input(i, p, r);
    mpc_input_delete(i);

    return x;
}

int mpc_parse_pipe(const char *filename, FILE *pipe, mpc_parser_t *p, mpc_result_t *r)
{
    int x;
    mpc_input_t *i = mpc_input_new_pipe(filename, pipe);
    x = mpc_parse_input(i, p, r);
    mpc_input_delete(i);

    return x;
}

int mpc_parse_contents(const char *filename, mpc_parser_t *p, mpc_result_t *r)
{
    FILE *f = fopen(filename, "rb");
    int res;

    if (f == NULL)
    {
        r->output = NULL;
        r->error = mpc_err_fail(filename, mpc_state_new(), "Unbale to open file!");

        return 0;
    }

    res = mpc_parse_file(filename, f, p, r);
    fclose(f);

    return res;
}
