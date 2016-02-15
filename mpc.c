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
