/*
 * Unit tests for src/process.c
 * Tests against real /proc entries of the running process and PID 1.
 * No mocking needed — pure filesystem reads.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
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
	/* Should not crash; ppid may be unchanged */
	pusb_get_process_parent_id(-1, &ppid);
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
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
