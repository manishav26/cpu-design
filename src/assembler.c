/*
 * assembler.c -- two-pass assembler for SIMPLE-16.
 * Pass 1: addresses and labels.
 */
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "assembler.h"

#define MAX_LABELS    64
#define MAX_OPERANDS  3

/* A label and its address */
typedef struct {
    char name[MAX_LABEL];
    int  addr;
} Label;

/* A statement saved by pass 1 for pass 2 */
typedef struct {
    int  lineno;
    char mnemonic[16];
    char operands[MAX_OPERANDS][MAX_LINE];
    int  num_operands;
} Statement;

/* Write an error message and return -1. */
static int fail(char *err, size_t errlen, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err, errlen, fmt, ap);
    va_end(ap);
    return -1;
}

/* Remove leading and trailing spaces. */
static char *trim(char *s)
{
    while (isspace((unsigned char)*s))
        s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1]))
        *--end = '\0';
    return s;
}

/* Safe string copy. */
static void copy(char *dst, size_t size, const char *src)
{
    snprintf(dst, size, "%s", src);
}

/* Label name -> address, or -1 if not found. */
static int find_label(const Label *labels, int n, const char *name)
{
    for (int i = 0; i < n; i++)
        if (strcmp(labels[i].name, name) == 0)
            return labels[i].addr;
    return -1;
}

/* Assemble a source file into a memory image and listing. */
int assemble_file(const char *path, Program *prog, char *err, size_t errlen)
{
    FILE *f = fopen(path, "r");
    if (f == NULL)
        return fail(err, errlen, "cannot open '%s'", path);

    memset(prog, 0, sizeof *prog);
    static Statement stmts[MEM_SIZE];
    Label  labels[MAX_LABELS];
    int    nlabels = 0;
    int    in_data = 0;                             /* 0 = text, 1 = data */
    int    next[2]  = { TEXT_START, DATA_START };    /* next free address */
    int    limit[2] = { TEXT_END, DATA_END };        /* segment end       */
    int    lineno = 0;
    char   line[MAX_LINE];

    /*  pass 1: addresses and labels */
    while (fgets(line, sizeof line, f) != NULL) {
        lineno++;
        line[strcspn(line, "\r\n")] = '\0';

        /* split off the comment */
        char comment[MAX_LINE] = "";
        char *semi = strchr(line, ';');
        if (semi != NULL) {
            copy(comment, sizeof comment, trim(semi + 1));
            *semi = '\0';
        }
        char *text = trim(line);
        if (*text == '\0')
            continue;                               /* blank line */

        /* segment directives */
        if (strcmp(text, ".text") == 0 || strcmp(text, ".TEXT") == 0) { in_data = 0; continue; }
        if (strcmp(text, ".data") == 0 || strcmp(text, ".DATA") == 0) { in_data = 1; continue; }

        /* optional "label:" -> address of this statement */
        char label[MAX_LABEL] = "";
        char *colon = strchr(text, ':');
        if (colon != NULL) {
            *colon = '\0';
            char *name = trim(text);
            if (*name == '\0' || strlen(name) >= MAX_LABEL || strchr(name, ' ') != NULL) {
                fclose(f);
                return fail(err, errlen, "line %d: bad label '%s'", lineno, name);
            }
            if (find_label(labels, nlabels, name) >= 0) {
                fclose(f);
                return fail(err, errlen, "line %d: duplicate label '%s'", lineno, name);
            }
            if (nlabels == MAX_LABELS) {
                fclose(f);
                return fail(err, errlen, "line %d: too many labels", lineno);
            }
            copy(labels[nlabels].name, MAX_LABEL, name);
            labels[nlabels].addr = next[in_data];
            nlabels++;
            copy(label, sizeof label, name);
            text = trim(colon + 1);
            if (*text == '\0')
                continue;                           /* label on its own line */
        }
        /* next address in the current segment */
        int addr = next[in_data];
        if (addr > limit[in_data]) {
            fclose(f);
            return fail(err, errlen, "line %d: %s segment is full (0x%02X-0x%02X)", lineno,
                        in_data ? "data" : "text", in_data ? DATA_START : TEXT_START, limit[in_data]);
        }
        next[in_data]++;
        /* save the statement for the listing and pass 2 */
        ListingEntry *e  = &prog->listing[prog->count];
        Statement    *st = &stmts[prog->count];
        prog->count++;
        memset(st, 0, sizeof *st);
        e->addr = addr;
        copy(e->text, sizeof e->text, text);
        copy(e->label, sizeof e->label, label);
        copy(e->comment, sizeof e->comment, comment);
        st->lineno = lineno;
        /* mnemonic (upper-case) and comma-separated operands */
        size_t mlen = strcspn(text, " \t");
        if (mlen >= sizeof st->mnemonic) {
            fclose(f);
            return fail(err, errlen, "line %d: unknown instruction '%s'", lineno, text);
        }
        for (size_t i = 0; i < mlen; i++)
            st->mnemonic[i] = (char)toupper((unsigned char)text[i]);
        char *rest = trim(text + mlen);
        if (*rest != '\0') {
            for (char *tok = strtok(rest, ","); tok != NULL; tok = strtok(NULL, ",")) {
                if (st->num_operands == MAX_OPERANDS) {
                    fclose(f);
                    return fail(err, errlen, "line %d: too many operands", lineno);
                }
                copy(st->operands[st->num_operands++], MAX_LINE, trim(tok));
            }
        }
    }
    fclose(f);
    return 0;
}