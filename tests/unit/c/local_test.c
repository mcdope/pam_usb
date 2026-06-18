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
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <cmocka.h>

/* Include source directly to access static helper functions. */
#include "../../../src/local.c"

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

static int make_memfd_with_content(const char *data, size_t len)
{
	int fd = memfd_create("test_cmdline", 0);
	if (fd < 0) return -1;
	if (write(fd, data, len) != (ssize_t)len) { int e = errno; close(fd); errno = e; return -1; }
	if (lseek(fd, 0, SEEK_SET) != 0) { int e = errno; close(fd); errno = e; return -1; }
	return fd;
}

static void test_read_cmdline_short_content(void **state)
{
	(void)state;
	char buf[CMDLINE_BUF_SIZE];
	memset(buf, 0xff, sizeof(buf));

	int fd = make_memfd_with_content("hello", 5);
	assert_true(fd >= 0);
	ssize_t n = pusb_read_cmdline(fd, buf, CMDLINE_BUF_SIZE);
	close(fd);

	assert_int_equal(5, n);
	assert_string_equal("hello", buf);
	assert_int_equal('\0', buf[5]);
}

static void test_read_cmdline_full_buffer(void **state)
{
	(void)state;
	/* Exactly CMDLINE_BUF_SIZE - 1 'A' bytes — the maximum the fixed code will read. */
	char src[CMDLINE_BUF_SIZE - 1];
	memset(src, 'A', sizeof(src));

	char buf[CMDLINE_BUF_SIZE];
	memset(buf, 0xff, sizeof(buf));

	int fd = make_memfd_with_content(src, sizeof(src));
	assert_true(fd >= 0);
	ssize_t n = pusb_read_cmdline(fd, buf, CMDLINE_BUF_SIZE);
	close(fd);

	assert_int_equal(CMDLINE_BUF_SIZE - 1, n);
	assert_int_equal('\0', buf[CMDLINE_BUF_SIZE - 1]);
}

static void test_read_cmdline_replaces_nulls(void **state)
{
	(void)state;
	/* cmdline format: NUL-separated argv tokens */
	const char data[] = "Xorg\0:0\0-auth\0/tmp/auth";
	size_t len = sizeof(data) - 1; /* drop trailing \0 added by C string literal */

	char buf[CMDLINE_BUF_SIZE];
	memset(buf, 0, sizeof(buf));

	int fd = make_memfd_with_content(data, len);
	assert_true(fd >= 0);
	ssize_t n = pusb_read_cmdline(fd, buf, CMDLINE_BUF_SIZE);
	close(fd);

	assert_int_equal((ssize_t)len, n);
	/* All internal NULs must be replaced with spaces */
	assert_string_equal("Xorg :0 -auth /tmp/auth", buf);
	assert_int_equal('\0', buf[n]);
}

static void test_read_cmdline_read_error(void **state)
{
	(void)state;
	char buf[CMDLINE_BUF_SIZE];
	/* fd -1 is always invalid */
	ssize_t n = pusb_read_cmdline(-1, buf, CMDLINE_BUF_SIZE);
	assert_int_equal(-1, n);
}

static void test_is_tty_local_null_returns_zero(void **state)
{
	(void)state;
	/* NULL tty must be rejected safely (fail-closed), never dereferenced. */
	assert_int_equal(0, pusb_is_tty_local(NULL));
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

static void test_loginctl_cmd_uses_auto_not_user_status(void **state)
{
	(void)state;
	/* Regression for issue #430: the command must resolve by calling-process
	 * session ("auto"), not by scraping the first session from user-status. */
	assert_non_null(strstr(LOGINCTL_SHOW_SESSION_CMD, "show-session auto"));
	assert_null(strstr(LOGINCTL_SHOW_SESSION_CMD, "user-status"));
	assert_null(strstr(LOGINCTL_SHOW_SESSION_CMD, "sed -n"));
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
		cmocka_unit_test(test_read_cmdline_short_content),
		cmocka_unit_test(test_read_cmdline_full_buffer),
		cmocka_unit_test(test_read_cmdline_replaces_nulls),
		cmocka_unit_test(test_read_cmdline_read_error),
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
		cmocka_unit_test(test_loginctl_cmd_uses_auto_not_user_status),
		cmocka_unit_test(test_is_tty_local_null_returns_zero),
		cmocka_unit_test(test_local_login_denies_xrdp_session),
		cmocka_unit_test(test_local_login_display_env_null),
		cmocka_unit_test(test_local_login_display_env_set),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
