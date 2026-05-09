/*
 * Unit tests for src/remote_services.c
 * Tests static helpers (via direct include) and public function.
 * No linker wraps needed — static helper functions accept path parameters,
 * allowing tests to inject fixture file paths directly.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <cmocka.h>

/* Include source directly to access static functions */
#include "../../../src/remote_services.c"

/* Fixture paths */
#define FIXTURES "tests/unit/c/fixtures"
#define TCP_EXTERNAL    FIXTURES "/proc_net_tcp_vnc_external"
#define TCP_LOOPBACK    FIXTURES "/proc_net_tcp_vnc_loopback"
#define TCP_LISTEN      FIXTURES "/proc_net_tcp_vnc_listen"
#define TCP_EMPTY       FIXTURES "/proc_net_tcp_empty"
#define TCP6_EXTERNAL      FIXTURES "/proc_net_tcp6_vnc_external"
#define TCP6_LOOPBACK      FIXTURES "/proc_net_tcp6_vnc_loopback"
#define TCP_RDP_EXTERNAL   FIXTURES "/proc_net_tcp_rdp_external"
#define TCP6_RDP_EXTERNAL  FIXTURES "/proc_net_tcp6_rdp_external"
#define TCP_TV_CONNECTED   FIXTURES "/proc_net_tcp_tv_connected"
#define TCP6_TV_CONNECTED  FIXTURES "/proc_net_tcp6_tv_connected"
#define PROC_TEAMVIEWER    FIXTURES "/proc_teamviewer"
#define PROC_TEAMVIEWERD   FIXTURES "/proc_teamviewerd"
#define PROC_ANYDESK       FIXTURES "/proc_anydesk"
#define PROC_ANYDESK_ARGS  FIXTURES "/proc_anydesk_in_args"
#define PROC_FIREFOX       FIXTURES "/proc_firefox"

/* ── pusb_proc_tcp_is_loopback_v4 ── */

static void test_loopback_v4_127_0_0_1(void **state)
{
	(void)state;
	/* 127.0.0.1 in /proc/net/tcp little-endian format: htonl(0x0100007F) >> 24 == 127 */
	assert_int_equal(1, pusb_proc_tcp_is_loopback_v4(0x0100007F));
}

static void test_loopback_v4_127_x_x_x(void **state)
{
	(void)state;
	/* 127.1.2.3 = 0x0302017F in LE format */
	assert_int_equal(1, pusb_proc_tcp_is_loopback_v4(0x0302017F));
}

static void test_loopback_v4_non_loopback(void **state)
{
	(void)state;
	/* 192.168.1.1 = 0x0101A8C0 in LE format */
	assert_int_equal(0, pusb_proc_tcp_is_loopback_v4(0x0101A8C0));
}

/* ── pusb_proc_tcp6_is_loopback ── */

static void test_loopback_v6_loopback_le(void **state)
{
	(void)state;
	/* ::1 in little-endian /proc/net/tcp6 format */
	assert_int_equal(1, pusb_proc_tcp6_is_loopback("00000000000000000000000001000000")); /* DevSkim: ignore */
}

static void test_loopback_v6_loopback_be(void **state)
{
	(void)state;
	/* ::1 in big-endian /proc/net/tcp6 format */
	assert_int_equal(1, pusb_proc_tcp6_is_loopback("00000000000000000000000000000001")); /* DevSkim: ignore */
}

static void test_loopback_v6_external(void **state)
{
	(void)state;
	/* 2001:db8::1 is not loopback */
	assert_int_equal(0, pusb_proc_tcp6_is_loopback("B80D0120000000000000000001000000"));
}

static void test_loopback_v6_short_string(void **state)
{
	(void)state;
	assert_int_equal(0, pusb_proc_tcp6_is_loopback("0000"));
}

/* ── pusb_proc_tcp4_has_established ── */

static void test_tcp4_empty_file(void **state)
{
	(void)state;
	assert_int_equal(0, pusb_proc_tcp4_has_established(TCP_EMPTY, 5900));
}

static void test_tcp4_external_connection(void **state)
{
	(void)state;
	assert_int_equal(1, pusb_proc_tcp4_has_established(TCP_EXTERNAL, 5900));
}

static void test_tcp4_loopback_not_detected(void **state)
{
	(void)state;
	assert_int_equal(0, pusb_proc_tcp4_has_established(TCP_LOOPBACK, 5900));
}

static void test_tcp4_listen_not_detected(void **state)
{
	(void)state;
	assert_int_equal(0, pusb_proc_tcp4_has_established(TCP_LISTEN, 5900));
}

static void test_tcp4_wrong_port(void **state)
{
	(void)state;
	/* File has port 5900 (0x170C), check for 3389 (0x0D3D) */
	assert_int_equal(0, pusb_proc_tcp4_has_established(TCP_EXTERNAL, 3389));
}

static void test_tcp4_nonexistent_file(void **state)
{
	(void)state;
	assert_int_equal(0, pusb_proc_tcp4_has_established("/nonexistent/path/tcp", 5900));
}

/* ── pusb_proc_tcp6_has_established ── */

static void test_tcp6_external_connection(void **state)
{
	(void)state;
	assert_int_equal(1, pusb_proc_tcp6_has_established(TCP6_EXTERNAL, 5900));
}

static void test_tcp6_loopback_not_detected(void **state)
{
	(void)state;
	assert_int_equal(0, pusb_proc_tcp6_has_established(TCP6_LOOPBACK, 5900));
}

/* ── pusb_process_name_exists ── */

static void test_process_teamviewer_desk_exists(void **state)
{
	(void)state;
	assert_int_equal(1, pusb_process_name_exists(PROC_TEAMVIEWER, "TeamViewer_Desk"));
}

