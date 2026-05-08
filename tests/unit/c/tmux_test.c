/*
 * Unit tests for src/tmux.c
 * Tests static (via direct include) and public functions.
 * popen/pclose are wrapped at link time with --wrap=popen,--wrap=pclose.
 *
 * Security regression coverage:
 *   C-1: injection via TMUX socket path metacharacters
 *   C-1: non-numeric TMUX client ID rejected
 *   C-1: pusb_tmux_escape_for_regex handles all 14 ERE metacharacters
 *   C-1: dot in username escaped so it doesn't match a different user
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cmocka.h>

/* Include source directly to access static functions */
#include "../../../src/tmux.c"

/* ── popen/pclose mock ── */

static const char *g_popen_output = "";

FILE *__wrap_popen(const char *cmd, const char *type)
{
	(void)cmd;
	return fmemopen((void *)g_popen_output, strlen(g_popen_output), type);
}

int __wrap_pclose(FILE *fp)
{
	fclose(fp);
	return 0;
}

/* ── pusb_tmux_is_safe_socket_path ── */

static void test_safe_path_valid(void **state)
{
	(void)state;
	assert_int_equal(1, pusb_tmux_is_safe_socket_path("/tmp/tmux-1000/default"));
}

static void test_safe_path_empty(void **state)
{
	(void)state;
	assert_int_equal(0, pusb_tmux_is_safe_socket_path(""));
}

static void test_safe_path_semicolon(void **state)
{
	(void)state;
	assert_int_equal(0, pusb_tmux_is_safe_socket_path("/tmp/x;y"));
}

static void test_safe_path_dollar(void **state)
{
	(void)state;
	assert_int_equal(0, pusb_tmux_is_safe_socket_path("/tmp/x$y"));
}

static void test_safe_path_backtick(void **state)
{
	(void)state;
	assert_int_equal(0, pusb_tmux_is_safe_socket_path("/tmp/x`y"));
}

static void test_safe_path_double_quote(void **state)
{
	(void)state;
	assert_int_equal(0, pusb_tmux_is_safe_socket_path("/tmp/x\"y"));
}

static void test_safe_path_pipe(void **state)
{
	(void)state;
	assert_int_equal(0, pusb_tmux_is_safe_socket_path("/tmp/x|y"));
}

/* ── pusb_tmux_is_numeric_id ── */

static void test_numeric_id_valid(void **state)
{
	(void)state;
	assert_int_equal(1, pusb_tmux_is_numeric_id("42"));
}

static void test_numeric_id_empty(void **state)
{
	(void)state;
	assert_int_equal(0, pusb_tmux_is_numeric_id(""));
}

static void test_numeric_id_nonnumeric(void **state)
{
	(void)state;
	assert_int_equal(0, pusb_tmux_is_numeric_id("4a2"));
}

/* ── pusb_tmux_escape_for_regex ── */

static void test_escape_dot(void **state)
{
	(void)state;
	char dst[64] = {0};
	pusb_tmux_escape_for_regex("a.b", dst, sizeof(dst));
	assert_string_equal("a\\.b", dst);
}

static void test_escape_plus(void **state)
{
	(void)state;
	char dst[64] = {0};
	pusb_tmux_escape_for_regex("user+name", dst, sizeof(dst));
	assert_string_equal("user\\+name", dst);
}

static void test_escape_all_metacharacters(void **state)
{
	(void)state;
	/* All 14 ERE metacharacters: \ . ^ $ * + ? { } [ ] | ( ) */
	const char *input = "\\.^$*+?{}[]|()";
	char dst[128] = {0};
	pusb_tmux_escape_for_regex(input, dst, sizeof(dst));
	/* Every metachar should be preceded by a backslash */
	const char *expected = "\\\\\\.\\^\\$\\*\\+\\?\\{\\}\\[\\]\\|\\(\\)";
	assert_string_equal(expected, dst);
}

static void test_escape_empty_input(void **state)
{
	(void)state;
	char dst[16] = "unchanged";
	pusb_tmux_escape_for_regex("", dst, sizeof(dst));
	assert_string_equal("", dst);
}

/* ── pusb_tmux_has_remote_clients ── */

static void test_has_remote_ipv4(void **state)
{
	(void)state;
	/* w output line contains IPv4 address and "tmux" — should detect remote */
	g_popen_output = "testuser pts/1   192.168.1.100  10:00   0.00s  tmux attach\n";
	assert_int_equal(1, pusb_tmux_has_remote_clients("testuser"));
}

