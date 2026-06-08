/*
 * Unit tests for src/evdev.c
 *
 * opendir/readdir/closedir/open/close are wrapped at link time.
 * fake_libevdev.c provides stub libevdev API controlled via g_mock_devices[].
 *
 * Convention: g_mock_devices[i] describes the device that will be returned
 * when open() is called for "eventI".
 */

#define _GNU_SOURCE
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <errno.h>
#include <cmocka.h>
#include <linux/input.h>

/* Declarations from fake_libevdev.c */
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

/* Include source directly */
#include "../../../src/evdev.c"

/* ── Helpers ── */

#define SETUP_DEVICE(idx, bt, ph, et) \
	do { \
		g_mock_devices[idx].bustype     = (bt); \
		g_mock_devices[idx].phys        = (ph); \
		g_mock_devices[idx].event_types = (et); \
		g_mock_devices[idx].init_fails  = 0; \
		g_mock_devices[idx].open_errno  = 0; \
		g_mock_device_count = (idx) + 1; \
	} while(0)

#define SETUP_DEVICE_EACCES(idx) \
	do { \
		memset(&g_mock_devices[idx], 0, sizeof(g_mock_devices[idx])); \
		g_mock_devices[idx].open_errno = EACCES; \
		g_mock_device_count = (idx) + 1; \
	} while(0)

#define SETUP_DEVICE_ELOOP(idx) \
	do { \
		int _idx = (idx); \
		memset(&g_mock_devices[_idx], 0, sizeof(g_mock_devices[_idx])); \
		g_mock_devices[_idx].open_errno = ELOOP; \
		g_mock_device_count = _idx + 1; \
	} while(0)

static int setup(void **state)
{
	(void)state;
	fake_evdev_reset();
	return 0;
}

/* ── Tests ── */

static void test_no_devices(void **state)
{
	(void)state;
	/* g_mock_device_count == 0, opendir returns NULL */
	assert_int_equal(0, pusb_has_virtual_input_device("/dev/input"));
}

static void test_physical_usb_keyboard(void **state)
{
	(void)state;
	/* BUS_USB, has physical path, has EV_KEY → NOT virtual */
	SETUP_DEVICE(0, BUS_USB, "/dev/input/event0", (1u << EV_KEY));
	assert_int_equal(0, pusb_has_virtual_input_device("/dev/input"));
}

static void test_virtual_keyboard(void **state)
{
	(void)state;
	/* BUS_VIRTUAL, no phys, EV_KEY → virtual */
	SETUP_DEVICE(0, BUS_VIRTUAL, NULL, (1u << EV_KEY));
	assert_int_equal(1, pusb_has_virtual_input_device("/dev/input"));
}

static void test_virtual_mouse(void **state)
{
	(void)state;
	/* BUS_VIRTUAL, no phys, EV_REL → virtual */
	SETUP_DEVICE(0, BUS_VIRTUAL, NULL, (1u << EV_REL));
	assert_int_equal(1, pusb_has_virtual_input_device("/dev/input"));
}

static void test_virtual_tablet(void **state)
{
	(void)state;
	/* BUS_VIRTUAL, no phys, EV_ABS → virtual */
	SETUP_DEVICE(0, BUS_VIRTUAL, NULL, (1u << EV_ABS));
	assert_int_equal(1, pusb_has_virtual_input_device("/dev/input"));
}

static void test_virtual_no_input_capability(void **state)
{
	(void)state;
	/* BUS_VIRTUAL, no phys, only EV_SYN → NOT flagged (no keyboard/pointer cap) */
	SETUP_DEVICE(0, BUS_VIRTUAL, NULL, (1u << EV_SYN));
	assert_int_equal(0, pusb_has_virtual_input_device("/dev/input"));
}

static void test_virtual_with_phys_path(void **state)
{
	(void)state;
	/* BUS_VIRTUAL but phys is set → NOT flagged */
	SETUP_DEVICE(0, BUS_VIRTUAL, "usb-0000:00:14.0-2/input0", (1u << EV_KEY));
	assert_int_equal(0, pusb_has_virtual_input_device("/dev/input"));
}

static void test_virtual_with_empty_phys(void **state)
{
	(void)state;
	/* BUS_VIRTUAL, phys is empty string → IS flagged */
	SETUP_DEVICE(0, BUS_VIRTUAL, "", (1u << EV_KEY));
	assert_int_equal(1, pusb_has_virtual_input_device("/dev/input"));
}

static void test_open_fails_skipped(void **state)
{
	(void)state;
	/* open() returns -1 for idx >= device_count, but we set count=1 and have
	 * init_fails so libevdev_new_from_fd will fail → should return 0 */
	g_mock_devices[0].bustype     = BUS_VIRTUAL;
	g_mock_devices[0].phys        = NULL;
	g_mock_devices[0].event_types = (1u << EV_KEY);
	g_mock_devices[0].init_fails  = 1;
	g_mock_device_count = 1;
	assert_int_equal(0, pusb_has_virtual_input_device("/dev/input"));
}

static void test_physical_before_virtual(void **state)
{
	(void)state;
	/* First device is physical, second is virtual → should detect virtual */
	SETUP_DEVICE(0, BUS_USB, "/dev/input/event0", (1u << EV_KEY));
	g_mock_devices[1].bustype     = BUS_VIRTUAL;
	g_mock_devices[1].phys        = NULL;
	g_mock_devices[1].event_types = (1u << EV_KEY);
	g_mock_devices[1].init_fails  = 0;
	g_mock_device_count = 2;
	assert_int_equal(1, pusb_has_virtual_input_device("/dev/input"));
}

