/*
 * run.h -- small helpers shared by the black-box C tests.
 * They run the real ./build/simple16 binary (like a grader would)
 * and capture its text output.
 */
#ifndef RUN_H
#define RUN_H
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

/* Run a shell command; return its output (caller frees) and exit code. */
static char *run(const char *cmd, int *status)
{
    FILE *p = popen(cmd, "r");
    assert(p);
    size_t cap = 1 << 16, len = 0;
    char *buf = malloc(cap);
    assert(buf);
    size_t n;
    while ((n = fread(buf + len, 1, cap - len - 1, p)) > 0) {
        len += n;
        if (cap - len < 4096) { cap *= 2; buf = realloc(buf, cap); assert(buf); }
    }
    buf[len] = '\0';
    int rc = pclose(p);
    if (status) *status = WIFEXITED(rc) ? WEXITSTATUS(rc) : -1;
    return buf;
}

/* Read a whole text file (caller frees). */
static char *read_file(const char *path)
{
    FILE *f = fopen(path, "r");
    assert(f);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *buf = malloc((size_t)size + 1);
    assert(buf && fread(buf, 1, (size_t)size, f) == (size_t)size);
    buf[size] = '\0';
    fclose(f);
    return buf;
}

/* Write text to a fresh temporary .asm file; the path goes into out. */
static void write_temp(char out[64], const char *text)
{
    strcpy(out, "/tmp/simple16_testXXXXXX");
    int fd = mkstemp(out);
    assert(fd >= 0);
    FILE *f = fdopen(fd, "w");
    assert(f);
    fputs(text, f);
    fclose(f);
}

/* Copy of src with the first `from` replaced by `to` (caller frees). */
static char *replace(const char *src, const char *from, const char *to)
{
    const char *at = strstr(src, from);
    assert(at);
    size_t a = (size_t)(at - src), fl = strlen(from), tl = strlen(to);
    char *out = malloc(strlen(src) - fl + tl + 1);
    assert(out);
    memcpy(out, src, a);
    memcpy(out + a, to, tl);
    strcpy(out + a + tl, at + fl);
    return out;
}

/* Split a trace into SNAPSHOT blocks (pointers into text, which is modified). */
static int split_snapshots(char *text, char **blocks, int max)
{
    int count = 0;
    char *p = strstr(text, "SNAPSHOT ");
    while (p && count < max) {
        blocks[count++] = p;
        char *next = strstr(p + 9, "SNAPSHOT ");
        if (next) next[-1] = '\0';
        p = next;
    }
    return count;
}
#endif