static void test_has_remote_ipv6(void **state)
{
	(void)state;
	/* w output with IPv6 — should detect remote */
	g_popen_output = "testuser pts/1   2001:db8:85a3:0000  10:00   0.00s  tmux attach\n";
	assert_int_equal(1, pusb_tmux_has_remote_clients("testuser"));
}

static void test_has_remote_local_display(void **state)
{
	(void)state;
	/* Local X display (:0) — no IP, not remote */
	g_popen_output = "testuser :0  -  10:00  0.00s  bash\n";
	assert_int_equal(0, pusb_tmux_has_remote_clients("testuser"));
}

static void test_has_remote_empty_output(void **state)
{
	(void)state;
	g_popen_output = "";
	assert_int_equal(0, pusb_tmux_has_remote_clients("testuser"));
}

static void test_has_remote_wrong_user_dot_regression(void **state)
{
	(void)state;
	/* C-1 regression: "test.user" must have its dot escaped so "testZuser" doesn't match.
	   Without escaping, regex "test.user" (dot = any char) would match "testZuser". */
	g_popen_output = "testZuser pts/1   192.168.1.100  10:00   0.00s  tmux attach\n";
	assert_int_equal(0, pusb_tmux_has_remote_clients("test.user"));
}

/* ── pusb_tmux_get_client_tty ── */

static void test_get_client_tty_no_tmux_env(void **state)
{
	(void)state;
	unsetenv("TMUX");
	/* env_pid=0 means "use getenv(TMUX)" — which is unset, so returns NULL */
	char *result = pusb_tmux_get_client_tty(0);
	assert_null(result);
}

static void test_get_client_tty_injection_semicolon(void **state)
{
	(void)state;
	/* C-1 regression: semicolon in socket path must be rejected */
	setenv("TMUX", "/tmp/tmux;evil,abc,42", 1);
	char *result = pusb_tmux_get_client_tty(0);
	assert_null(result);
	unsetenv("TMUX");
}

static void test_get_client_tty_nonnumeric_id(void **state)
{
	(void)state;
	/* C-1 regression: non-numeric client ID must be rejected */
	setenv("TMUX", "/tmp/tmux-1000/default,abc,notanumber", 1);
	char *result = pusb_tmux_get_client_tty(0);
	assert_null(result);
	unsetenv("TMUX");
}

static void test_get_client_tty_valid(void **state)
{
	(void)state;
	setenv("TMUX", "/tmp/tmux-1000/default,abc,42", 1);
	g_popen_output = "/dev/pts/1: session info\n";
	char *result = pusb_tmux_get_client_tty(0);
	assert_non_null(result);
	assert_string_equal("pts/1", result);
	free(result);
	unsetenv("TMUX");
}

/* ── main ── */

int main(void)
{
	t_pusb_options opts = {0};
	opts.debug = 0;
	opts.quiet = 1;
	pusb_log_init(&opts);

	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_safe_path_valid),
		cmocka_unit_test(test_safe_path_empty),
		cmocka_unit_test(test_safe_path_semicolon),
		cmocka_unit_test(test_safe_path_dollar),
		cmocka_unit_test(test_safe_path_backtick),
		cmocka_unit_test(test_safe_path_double_quote),
		cmocka_unit_test(test_safe_path_pipe),
		cmocka_unit_test(test_numeric_id_valid),
		cmocka_unit_test(test_numeric_id_empty),
		cmocka_unit_test(test_numeric_id_nonnumeric),
		cmocka_unit_test(test_escape_dot),
		cmocka_unit_test(test_escape_plus),
		cmocka_unit_test(test_escape_all_metacharacters),
		cmocka_unit_test(test_escape_empty_input),
		cmocka_unit_test(test_has_remote_ipv4),
		cmocka_unit_test(test_has_remote_ipv6),
		cmocka_unit_test(test_has_remote_local_display),
		cmocka_unit_test(test_has_remote_empty_output),
		cmocka_unit_test(test_has_remote_wrong_user_dot_regression),
		cmocka_unit_test(test_get_client_tty_no_tmux_env),
		cmocka_unit_test(test_get_client_tty_injection_semicolon),
		cmocka_unit_test(test_get_client_tty_nonnumeric_id),
		cmocka_unit_test(test_get_client_tty_valid),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