static void test_opendir_eacces_returns_inconclusive(void **state)
{
	(void)state;
	/* opendir() itself fails with EACCES → should return -1, not 0 */
	g_opendir_errno = EACCES;
	g_mock_device_count = 0;
	assert_int_equal(-1, pusb_has_virtual_input_device("/dev/input"));
}

static void test_opendir_eperm_returns_inconclusive(void **state)
{
	(void)state;
	/* opendir() fails with EPERM (e.g. AppArmor/SELinux policy) → should return -1 */
	g_opendir_errno = EPERM;
	g_mock_device_count = 0;
	assert_int_equal(-1, pusb_has_virtual_input_device("/dev/input"));
}

static void test_all_devices_eperm_returns_inconclusive(void **state)
{
	(void)state;
	/* All opens fail with EPERM (LSM policy) → should return -1, not 0 */
	g_mock_devices[0].open_errno = EPERM;
	g_mock_device_count = 1;
	assert_int_equal(-1, pusb_has_virtual_input_device("/dev/input"));
}

static void test_all_devices_eacces_returns_inconclusive(void **state)
{
	(void)state;
	/* All opens fail with EACCES → should return -1, not 0 */
	SETUP_DEVICE_EACCES(0);
	assert_int_equal(-1, pusb_has_virtual_input_device("/dev/input"));
}

static void test_eacces_then_virtual_returns_found(void **state)
{
	(void)state;
	/* First device EACCES, second is virtual keyboard → should return 1 */
	SETUP_DEVICE_EACCES(0);
	g_mock_devices[1].bustype     = BUS_VIRTUAL;
	g_mock_devices[1].phys        = NULL;
	g_mock_devices[1].event_types = (1u << EV_KEY);
	g_mock_devices[1].init_fails  = 0;
	g_mock_devices[1].open_errno  = 0;
	g_mock_device_count = 2;
	assert_int_equal(1, pusb_has_virtual_input_device("/dev/input"));
}

static void test_symlink_device_skipped(void **state)
{
	(void)state;
	/* O_NOFOLLOW rejects a symlink → ELOOP → device skipped, result is 0 (not inconclusive) */
	SETUP_DEVICE_ELOOP(0);
	assert_int_equal(0, pusb_has_virtual_input_device("/dev/input"));
}

static void test_symlink_then_virtual_detected(void **state)
{
	(void)state;
	/* First device is a symlink (ELOOP), second is a real virtual keyboard → still detected */
	SETUP_DEVICE_ELOOP(0);
	SETUP_DEVICE(1, BUS_VIRTUAL, NULL, (1u << EV_KEY));
	assert_int_equal(1, pusb_has_virtual_input_device("/dev/input"));
}

/* ── pusb_has_virtual_input_device_safe() fallback tests ──
 * Compiled with PAMUSB_EVDEV_HELPER_PATH="/nonexistent/pamusb-evdev-helper"
 * so the safe variant always falls back to the direct scan.  The underlying
 * direct-scan behaviour is therefore the same as the tests above.
 */

static void test_safe_no_helper_falls_back_no_devices(void **state)
{
	(void)state;
	/* helper absent, no mock devices → fallback returns 0 */
	assert_int_equal(0, pusb_has_virtual_input_device_safe("/dev/input"));
}

static void test_safe_no_helper_virtual_device_detected(void **state)
{
	(void)state;
	/* helper absent, virtual keyboard present → fallback returns 1 */
	SETUP_DEVICE(0, BUS_VIRTUAL, NULL, (1u << EV_KEY));
	assert_int_equal(1, pusb_has_virtual_input_device_safe("/dev/input"));
}

static void test_safe_no_helper_all_eacces_inconclusive(void **state)
{
	(void)state;
	/* helper absent, all EACCES → fallback returns -1 */
	SETUP_DEVICE_EACCES(0);
	assert_int_equal(-1, pusb_has_virtual_input_device_safe("/dev/input"));
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup(test_no_devices,                   setup),
		cmocka_unit_test_setup(test_physical_usb_keyboard,        setup),
		cmocka_unit_test_setup(test_virtual_keyboard,             setup),
		cmocka_unit_test_setup(test_virtual_mouse,                setup),
		cmocka_unit_test_setup(test_virtual_tablet,               setup),
		cmocka_unit_test_setup(test_virtual_no_input_capability,  setup),
		cmocka_unit_test_setup(test_virtual_with_phys_path,       setup),
		cmocka_unit_test_setup(test_virtual_with_empty_phys,      setup),
		cmocka_unit_test_setup(test_open_fails_skipped,                    setup),
		cmocka_unit_test_setup(test_physical_before_virtual,               setup),
		cmocka_unit_test_setup(test_opendir_eacces_returns_inconclusive,    setup),
		cmocka_unit_test_setup(test_opendir_eperm_returns_inconclusive,     setup),
		cmocka_unit_test_setup(test_all_devices_eacces_returns_inconclusive, setup),
		cmocka_unit_test_setup(test_all_devices_eperm_returns_inconclusive, setup),
		cmocka_unit_test_setup(test_eacces_then_virtual_returns_found,     setup),
		cmocka_unit_test_setup(test_symlink_device_skipped,               setup),
		cmocka_unit_test_setup(test_symlink_then_virtual_detected,        setup),
		/* safe-wrapper fallback tests */
		cmocka_unit_test_setup(test_safe_no_helper_falls_back_no_devices,  setup),
		cmocka_unit_test_setup(test_safe_no_helper_virtual_device_detected, setup),
		cmocka_unit_test_setup(test_safe_no_helper_all_eacces_inconclusive, setup),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
