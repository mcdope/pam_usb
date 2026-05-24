/*
 * Unit tests for src/local.c.
 * Include the source directly to cover static helpers used with fixed-size
 * utmpx fields.
 */

#define _GNU_SOURCE
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <cmocka.h>

/* Include source directly to access static helper functions. */
#include "../../../src/local.c"

/* popen/pclose wrapped at link time with --wrap=popen,--wrap=pclose */
static const char *g_popen_output = "";
static char g_last_popen_cmd[BUFSIZ];

FILE *__wrap_popen(const char *cmd, const char *type)
{
	snprintf(g_last_popen_cmd, sizeof(g_last_popen_cmd), "%s", cmd);
	return fmemopen((void *)g_popen_output, strlen(g_popen_output), type); /* DevSkim: ignore DS154189 - g_popen_output is always a null-terminated string literal assigned in test code */
}

int __wrap_pclose(FILE *fp)
{
	fclose(fp);
	return 0;
}

static void test_utmpx_field_equals_exact_nul_padded(void **state)
{
	(void)state;
	char field[8] = {0};
	snprintf(field, sizeof(field), "%s", "alice");

	assert_int_equal(1, pusb_utmpx_field_equals(field, sizeof(field), "alice"));
}

static void test_utmpx_field_equals_rejects_null_input(void **state)
{
	(void)state;
	char field[8] = {0};
	snprintf(field, sizeof(field), "%s", "alice");

	assert_int_equal(0, pusb_utmpx_field_equals(NULL, sizeof(field), "alice"));
	assert_int_equal(0, pusb_utmpx_field_equals(field, sizeof(field), NULL));
}

static void test_utmpx_field_equals_rejects_prefix_match(void **state)
{
	(void)state;
	char field[8] = {0};
	snprintf(field, sizeof(field), "%s", "alice2");

	assert_int_equal(0, pusb_utmpx_field_equals(field, sizeof(field), "alice"));
}

static void test_utmpx_field_equals_rejects_value_longer_than_field(void **state)
{
	(void)state;
	char field[5] = {'a', 'l', 'i', 'c', 'e'};

	assert_int_equal(0, pusb_utmpx_field_equals(field, sizeof(field), "alice2"));
}

static void test_utmpx_field_equals_full_width_field(void **state)
{
	(void)state;
	char field[5] = {'a', 'l', 'i', 'c', 'e'};

	assert_int_equal(1, pusb_utmpx_field_equals(field, sizeof(field), "alice"));
	assert_int_equal(0, pusb_utmpx_field_equals(field, sizeof(field), "ali"));
}

static void test_utmpx_field_starts_with_tty_number(void **state)
{
	(void)state;
	char field[8] = {0};
	snprintf(field, sizeof(field), "%s", "tty1");

	assert_int_equal(1, pusb_utmpx_field_starts_with(field, sizeof(field), "tty"));
}

static void test_utmpx_field_starts_with_rejects_null_input(void **state)
{
	(void)state;
	char field[8] = {0};
	snprintf(field, sizeof(field), "%s", "tty1");

	assert_int_equal(0, pusb_utmpx_field_starts_with(NULL, sizeof(field), "tty"));
	assert_int_equal(0, pusb_utmpx_field_starts_with(field, sizeof(field), NULL));
}

static void test_utmpx_field_starts_with_pts_slave(void **state)
{
	(void)state;
	char field[8] = {0};
	snprintf(field, sizeof(field), "%s", "pts/0");

	assert_int_equal(1, pusb_utmpx_field_starts_with(field, sizeof(field), "pts"));
}

static void test_utmpx_field_starts_with_full_width_field(void **state)
{
	(void)state;
	char field[3] = {'t', 't', 'y'};

	assert_int_equal(1, pusb_utmpx_field_starts_with(field, sizeof(field), "tty"));
}

static void test_utmpx_field_starts_with_rejects_long_prefix(void **state)
{
	(void)state;
	char field[3] = {'p', 't', 's'};

	assert_int_equal(0, pusb_utmpx_field_starts_with(field, sizeof(field), "pts/"));
}

static void test_loginctl_parse_output_newline_only(void **state)
{
	(void)state;
	char buf[] = "\n";
	assert_null(pusb_loginctl_parse_output(buf));
}

static void test_loginctl_parse_output_empty(void **state)
{
	(void)state;
	char buf[] = "";
	assert_null(pusb_loginctl_parse_output(buf));
}

static void test_loginctl_parse_output_valid_with_newline(void **state)
{
	(void)state;
	char buf[] = "no\n";
	const char *result = pusb_loginctl_parse_output(buf);
	assert_non_null(result);
	assert_string_equal("no", result);
}

static void test_loginctl_parse_output_valid_without_newline(void **state)
{
	(void)state;
	char buf[] = "yes";
	const char *result = pusb_loginctl_parse_output(buf);
	assert_non_null(result);
	assert_string_equal("yes", result);
}

