/*
 * Unit tests for src/process.c
 * Tests against real /proc entries of the running process and PID 1.
 * No mocking needed — pure filesystem reads.
 */

#define _GNU_SOURCE
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <cmocka.h>
#include "../../../src/process.h"
#include "../../../src/conf.h"
#include "../../../src/log.h"

/* ── pusb_get_process_name ── */

static void test_process_name_self(void **state)
{
	(void)state;
	char name[256] = {0};
	pusb_get_process_name(getpid(), name, sizeof(name));
	assert_true(name[0] != '\0');
}

static void test_process_name_pid1(void **state)
{
	(void)state;
	char name[256] = {0};
	pusb_get_process_name(1, name, sizeof(name));
	assert_true(name[0] != '\0');
}

static void test_process_name_invalid_pid(void **state)
{
	(void)state;
	char name[256] = {0};
	/* Should not crash on a non-existent PID; name remains empty */
	pusb_get_process_name(-1, name, sizeof(name));
	/* No assertion on value — just confirm no crash */
}

/* ── pusb_get_process_parent_id ── */

static void test_ppid_self(void **state)
{
	(void)state;
	pid_t ppid = 0;
	pusb_get_process_parent_id(getpid(), &ppid);
	assert_true(ppid > 0);
}

static void test_ppid_init(void **state)
{
	(void)state;
	pid_t ppid = 99;
	pusb_get_process_parent_id(1, &ppid);
	assert_int_equal(0, (int)ppid);
}

static void test_ppid_invalid_pid(void **state)
{
	(void)state;
	pid_t ppid = 42; /* preset sentinel */
	pusb_get_process_parent_id(-1, &ppid);
	assert_int_equal(0, (int)ppid);
}

/* ── /proc/[pid]/stat parsing ── */

static void test_parse_stat_ppid_with_plain_comm(void **state)
{
	(void)state;
	pid_t ppid = 0;
	assert_int_equal(1, pusb_parse_process_stat_parent_id(
		"1234 (sshd) S 1 2 3 4 5 6 7 8 9\n", &ppid));
	assert_int_equal(1, (int)ppid);
}

static void test_parse_stat_ppid_with_spaced_comm(void **state)
{
	(void)state;
	pid_t ppid = 0;
	assert_int_equal(1, pusb_parse_process_stat_parent_id(
		"1234 (remote helper) S 567 2 3 4 5 6 7 8 9\n", &ppid));
	assert_int_equal(567, (int)ppid);
}

static void test_parse_stat_ppid_uses_last_closing_paren(void **state)
{
	(void)state;
	pid_t ppid = 0;
	assert_int_equal(1, pusb_parse_process_stat_parent_id(
		"1234 (helper) name) S 890 2 3 4 5 6 7 8 9\n", &ppid));
	assert_int_equal(890, (int)ppid);
}

static void test_parse_stat_ppid_rejects_malformed_line(void **state)
{
	(void)state;
	pid_t ppid = 42;
	assert_int_equal(0, pusb_parse_process_stat_parent_id(
		"1234 helper S 1 2 3\n", &ppid));
	assert_int_equal(42, (int)ppid);
}

/* ── pusb_get_process_envvar ── */

static void test_envvar_path_found(void **state)
{
	(void)state;
	/* PATH is virtually always set in every process */
	char *val = pusb_get_process_envvar(getpid(), "PATH");
	assert_non_null(val);
	assert_true(strchr(val, '/') != NULL);
	free(val);
}

static void test_envvar_nonexistent(void **state)
{
	(void)state;
	char *val = pusb_get_process_envvar(getpid(), "NONEXISTENT_VAR_PAMUSB_XYZ_12345");
	assert_null(val);
}

static void test_envvar_invalid_pid(void **state)
{
	(void)state;
	char *val = pusb_get_process_envvar(-1, "PATH");
	assert_null(val);
}

