/*
 * Automated Testing Framework (atf)
 *
 * Copyright (c) 2008, 2009, 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "atf-c/env.h"
#include "atf-c/error.h"
#include "atf-c/fs.h"
#include "atf-c/sanity.h"
#include "atf-c/tc.h"
#include "atf-c/text.h"

/* ---------------------------------------------------------------------
 * Auxiliary functions.
 * --------------------------------------------------------------------- */

struct context {
    const atf_tc_t *tc;
    const atf_fs_path_t *resfile;
    size_t fail_count;
};

static void
context_init(struct context *ctx, const atf_tc_t *tc,
             const atf_fs_path_t *resfile)
{
    ctx->tc = tc;
    ctx->resfile = resfile;
    ctx->fail_count = 0;
}

static void
check_fatal_error(atf_error_t err)
{
    if (atf_is_error(err)) {
        char buf[1024];
        atf_error_format(err, buf, sizeof(buf));
        fprintf(stderr, "FATAL ERROR: %s\n", buf);
        atf_error_free(err);
        abort();
    }
}

static void
report_fatal_error(const char *msg, ...)
{
    va_list ap;
    fprintf(stderr, "FATAL ERROR: ");

    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);

    fprintf(stderr, "\n");
    abort();
}

/** Writes to a results file.
 *
 * The results file is supposed to be already open.
 *
 * This function returns an error code instead of exiting in case of error
 * because the caller needs to clean up the reason object before terminating.
 */
static atf_error_t
write_resfile(FILE *file, const char *result, const atf_dynstr_t *reason)
{
    if (reason == NULL) {
        if (fprintf(file, "%s\n", result) <= 0)
            goto err;
    } else {
        if (fprintf(file, "%s: %s\n", result, atf_dynstr_cstring(reason)) <= 0)
            goto err;
    }
    return atf_no_error();

err:
    return atf_libc_error(errno, "Failed to write results file; result %s, "
        "reason %s", result,
        reason == NULL ? "null" : atf_dynstr_cstring(reason));
}

/** Creates a results file.
 *
 * The input reason is released in all cases.
 *
 * An error in this function is considered to be fatal, hence why it does
 * not return any error code.
 */
static void
create_resfile(const atf_fs_path_t *resfile, const char *result,
               atf_dynstr_t *reason)
{
    atf_error_t err;

    if (strcmp("/dev/stdout", atf_fs_path_cstring(resfile)) == 0) {
        err = write_resfile(stdout, result, reason);
    } else if (strcmp("/dev/stderr", atf_fs_path_cstring(resfile)) == 0) {
        err = write_resfile(stderr, result, reason);
    } else {
        FILE *file = fopen(atf_fs_path_cstring(resfile), "w");
        if (file == NULL) {
            err = atf_libc_error(errno, "Cannot create results file '%s'",
                                 atf_fs_path_cstring(resfile));
        } else {
            err = write_resfile(file, result, reason);
            fclose(file);
        }
    }

    if (reason != NULL)
        atf_dynstr_fini(reason);

    check_fatal_error(err);
}

static void
fail_requirement(struct context *ctx, atf_dynstr_t *reason)
{
    create_resfile(ctx->resfile, "failed", reason);
    exit(EXIT_FAILURE);
}

static void
fail_check(struct context *ctx, atf_dynstr_t *reason)
{
    fprintf(stderr, "*** Check failed: %s\n", atf_dynstr_cstring(reason));
    atf_dynstr_fini(reason);

    ctx->fail_count++;
}

static void
pass(struct context *ctx)
{
    create_resfile(ctx->resfile, "passed", NULL);
    exit(EXIT_SUCCESS);
}

static void
skip(struct context *ctx, atf_dynstr_t *reason)
{
    create_resfile(ctx->resfile, "skipped", reason);
    exit(EXIT_SUCCESS);
}

/** Formats a failure/skip reason message.
 *
 * The formatted reason is stored in out_reason.  out_reason is initialized
 * in this function and is supposed to be released by the caller.  In general,
 * the reason will eventually be fed to create_resfile, which will release
 * it.
 *
 * Errors in this function are fatal.  Rationale being: reasons are used to
 * create results files; if we can't format the reason correctly, the result
 * of the test program will be bogus.  So it's better to just exit with a
 * fatal error.
 */
static void
format_reason_ap(atf_dynstr_t *out_reason,
                 const char *source_file, const size_t source_line,
                 const char *reason, va_list ap)
{
    atf_error_t err;

    if (source_file != NULL) {
        err = atf_dynstr_init_fmt(out_reason, "%s:%zd: ", source_file,
                                  source_line);
    } else {
        PRE(source_line == 0);
        err = atf_dynstr_init(out_reason);
    }

    if (!atf_is_error(err)) {
        va_list ap2;
        va_copy(ap2, ap);
        err = atf_dynstr_append_ap(out_reason, reason, ap2);
        va_end(ap2);
    }

    check_fatal_error(err);
}

