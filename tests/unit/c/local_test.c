/*
 * Unit tests for src/local.c.
 * Include the source directly to cover static helpers used with fixed-size
 * utmpx fields.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
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

static void test_utmpx_field_equals_rejects_prefix_match(void **state)
{
	(void)state;
	char field[8] = {0};
	snprintf(field, sizeof(field), "%s", "alice2");

	assert_int_equal(0, pusb_utmpx_field_equals(field, sizeof(field), "alice"));
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

static void test_utmpx_field_starts_with_pts_slave(void **state)
{
	(void)state;
	char field[8] = {0};
	snprintf(field, sizeof(field), "%s", "pts/0");

	assert_int_equal(1, pusb_utmpx_field_starts_with(field, sizeof(field), "pts"));
}

static void test_utmpx_field_starts_with_rejects_long_prefix(void **state)
{
	(void)state;
	char field[3] = {'p', 't', 's'};

	assert_int_equal(0, pusb_utmpx_field_starts_with(field, sizeof(field), "pts/"));
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_utmpx_field_equals_exact_nul_padded),
		cmocka_unit_test(test_utmpx_field_equals_rejects_prefix_match),
		cmocka_unit_test(test_utmpx_field_equals_full_width_field),
		cmocka_unit_test(test_utmpx_field_starts_with_tty_number),
		cmocka_unit_test(test_utmpx_field_starts_with_pts_slave),
		cmocka_unit_test(test_utmpx_field_starts_with_rejects_long_prefix),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
