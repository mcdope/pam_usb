/*
 * Unit tests for src/log.c
 * Tests pusb_log_init state management: null safety, value copying, and
 * sequential override. The critical test (copies_not_pointer) directly
 * guards against regression to the old static-pointer pattern that caused
 * the thread-safety bug fixed in #355 (issue #350).
 *
 * log.h getters (pusb_log_get_debug/quiet/color) expose the thread-local state
 * without direct variable access; log.o is linked at build time.
 */

#define _GNU_SOURCE
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>

#include "../../../src/log.h"

/* ── pusb_log_init(NULL) ── */

static void test_log_init_null_zeroes_all(void **state)
{
	(void)state;
	t_pusb_options opts;
	memset(&opts, 0, sizeof(opts));
	opts.debug = 1;
	opts.quiet = 1;
	opts.color_log = 1;
	pusb_log_init(&opts);

	pusb_log_init(NULL);

	assert_int_equal(0, pusb_log_get_debug());
	assert_int_equal(0, pusb_log_get_quiet());
	assert_int_equal(0, pusb_log_get_color());
}

/* ── pusb_log_init(&opts) copies individual fields ── */

static void test_log_init_sets_debug(void **state)
{
	(void)state;
	t_pusb_options opts;
	memset(&opts, 0, sizeof(opts));
	opts.debug = 1;
	pusb_log_init(&opts);
	assert_int_equal(1, pusb_log_get_debug());
	assert_int_equal(0, pusb_log_get_quiet());
	assert_int_equal(0, pusb_log_get_color());
}

static void test_log_init_sets_quiet(void **state)
{
	(void)state;
	t_pusb_options opts;
	memset(&opts, 0, sizeof(opts));
	opts.quiet = 1;
	pusb_log_init(&opts);
	assert_int_equal(0, pusb_log_get_debug());
	assert_int_equal(1, pusb_log_get_quiet());
	assert_int_equal(0, pusb_log_get_color());
}

static void test_log_init_sets_color(void **state)
{
	(void)state;
	t_pusb_options opts;
	memset(&opts, 0, sizeof(opts));
	opts.color_log = 1;
	pusb_log_init(&opts);
	assert_int_equal(0, pusb_log_get_debug());
	assert_int_equal(0, pusb_log_get_quiet());
	assert_int_equal(1, pusb_log_get_color());
}

/* ── Key regression guard: values are copied, not pointed to ──
 *
 * The bug fixed in #355 stored &opts (stack) in a process-wide static.
 * This test proves the current implementation copies field values: mutating
 * opts after pusb_log_init() must NOT change the log state.
 */
static void test_log_init_copies_not_pointer(void **state)
{
	(void)state;
	t_pusb_options opts;
	memset(&opts, 0, sizeof(opts));
	opts.debug = 1;
	opts.quiet = 1;
	opts.color_log = 1;

	pusb_log_init(&opts);

	/* Mutate the source struct — internal log state must NOT follow */
	opts.debug = 0;
	opts.quiet = 0;
	opts.color_log = 0;

	assert_int_equal(1, pusb_log_get_debug());
	assert_int_equal(1, pusb_log_get_quiet());
	assert_int_equal(1, pusb_log_get_color());
}

/* ── Sequential calls overwrite previous values ── */

static void test_log_init_sequential_override(void **state)
{
	(void)state;
	t_pusb_options opts;
	memset(&opts, 0, sizeof(opts));

	opts.debug = 1;
	pusb_log_init(&opts);
	assert_int_equal(1, pusb_log_get_debug());

	opts.debug = 0;
	pusb_log_init(&opts);
	assert_int_equal(0, pusb_log_get_debug());
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_log_init_null_zeroes_all),
		cmocka_unit_test(test_log_init_sets_debug),
		cmocka_unit_test(test_log_init_sets_quiet),
		cmocka_unit_test(test_log_init_sets_color),
		cmocka_unit_test(test_log_init_copies_not_pointer),
		cmocka_unit_test(test_log_init_sequential_override),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