static void
format_reason_fmt(atf_dynstr_t *out_reason,
                  const char *source_file, const size_t source_line,
                  const char *reason, ...)
{
    va_list ap;

    va_start(ap, reason);
    format_reason_ap(out_reason, source_file, source_line, reason, ap);
    va_end(ap);
}

static void
errno_test(struct context *ctx, const char *file, const size_t line,
           const int exp_errno, const char *expr_str,
           const bool expr_result,
           void (*fail_func)(struct context *ctx, atf_dynstr_t *))
{
    const int actual_errno = errno;

    if (expr_result) {
        if (exp_errno != actual_errno) {
            atf_dynstr_t reason;

            format_reason_fmt(&reason, file, line, "Expected errno %d, got %d, "
                "in %s", exp_errno, actual_errno, expr_str);
            fail_func(ctx, &reason);
        }
    } else {
        atf_dynstr_t reason;

        format_reason_fmt(&reason, file, line, "Expected true value in %s",
            expr_str);
        fail_func(ctx, &reason);
    }
}

struct prog_found_pair {
    const char *prog;
    bool found;
};

static atf_error_t
check_prog_in_dir(const char *dir, void *data)
{
    struct prog_found_pair *pf = data;
    atf_error_t err;

    if (pf->found)
        err = atf_no_error();
    else {
        atf_fs_path_t p;

        err = atf_fs_path_init_fmt(&p, "%s/%s", dir, pf->prog);
        if (atf_is_error(err))
            goto out_p;

        err = atf_fs_eaccess(&p, atf_fs_access_x);
        if (!atf_is_error(err))
            pf->found = true;
        else {
            atf_error_free(err);
            INV(!pf->found);
            err = atf_no_error();
        }

out_p:
        atf_fs_path_fini(&p);
    }

    return err;
}

static atf_error_t
check_prog(struct context *ctx, const char *prog, void *data)
{
    atf_error_t err;
    atf_fs_path_t p;

    err = atf_fs_path_init_fmt(&p, "%s", prog);
    if (atf_is_error(err))
        goto out;

    if (atf_fs_path_is_absolute(&p)) {
        err = atf_fs_eaccess(&p, atf_fs_access_x);
        if (atf_is_error(err)) {
            atf_dynstr_t reason;

            atf_error_free(err);
            atf_fs_path_fini(&p);
            format_reason_fmt(&reason, NULL, 0, "The required program %s could "
                "not be found", prog);
            skip(ctx, &reason);
        }
    } else {
        const char *path = atf_env_get("PATH");
        struct prog_found_pair pf;
        atf_fs_path_t bp;

        err = atf_fs_path_branch_path(&p, &bp);
        if (atf_is_error(err))
            goto out_p;

        if (strcmp(atf_fs_path_cstring(&bp), ".") != 0) {
            atf_fs_path_fini(&bp);
            atf_fs_path_fini(&p);

            report_fatal_error("Relative paths are not allowed when searching "
                "for a program (%s)", prog);
            UNREACHABLE;
        }

        pf.prog = prog;
        pf.found = false;
        err = atf_text_for_each_word(path, ":", check_prog_in_dir, &pf);
        if (atf_is_error(err))
            goto out_bp;

        if (!pf.found) {
            atf_dynstr_t reason;

            atf_fs_path_fini(&bp);
            atf_fs_path_fini(&p);
            format_reason_fmt(&reason, NULL, 0, "The required program %s could "
                "not be found in the PATH", prog);
            fail_requirement(ctx, &reason);
        }

out_bp:
        atf_fs_path_fini(&bp);
    }

out_p:
    atf_fs_path_fini(&p);
out:
    return err;
}

/* ---------------------------------------------------------------------
 * The "atf_tc" type.
 * --------------------------------------------------------------------- */

/*
 * Constructors/destructors.
 */

atf_error_t
atf_tc_init(atf_tc_t *tc, const char *ident, atf_tc_head_t head,
            atf_tc_body_t body, atf_tc_cleanup_t cleanup,
            const atf_map_t *config)
{
    atf_error_t err;

    tc->m_ident = ident;
    tc->m_head = head;
    tc->m_body = body;
    tc->m_cleanup = cleanup;
    tc->m_config = config;

    err = atf_map_init(&tc->m_vars);
    if (atf_is_error(err))
        goto err;

    err = atf_tc_set_md_var(tc, "ident", ident);
    if (atf_is_error(err))
        goto err_map;

    if (cleanup != NULL) {
        err = atf_tc_set_md_var(tc, "has.cleanup", "true");
        if (atf_is_error(err))
            goto err_map;
    }

    /* XXX Should the head be able to return error codes? */
    if (tc->m_head != NULL)
        tc->m_head(tc);

    if (strcmp(atf_tc_get_md_var(tc, "ident"), ident) != 0) {
        report_fatal_error("Test case head modified the read-only 'ident' "
            "property");
        UNREACHABLE;
    }

    INV(!atf_is_error(err));
    return err;

err_map:
    atf_map_fini(&tc->m_vars);
err:
    return err;
}

