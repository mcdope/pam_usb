/*
 * Unit tests for src/evdev.c
 *
 * opendir/readdir/closedir/open/close are wrapped at link time.
 * fake_libevdev.c provides stub libevdev API controlled via g_mock_devices[].
 *
 * Convention: g_mock_devices[i] describes the device that will be returned
 * when open() is called for "eventI".
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>
#include <linux/input.h>

/* Declarations from fake_libevdev.c */
typedef struct {
	int  bustype;
	const char *phys;
	unsigned int event_types;
	int  init_fails;
} fake_device_t;

extern fake_device_t g_mock_devices[];
extern int           g_mock_device_count;
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
		g_mock_device_count = (idx) + 1; \
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
		cmocka_unit_test_setup(test_open_fails_skipped,           setup),
		cmocka_unit_test_setup(test_physical_before_virtual,      setup),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