static void test_local_login_denies_xrdp_session(void **state)
{
	(void)state;
	t_pusb_options opts;
	memset(&opts, 0, sizeof(opts));
	opts.deny_remote = 1;

	setenv("XRDP_SESSION", "1", 1);
	int result = pusb_local_login(&opts, "testuser", "testservice");
	unsetenv("XRDP_SESSION");

	assert_int_equal(0, result);
}

static void test_local_login_display_env_null(void **state)
{
	(void)state;
	t_pusb_options opts;
	memset(&opts, 0, sizeof(opts));
	opts.deny_remote = 1;

	const char *raw = getenv("DISPLAY"); /* DevSkim: ignore DS154189 - saving for restore, xstrdup'd immediately */
	char *saved = raw ? xstrdup(raw) : NULL;
	unsetenv("DISPLAY");
	int result = pusb_local_login(&opts, "testuser", "testservice");
	if (saved)
	{
		setenv("DISPLAY", saved, 1);
		xfree(saved);
	}

	/* NULL DISPLAY must not crash; result depends on environment (loginctl, utmp) */
	assert_true(result >= -1 && result <= 1);
}

static void test_is_sddm_active_returns_1_when_session_exists(void **state)
{
	(void)state;
	g_popen_output = "1\n";
	assert_int_equal(1, pusb_is_sddm_active());
}

static void test_is_sddm_active_returns_0_when_no_session(void **state)
{
	(void)state;
	g_popen_output = "";
	assert_int_equal(0, pusb_is_sddm_active());
}

static void test_get_tty_by_sddm_returns_tty_when_sddm_running(void **state)
{
	(void)state;
	g_popen_output = "tty1\n";
	char *result = pusb_get_tty_by_sddm();
	assert_non_null(result);
	assert_string_equal("tty1", result);
	xfree(result);
}

static void test_get_tty_by_sddm_returns_null_when_no_sddm_session(void **state)
{
	(void)state;
	g_popen_output = "";
	char *result = pusb_get_tty_by_sddm();
	assert_null(result);
}

static void test_get_tty_by_sddm_returns_null_on_empty_tty_field(void **state)
{
	(void)state;
	g_popen_output = "\n";
	char *result = pusb_get_tty_by_sddm();
	assert_null(result);
}

static void test_local_login_display_env_set(void **state)
{
	(void)state;
	t_pusb_options opts;
	memset(&opts, 0, sizeof(opts));
	opts.deny_remote = 1;

	const char *raw = getenv("DISPLAY"); /* DevSkim: ignore DS154189 - saving for restore, xstrdup'd immediately */
	char *saved = raw ? xstrdup(raw) : NULL;
	setenv("DISPLAY", ":0", 1);
	int result = pusb_local_login(&opts, "testuser", "testservice");
	if (saved)
	{
		setenv("DISPLAY", saved, 1);
		xfree(saved);
	}
	else
	{
		unsetenv("DISPLAY");
	}

	/* DISPLAY=:0 must not crash; result depends on environment (loginctl, utmp) */
	assert_true(result >= -1 && result <= 1);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_utmpx_field_equals_exact_nul_padded),
		cmocka_unit_test(test_utmpx_field_equals_rejects_null_input),
		cmocka_unit_test(test_utmpx_field_equals_rejects_prefix_match),
		cmocka_unit_test(test_utmpx_field_equals_rejects_value_longer_than_field),
		cmocka_unit_test(test_utmpx_field_equals_full_width_field),
		cmocka_unit_test(test_utmpx_field_starts_with_tty_number),
		cmocka_unit_test(test_utmpx_field_starts_with_rejects_null_input),
		cmocka_unit_test(test_utmpx_field_starts_with_pts_slave),
		cmocka_unit_test(test_utmpx_field_starts_with_full_width_field),
		cmocka_unit_test(test_utmpx_field_starts_with_rejects_long_prefix),
		cmocka_unit_test(test_loginctl_parse_output_newline_only),
		cmocka_unit_test(test_loginctl_parse_output_empty),
		cmocka_unit_test(test_loginctl_parse_output_valid_with_newline),
		cmocka_unit_test(test_loginctl_parse_output_valid_without_newline),
		cmocka_unit_test(test_local_login_denies_xrdp_session),
		cmocka_unit_test(test_local_login_display_env_null),
		cmocka_unit_test(test_local_login_display_env_set),
		cmocka_unit_test(test_is_sddm_active_returns_1_when_session_exists),
		cmocka_unit_test(test_is_sddm_active_returns_0_when_no_session),
		cmocka_unit_test(test_get_tty_by_sddm_returns_tty_when_sddm_running),
		cmocka_unit_test(test_get_tty_by_sddm_returns_null_when_no_sddm_session),
		cmocka_unit_test(test_get_tty_by_sddm_returns_null_on_empty_tty_field),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
