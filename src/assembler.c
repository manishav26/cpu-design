/*
 * assembler.c -- two-pass assembler for SIMPLE-16.
 * Pass 1: addresses and labels.  Pass 2: encode each statement.
 */
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
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

/* "R0".."R3" -> 0..3, otherwise -1. */
static int parse_reg(const char *raw)
{
    char tok[MAX_LINE];
    copy(tok, sizeof tok, raw);
    char *t = trim(tok);
    if ((t[0] == 'R' || t[0] == 'r') && t[1] >= '0' && t[1] < '0' + NUM_REGS && t[2] == '\0')
        return t[1] - '0';
    return -1;
}

/* Number (12, -1, 0x1F) or label -> value. */
static int parse_value(const char *raw, const Label *labels, int nlabels, long *out)
{
    char tok[MAX_LINE];
    copy(tok, sizeof tok, raw);
    char *t = trim(tok);
    if (*t == '\0')
        return -1;
    int addr = find_label(labels, nlabels, t);
    if (addr >= 0) {
        *out = addr;
        return 0;
    }
    char *end;
    errno = 0;
    long v = strtol(t, &end, 0);                     /* decimal or 0x hex */
    if (errno != 0 || *trim(end) != '\0')
        return -1;
    *out = v;
    return 0;
}

/* [addr] -> M = 0 (direct),  [Rs] / [Rs+n] -> M = 1 (register + offset). */
static int parse_mem(const char *raw, const Label *labels, int nlabels,
                     int *mode, int *rs, int *imm, int lineno, char *err, size_t errlen)
{
    char tok[MAX_LINE];
    copy(tok, sizeof tok, raw);
    char *t = trim(tok);
    if (*t == '[') {                                 /* strip the brackets */
        char *close = strrchr(t, ']');
        if (close == NULL || *trim(close + 1) != '\0')
            return fail(err, errlen, "line %d: missing ']' in '%s'", lineno, raw);
        *close = '\0';
        t = trim(t + 1);
    }

    /* register + optional offset */
    if ((t[0] == 'R' || t[0] == 'r') && t[1] >= '0' && t[1] < '0' + NUM_REGS &&
        (t[2] == '\0' || t[2] == '+' || isspace((unsigned char)t[2]))) {
        *rs = t[1] - '0';
        *mode = 1;
        char *rest = trim(t + 2);
        long off = 0;
        if (*rest != '\0') {
            if (*rest != '+' || parse_value(rest + 1, labels, nlabels, &off) != 0)
                return fail(err, errlen, "line %d: bad offset in '%s' (use [Rs+n])", lineno, raw);
            if (off < 0 || off > 0x7F)
                return fail(err, errlen, "line %d: offset %ld must be 0..127", lineno, off);
        }
        *imm = (int)off;
        return 0;
    }

    /* direct address (must fit in 7 bits) */
    long addr;
    if (parse_value(t, labels, nlabels, &addr) != 0)
        return fail(err, errlen, "line %d: unknown label or bad address '%s'", lineno, raw);
    if (addr < 0 || addr > DIRECT_MAX)
        return fail(err, errlen, "line %d: direct address 0x%lX is above 0x%02X; use [Rs+offset]",
                    lineno, addr, DIRECT_MAX);
    *mode = 0;
    *rs = 0;
    *imm = (int)addr;
    return 0;
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

    /*pass 1: addresses and labels*/
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

    /*  pass 2: encode each statement  */
    for (int i = 0; i < prog->count; i++) {
        Statement    *st = &stmts[i];
        ListingEntry *e  = &prog->listing[i];
        long value;

        /* .word: a data value */
        if (strcmp(st->mnemonic, ".WORD") == 0) {
            if (st->num_operands != 1)
                return fail(err, errlen, "line %d: .word takes one value", st->lineno);
            if (parse_value(st->operands[0], labels, nlabels, &value) != 0)
                return fail(err, errlen, "line %d: bad value '%s'", st->lineno, st->operands[0]);
            e->word = (uint16_t)(value & 0xFFFF);
            e->is_data = 1;
            prog->mem[e->addr] = e->word;
            continue;
        }

        /* look up the instruction, check the operand count */
        const OpInfo *op = op_by_name(st->mnemonic);
        if (op == NULL)
            return fail(err, errlen, "line %d: unknown instruction '%s'", st->lineno, st->mnemonic);
        if (st->num_operands != op->nops)
            return fail(err, errlen, "line %d: %s needs %d operand(s)", st->lineno, op->name, op->nops);

        /* first operand is Rd (except jumps) */
        int rd = 0, rs = 0, mode = 0, imm = 0;
        if (op->nops >= 1 && op->fmt != FMT_J) {
            rd = parse_reg(st->operands[0]);
            if (rd < 0)
                return fail(err, errlen, "line %d: expected register R0..R3, got '%s'",
                            st->lineno, st->operands[0]);
        }

        /* the rest depends on the format */
        switch (op->fmt) {
        case FMT_R:                                  /* second register */
            rs = parse_reg(st->operands[1]);
            if (rs < 0)
                return fail(err, errlen, "line %d: expected register R0..R3, got '%s'",
                            st->lineno, st->operands[1]);
            break;
        case FMT_I:                                  /* 8-bit constant */
            if (parse_value(st->operands[1], labels, nlabels, &value) != 0)
                return fail(err, errlen, "line %d: bad number '%s'", st->lineno, st->operands[1]);
            if (op->opcode == OP_ADDI ? (value < -128 || value > 127) : (value < 0 || value > 255))
                return fail(err, errlen, "line %d: value %ld out of range for %s (%s)", st->lineno,
                            value, op->name, op->opcode == OP_ADDI ? "-128..127" : "0..255");
            imm = (int)value;
            break;
        case FMT_M:                                  /* memory operand */
            if (parse_mem(st->operands[1], labels, nlabels, &mode, &rs, &imm,
                          st->lineno, err, errlen) != 0)
                return -1;
            break;
        case FMT_J:                                  /* jump target */
            if (parse_value(st->operands[0], labels, nlabels, &value) != 0)
                return fail(err, errlen, "line %d: unknown label or bad address '%s'",
                            st->lineno, st->operands[0]);
            if (value < 0 || value > 0xFF)
                return fail(err, errlen, "line %d: jump target %ld outside 0..255", st->lineno, value);
            imm = (int)value;
            break;
        default:                                     /* N, S: no more operands */
            break;
        }

        /* build the 16-bit word and place it in memory */
        e->word = encode(op, rd, rs, mode, imm);
        prog->mem[e->addr] = e->word;
    }
    return 0;
}