atf_error_t
atf_tc_init_pack(atf_tc_t *tc, const atf_tc_pack_t *pack,
                 const atf_map_t *config)
{
    return atf_tc_init(tc, pack->m_ident, pack->m_head, pack->m_body,
                       pack->m_cleanup, config);
}

void
atf_tc_fini(atf_tc_t *tc)
{
    atf_map_fini(&tc->m_vars);
}

/*
 * Getters.
 */

const char *
atf_tc_get_ident(const atf_tc_t *tc)
{
    return tc->m_ident;
}

const char *
atf_tc_get_config_var(const atf_tc_t *tc, const char *name)
{
    const char *val;
    atf_map_citer_t iter;

    PRE(atf_tc_has_config_var(tc, name));
    iter = atf_map_find_c(tc->m_config, name);
    val = atf_map_citer_data(iter);
    INV(val != NULL);

    return val;
}

const char *
atf_tc_get_config_var_wd(const atf_tc_t *tc, const char *name,
                         const char *defval)
{
    const char *val;

    if (!atf_tc_has_config_var(tc, name))
        val = defval;
    else
        val = atf_tc_get_config_var(tc, name);

    return val;
}

const char *
atf_tc_get_md_var(const atf_tc_t *tc, const char *name)
{
    const char *val;
    atf_map_citer_t iter;

    PRE(atf_tc_has_md_var(tc, name));
    iter = atf_map_find_c(&tc->m_vars, name);
    val = atf_map_citer_data(iter);
    INV(val != NULL);

    return val;
}

const atf_map_t *
atf_tc_get_md_vars(const atf_tc_t *tc)
{
    return &tc->m_vars;
}

bool
atf_tc_has_config_var(const atf_tc_t *tc, const char *name)
{
    bool found;
    atf_map_citer_t end, iter;

    if (tc->m_config == NULL)
        found = false;
    else {
        iter = atf_map_find_c(tc->m_config, name);
        end = atf_map_end_c(tc->m_config);
        found = !atf_equal_map_citer_map_citer(iter, end);
    }

    return found;
}

bool
atf_tc_has_md_var(const atf_tc_t *tc, const char *name)
{
    atf_map_citer_t end, iter;

    iter = atf_map_find_c(&tc->m_vars, name);
    end = atf_map_end_c(&tc->m_vars);
    return !atf_equal_map_citer_map_citer(iter, end);
}

/*
 * Modifiers.
 */

atf_error_t
atf_tc_set_md_var(atf_tc_t *tc, const char *name, const char *fmt, ...)
{
    atf_error_t err;
    char *value;
    va_list ap;

    va_start(ap, fmt);
    err = atf_text_format_ap(&value, fmt, ap);
    va_end(ap);

    if (!atf_is_error(err))
        err = atf_map_insert(&tc->m_vars, name, value, true);
    else
        free(value);

    return err;
}

/* ---------------------------------------------------------------------
 * Free functions, as they should be publicly but they can't.
 * --------------------------------------------------------------------- */

static void
_atf_tc_fail(struct context *ctx, const char *fmt, va_list ap)
{
    va_list ap2;
    atf_dynstr_t reason;

    va_copy(ap2, ap);
    format_reason_ap(&reason, NULL, 0, fmt, ap2);
    va_end(ap2);

    fail_requirement(ctx, &reason);
    UNREACHABLE;
}

static void
_atf_tc_fail_nonfatal(struct context *ctx, const char *fmt, va_list ap)
{
    va_list ap2;
    atf_dynstr_t reason;

    va_copy(ap2, ap);
    format_reason_ap(&reason, NULL, 0, fmt, ap2);
    va_end(ap2);

    fail_check(ctx, &reason);
}

static void
_atf_tc_fail_check(struct context *ctx, const char *file, const size_t line,
                   const char *fmt, va_list ap)
{
    va_list ap2;
    atf_dynstr_t reason;

    va_copy(ap2, ap);
    format_reason_ap(&reason, file, line, fmt, ap2);
    va_end(ap2);

    fail_check(ctx, &reason);
}

static void
_atf_tc_fail_requirement(struct context *ctx, const char *file,
                         const size_t line, const char *fmt, va_list ap)
{
    va_list ap2;
    atf_dynstr_t reason;

    va_copy(ap2, ap);
    format_reason_ap(&reason, file, line, fmt, ap2);
    va_end(ap2);

    fail_requirement(ctx, &reason);
    UNREACHABLE;
}