static void test_process_anydesk_exists(void **state)
{
	(void)state;
	assert_int_equal(1, pusb_process_name_exists(PROC_ANYDESK, "anydesk"));
}

static void test_process_unknown_not_detected(void **state)
{
	(void)state;
	/* proc_firefox only has "firefox", not "TeamViewer_Desk" */
	assert_int_equal(0, pusb_process_name_exists(PROC_FIREFOX, "TeamViewer_Desk"));
}

static void test_process_nonexistent_dir(void **state)
{
	(void)state;
	assert_int_equal(0, pusb_process_name_exists("/nonexistent/proc", "anything"));
}

static void test_process_anydesk_not_detected_in_argv(void **state)
{
	(void)state;
	/* "anydesk" appears only in argv[1+], not argv[0] — must not trigger */
	assert_int_equal(0, pusb_process_name_exists(PROC_ANYDESK_ARGS, "anydesk"));
}

static void test_tcp4_rdp_external_connection(void **state)
{
	(void)state;
	assert_int_equal(1, pusb_proc_tcp4_has_established(TCP_RDP_EXTERNAL, 3389));
}

static void test_tcp4_rdp_wrong_port(void **state)
{
	(void)state;
	/* File has port 3389 (0x0D3D); checking VNC port 5900 must return 0 */
	assert_int_equal(0, pusb_proc_tcp4_has_established(TCP_RDP_EXTERNAL, 5900));
}

static void test_tcp6_rdp_external_connection(void **state)
{
	(void)state;
	assert_int_equal(1, pusb_proc_tcp6_has_established(TCP6_RDP_EXTERNAL, 3389));
}

static void test_tcp6_rdp_wrong_port(void **state)
{
	(void)state;
	/* File has port 3389; checking VNC port 5900 must return 0 */
	assert_int_equal(0, pusb_proc_tcp6_has_established(TCP6_RDP_EXTERNAL, 5900));
}

/* ── pusb_proc_tcp4_has_established_to ── */

static void test_tcp4_established_to_tv_port(void **state)
{
	(void)state;
	assert_int_equal(1, pusb_proc_tcp4_has_established_to(TCP_TV_CONNECTED, 5938));
}

static void test_tcp4_established_to_wrong_remote_port(void **state)
{
	(void)state;
	/* File has remote port 5938; checking 5900 must return 0 */
	assert_int_equal(0, pusb_proc_tcp4_has_established_to(TCP_TV_CONNECTED, 5900));
}

static void test_tcp4_established_to_loopback_ignored(void **state)
{
	(void)state;
	/* TCP_LOOPBACK has remote 127.x.x.x — must not match even if port fits */
	assert_int_equal(0, pusb_proc_tcp4_has_established_to(TCP_LOOPBACK, 5900));
}

/* ── pusb_proc_tcp6_has_established_to ── */

static void test_tcp6_established_to_tv_port(void **state)
{
	(void)state;
	assert_int_equal(1, pusb_proc_tcp6_has_established_to(TCP6_TV_CONNECTED, 5938));
}

static void test_tcp6_established_to_wrong_remote_port(void **state)
{
	(void)state;
	/* File has remote port 5938; checking 5900 must return 0 */
	assert_int_equal(0, pusb_proc_tcp6_has_established_to(TCP6_TV_CONNECTED, 5900));
}

/* ── teamviewerd process detection ── */

static void test_process_teamviewerd_exists(void **state)
{
	(void)state;
	assert_int_equal(1, pusb_process_name_exists(PROC_TEAMVIEWERD, "teamviewerd"));
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_loopback_v4_127_0_0_1),
		cmocka_unit_test(test_loopback_v4_127_x_x_x),
		cmocka_unit_test(test_loopback_v4_non_loopback),
		cmocka_unit_test(test_loopback_v6_loopback_le),
		cmocka_unit_test(test_loopback_v6_loopback_be),
		cmocka_unit_test(test_loopback_v6_external),
		cmocka_unit_test(test_loopback_v6_short_string),
		cmocka_unit_test(test_tcp4_empty_file),
		cmocka_unit_test(test_tcp4_external_connection),
		cmocka_unit_test(test_tcp4_loopback_not_detected),
		cmocka_unit_test(test_tcp4_listen_not_detected),
		cmocka_unit_test(test_tcp4_wrong_port),
		cmocka_unit_test(test_tcp4_nonexistent_file),
		cmocka_unit_test(test_tcp6_external_connection),
		cmocka_unit_test(test_tcp6_loopback_not_detected),
		cmocka_unit_test(test_process_teamviewer_desk_exists),
		cmocka_unit_test(test_process_anydesk_exists),
		cmocka_unit_test(test_process_anydesk_not_detected_in_argv),
		cmocka_unit_test(test_process_unknown_not_detected),
		cmocka_unit_test(test_process_nonexistent_dir),
		cmocka_unit_test(test_tcp4_rdp_external_connection),
		cmocka_unit_test(test_tcp4_rdp_wrong_port),
		cmocka_unit_test(test_tcp6_rdp_external_connection),
		cmocka_unit_test(test_tcp6_rdp_wrong_port),
		cmocka_unit_test(test_tcp4_established_to_tv_port),
		cmocka_unit_test(test_tcp4_established_to_wrong_remote_port),
		cmocka_unit_test(test_tcp4_established_to_loopback_ignored),
		cmocka_unit_test(test_tcp6_established_to_tv_port),
		cmocka_unit_test(test_tcp6_established_to_wrong_remote_port),
		cmocka_unit_test(test_process_teamviewerd_exists),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
