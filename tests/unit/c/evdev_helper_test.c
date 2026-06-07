/*
 * Unit tests for the pusb_has_virtual_input_device_safe() helper-exec path.
 *
 * Compiled twice by the Makefile:
 *   evdev_helper_test_exit0  — PAMUSB_EVDEV_HELPER_PATH → fake_evdev_helper_exit0.sh
 *                              EXPECTED_HELPER_EXIT=0
 *   evdev_helper_test_exit1  — PAMUSB_EVDEV_HELPER_PATH → fake_evdev_helper_exit1.sh
 *                              EXPECTED_HELPER_EXIT=1
 *
 * The fake helper scripts exit with the expected code; the mock opendir/etc.
 * ensure the fallback direct scan is never reached during these tests.
 */

#define _GNU_SOURCE
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <errno.h>
#include <cmocka.h>
#include <linux/input.h>

typedef struct {
	int  bustype;
	const char *phys;
	unsigned int event_types;
	int  init_fails;
	int  open_errno;
} fake_device_t;

extern fake_device_t g_mock_devices[];
extern int           g_mock_device_count;
extern int           g_opendir_errno;
void fake_evdev_reset(void);

#include "../../../src/evdev.c"

#ifndef EXPECTED_HELPER_EXIT
#define EXPECTED_HELPER_EXIT 0
#endif

static int setup(void **state)
{
	(void)state;
	fake_evdev_reset();
	return 0;
}

static void test_helper_exec_returns_expected(void **state)
{
	(void)state;
	/* With a working helper script, _safe() must reflect the helper exit code.
	 * g_mock_device_count == 0 so the fallback direct scan would return 0;
	 * if the result differs from 0 we know the helper path was actually taken. */
	assert_int_equal(EXPECTED_HELPER_EXIT,
	                 pusb_has_virtual_input_device_safe("/dev/input"));
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup(test_helper_exec_returns_expected, setup),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