static void
_atf_tc_pass(struct context *ctx)
{
    pass(ctx);
    UNREACHABLE;
}

static void
_atf_tc_require_prog(struct context *ctx, const char *prog)
{
    check_fatal_error(check_prog(ctx, prog, NULL));
}

static void
_atf_tc_skip(struct context *ctx, const char *fmt, va_list ap)
{
    atf_dynstr_t reason;
    va_list ap2;

    va_copy(ap2, ap);
    format_reason_ap(&reason, NULL, 0, fmt, ap2);
    va_end(ap2);

    skip(ctx, &reason);
}

static void
_atf_tc_check_errno(struct context *ctx, const char *file, const size_t line,
                    const int exp_errno, const char *expr_str,
                    const bool expr_result)
{
    errno_test(ctx, file, line, exp_errno, expr_str, expr_result, fail_check);
}

static void
_atf_tc_require_errno(struct context *ctx, const char *file, const size_t line,
                      const int exp_errno, const char *expr_str,
                      const bool expr_result)
{
    errno_test(ctx, file, line, exp_errno, expr_str, expr_result,
        fail_requirement);
}

/* ---------------------------------------------------------------------
 * Free functions.
 * --------------------------------------------------------------------- */

static struct context Current;

atf_error_t
atf_tc_run(const atf_tc_t *tc, const atf_fs_path_t *resfile)
{
    context_init(&Current, tc, resfile);

    tc->m_body(tc);

    if (Current.fail_count == 0) {
        pass(&Current);
    } else {
        atf_dynstr_t reason;

        format_reason_fmt(&reason, NULL, 0, "%d checks failed; see output for "
            "more details", Current.fail_count);
        fail_requirement(&Current, &reason);
    }

    return atf_no_error();
}

atf_error_t
atf_tc_cleanup(const atf_tc_t *tc)
{
    if (tc->m_cleanup != NULL)
        tc->m_cleanup(tc);
    return atf_no_error(); /* XXX */
}

/* ---------------------------------------------------------------------
 * Free functions that depend on Current.
 * --------------------------------------------------------------------- */

/*
 * All the functions below provide delegates to other internal functions
 * (prefixed by _) that take the current test case as an argument to
 * prevent them from accessing global state.  This is to keep the side-
 * effects of the internal functions clearer and easier to understand.
 *
 * The public API should never have hid the fact that it needs access to
 * the current test case (other than maybe in the macros), but changing it
 * is hard.  TODO: Revisit in the future.
 */

void
atf_tc_fail(const char *fmt, ...)
{
    va_list ap;

    PRE(Current.tc != NULL);

    va_start(ap, fmt);
    _atf_tc_fail(&Current, fmt, ap);
    va_end(ap);
}

void
atf_tc_fail_nonfatal(const char *fmt, ...)
{
    va_list ap;

    PRE(Current.tc != NULL);

    va_start(ap, fmt);
    _atf_tc_fail_nonfatal(&Current, fmt, ap);
    va_end(ap);
}

void
atf_tc_fail_check(const char *file, const size_t line, const char *fmt, ...)
{
    va_list ap;

    PRE(Current.tc != NULL);

    va_start(ap, fmt);
    _atf_tc_fail_check(&Current, file, line, fmt, ap);
    va_end(ap);
}

void
atf_tc_fail_requirement(const char *file, const size_t line,
                        const char *fmt, ...)
{
    va_list ap;

    PRE(Current.tc != NULL);

    va_start(ap, fmt);
    _atf_tc_fail_requirement(&Current, file, line, fmt, ap);
    va_end(ap);
}

void
atf_tc_pass(void)
{
    PRE(Current.tc != NULL);

    _atf_tc_pass(&Current);
}

void
atf_tc_require_prog(const char *prog)
{
    PRE(Current.tc != NULL);

    _atf_tc_require_prog(&Current, prog);
}

void
atf_tc_skip(const char *fmt, ...)
{
    va_list ap;

    PRE(Current.tc != NULL);

    va_start(ap, fmt);
    _atf_tc_skip(&Current, fmt, ap);
    va_end(ap);
}

void
atf_tc_check_errno(const char *file, const size_t line, const int exp_errno,
                   const char *expr_str, const bool expr_result)
{
    PRE(Current.tc != NULL);

    _atf_tc_check_errno(&Current, file, line, exp_errno, expr_str,
                        expr_result);
}

void
atf_tc_require_errno(const char *file, const size_t line, const int exp_errno,
                     const char *expr_str, const bool expr_result)
{
    PRE(Current.tc != NULL);

    _atf_tc_require_errno(&Current, file, line, exp_errno, expr_str,
                          expr_result);
}