static void test_envvar_called_twice_no_state_bleed(void **state)
{
	(void)state;
	/* M-1 regression: old strtok() left global tokeniser state; a second call could
	 * return garbage picked up from the first call's buffer instead of re-scanning. */
	char *path = pusb_get_process_envvar(getpid(), "PATH");
	char *home = pusb_get_process_envvar(getpid(), "HOME");

	assert_non_null(path);
	assert_true(strchr(path, '/') != NULL);

	assert_non_null(home);
	assert_true(home[0] == '/');

	free(path);
	free(home);
}

/* ── pusb_scan_environ_buffer ── */

static void test_scan_buffer_var_at_start(void **state)
{
	(void)state;
	/* Variable is the very first entry in the buffer */
	const char buf[] = "TMUX=/tmp/s,0,1\0OTHER=x";
	char *r = pusb_scan_environ_buffer(buf, sizeof(buf) - 1, "TMUX");
	assert_non_null(r);
	assert_string_equal("/tmp/s,0,1", r);
	free(r);
}

static void test_scan_buffer_var_beyond_bufsiz(void **state)
{
	const size_t cap = BUFSIZ + 4096;
	char *buf;
	size_t pos;
	int n;
	size_t total;
	char *r;

	(void)state;
	/* Build a buffer that pushes PUSB_TEST past the old 8 192-byte (BUFSIZ) limit,
	   verifying the dynamic-allocation fix works for large environ files. */
	buf = malloc(cap); /* DevSkim: ignore DS154189,DS161085 - cap is BUFSIZ+4096, a compile-time constant */
	assert_non_null(buf);
	pos = 0;

	while (pos < (size_t)BUFSIZ + 64)
	{
		n = snprintf(buf + pos, cap - pos, "DUMMY_%zu=value", pos);
		pos += (size_t)n + 1; /* +1 skips past snprintf's NUL (the entry separator) */
	}
	assert_true(pos > (size_t)BUFSIZ);

	n = snprintf(buf + pos, cap - pos, "PUSB_TEST=found_it");
	total = pos + (size_t)n; /* snprintf places NUL at buf[total] — the sentinel */

	r = pusb_scan_environ_buffer(buf, total, "PUSB_TEST");
	assert_non_null(r);
	assert_string_equal("found_it", r);
	free(r);
	free(buf);
}

static void test_scan_buffer_var_not_found(void **state)
{
	(void)state;
	const char buf[] = "FOO=bar\0BAZ=qux";
	char *r = pusb_scan_environ_buffer(buf, sizeof(buf) - 1, "TMUX");
	assert_null(r);
}

static void test_scan_buffer_empty(void **state)
{
	(void)state;
	char *r = pusb_scan_environ_buffer("", 0, "TMUX");
	assert_null(r);
}

/* ── main ── */

int main(void)
{
	t_pusb_options opts = {0};
	opts.debug = 0;
	opts.quiet = 1;
	pusb_log_init(&opts);

	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_process_name_self),
		cmocka_unit_test(test_process_name_pid1),
		cmocka_unit_test(test_process_name_invalid_pid),
		cmocka_unit_test(test_ppid_self),
		cmocka_unit_test(test_ppid_init),
		cmocka_unit_test(test_ppid_invalid_pid),
		cmocka_unit_test(test_parse_stat_ppid_with_plain_comm),
		cmocka_unit_test(test_parse_stat_ppid_with_spaced_comm),
		cmocka_unit_test(test_parse_stat_ppid_uses_last_closing_paren),
		cmocka_unit_test(test_parse_stat_ppid_rejects_malformed_line),
		cmocka_unit_test(test_envvar_path_found),
		cmocka_unit_test(test_envvar_nonexistent),
		cmocka_unit_test(test_envvar_invalid_pid),
		cmocka_unit_test(test_envvar_called_twice_no_state_bleed),
		cmocka_unit_test(test_scan_buffer_var_at_start),
		cmocka_unit_test(test_scan_buffer_var_beyond_bufsiz),
		cmocka_unit_test(test_scan_buffer_var_not_found),
		cmocka_unit_test(test_scan_buffer_empty),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
