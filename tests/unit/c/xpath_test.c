/*
 * Unit tests for src/xpath.c
 * Tests pusb_xpath_get_bool, _get_time, _get_int, _get_string, _get_string_list
 */

#define _GNU_SOURCE
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>
#include <libxml/parser.h>
#include "../../../src/xpath.h"
#include "../../../src/conf.h"
#include "../../../src/log.h"

static xmlDocPtr make_doc(const char *xml)
{
	return xmlReadMemory(xml, (int)strlen(xml), "mem.xml", NULL, 0);
}

/* ── pusb_xpath_get_bool ── */

static void test_bool_true(void **state)
{
	(void)state;
	xmlDocPtr doc = make_doc("<r><v>true</v></r>");
	int val = 0;
	assert_int_equal(1, pusb_xpath_get_bool(doc, "//r/v", &val));
	assert_int_equal(1, val);
	xmlFreeDoc(doc);
}

static void test_bool_false(void **state)
{
	(void)state;
	xmlDocPtr doc = make_doc("<r><v>false</v></r>");
	int val = 99;
	assert_int_equal(1, pusb_xpath_get_bool(doc, "//r/v", &val));
	assert_int_equal(0, val);
	xmlFreeDoc(doc);
}

static void test_bool_missing_node(void **state)
{
	(void)state;
	xmlDocPtr doc = make_doc("<r><v>true</v></r>");
	int val = 42;
	assert_int_equal(0, pusb_xpath_get_bool(doc, "//r/missing", &val));
	assert_int_equal(42, val);
	xmlFreeDoc(doc);
}

/* ── pusb_xpath_get_time ── */

static void test_time_bare_int(void **state)
{
	(void)state;
	xmlDocPtr doc = make_doc("<r><v>42</v></r>");
	time_t val = 0;
	assert_int_equal(1, pusb_xpath_get_time(doc, "//r/v", &val));
	assert_int_equal(42, (int)val);
	xmlFreeDoc(doc);
}

static void test_time_seconds(void **state)
{
	(void)state;
	xmlDocPtr doc = make_doc("<r><v>30s</v></r>");
	time_t val = 0;
	assert_int_equal(1, pusb_xpath_get_time(doc, "//r/v", &val));
	assert_int_equal(30, (int)val);
	xmlFreeDoc(doc);
}

static void test_time_minutes(void **state)
{
	(void)state;
	xmlDocPtr doc = make_doc("<r><v>5m</v></r>");
	time_t val = 0;
	assert_int_equal(1, pusb_xpath_get_time(doc, "//r/v", &val));
	assert_int_equal(300, (int)val);
	xmlFreeDoc(doc);
}

static void test_time_hours(void **state)
{
	(void)state;
	xmlDocPtr doc = make_doc("<r><v>2h</v></r>");
	time_t val = 0;
	assert_int_equal(1, pusb_xpath_get_time(doc, "//r/v", &val));
	assert_int_equal(7200, (int)val);
	xmlFreeDoc(doc);
}

static void test_time_days(void **state)
{
	(void)state;
	xmlDocPtr doc = make_doc("<r><v>1d</v></r>");
	time_t val = 0;
	assert_int_equal(1, pusb_xpath_get_time(doc, "//r/v", &val));
	assert_int_equal(86400, (int)val);
	xmlFreeDoc(doc);
}

static void test_time_invalid_suffix(void **state)
{
	(void)state;
	xmlDocPtr doc = make_doc("<r><v>5x</v></r>");
	time_t val = 0;
	assert_int_equal(0, pusb_xpath_get_time(doc, "//r/v", &val));
	xmlFreeDoc(doc);
}

static void test_time_negative(void **state)
{
	(void)state;
	xmlDocPtr doc = make_doc("<r><v>-5s</v></r>");
	time_t val = 999;
	assert_int_equal(0, pusb_xpath_get_time(doc, "//r/v", &val));
	assert_true(val == 999);
	xmlFreeDoc(doc);
}

static void test_time_overflow(void **state)
{
	(void)state;
	xmlDocPtr doc = make_doc("<r><v>9999999999999999999d</v></r>");
	time_t val = 999;
	assert_int_equal(0, pusb_xpath_get_time(doc, "//r/v", &val));
	assert_true(val == 999);
	xmlFreeDoc(doc);
}

static void test_time_overflow_via_multiplication(void **state)
{
	(void)state;
	/* 200000000000000 * 86400 > LONG_MAX; fits strtol cleanly, only the
	   multiplication overflow guard catches it */
	xmlDocPtr doc = make_doc("<r><v>200000000000000d</v></r>");
	time_t val = 999;
	assert_int_equal(0, pusb_xpath_get_time(doc, "//r/v", &val));
	assert_true(val == 999);
	xmlFreeDoc(doc);
}

static void test_time_empty_numeric_part(void **state)
{
	(void)state;
	xmlDocPtr doc = make_doc("<r><v>d</v></r>");
	time_t val = 999;
	assert_int_equal(0, pusb_xpath_get_time(doc, "//r/v", &val));
	assert_true(val == 999);
	xmlFreeDoc(doc);
}

static void test_time_non_ascii_suffix(void **state)
{
	(void)state;
	/* A trailing byte with the high bit set (here the UTF-8 'é', last byte
	 * 0xA9) must be rejected as an invalid modifier, never passed to isdigit()
	 * as a negative int (UB, CERT STR37-C). Pins the (unsigned char) cast in
	 * pusb_xpath_get_time; on glibc the value is rejected either way, so this
	 * is a portability/UB-hardening pin rather than a behavioural red/green. */
	xmlDocPtr doc = make_doc("<r><v>30\xc3\xa9</v></r>");
	time_t val = 999;
	assert_int_equal(0, pusb_xpath_get_time(doc, "//r/v", &val));
	assert_true(val == 999);
	xmlFreeDoc(doc);
}

/* ── pusb_xpath_get_int ── */

static void test_int_value(void **state)
{
	(void)state;
	xmlDocPtr doc = make_doc("<r><v>42</v></r>");
	int val = 0;
	assert_int_equal(1, pusb_xpath_get_int(doc, "//r/v", &val));
	assert_int_equal(42, val);
	xmlFreeDoc(doc);
}

static void test_int_zero(void **state)
{
	(void)state;
	xmlDocPtr doc = make_doc("<r><v>0</v></r>");
	int val = 99;
	assert_int_equal(1, pusb_xpath_get_int(doc, "//r/v", &val));
	assert_int_equal(0, val);
	xmlFreeDoc(doc);
}

static void test_int_overflow(void **state)
{
	(void)state;
	xmlDocPtr doc = make_doc("<r><v>9999999999</v></r>");
	int val = 42;
	assert_int_equal(0, pusb_xpath_get_int(doc, "//r/v", &val));
	assert_int_equal(42, val);
	xmlFreeDoc(doc);
}

static void test_int_underflow(void **state)
{
	(void)state;
	xmlDocPtr doc = make_doc("<r><v>-9999999999</v></r>");
	int val = 42;
	assert_int_equal(0, pusb_xpath_get_int(doc, "//r/v", &val));
	assert_int_equal(42, val);
	xmlFreeDoc(doc);
}

static void test_int_invalid_chars(void **state)
{
	(void)state;
	xmlDocPtr doc = make_doc("<r><v>42abc</v></r>");
	int val = 42;
	assert_int_equal(0, pusb_xpath_get_int(doc, "//r/v", &val));
	assert_int_equal(42, val);
	xmlFreeDoc(doc);
}

/* ── pusb_xpath_get_string ── */

static void test_string_value(void **state)
{
	(void)state;
	xmlDocPtr doc = make_doc("<r><v>hello</v></r>");
	char buf[64] = {0};
	assert_int_equal(1, pusb_xpath_get_string(doc, "//r/v", buf, sizeof(buf)));
	assert_string_equal("hello", buf);
	xmlFreeDoc(doc);
}

static void test_string_whitespace_stripped(void **state)
{
	(void)state;
	xmlDocPtr doc = make_doc("<r><v>  hello  </v></r>");
	char buf[64] = {0};
	assert_int_equal(1, pusb_xpath_get_string(doc, "//r/v", buf, sizeof(buf)));
	assert_string_equal("hello", buf);
	xmlFreeDoc(doc);
}

static void test_string_too_long(void **state)
{
	(void)state;
	xmlDocPtr doc = make_doc("<r><v>abcdefgh</v></r>");
	char buf[4] = {0};
	/* size=4 can hold at most 3 chars + NUL; "abcdefgh" (8 chars) must fail */
	assert_int_equal(0, pusb_xpath_get_string(doc, "//r/v", buf, sizeof(buf)));
	xmlFreeDoc(doc);
}

static void test_string_exact_buffer_size_rejected(void **state)
{
	(void)state;
	xmlDocPtr doc = make_doc("<r><v>abcd</v></r>");
	char buf[4] = {0};
	/* size=4 needs one byte for NUL, so 4 content bytes must fail. */
	assert_int_equal(0, pusb_xpath_get_string(doc, "//r/v", buf, sizeof(buf)));
	xmlFreeDoc(doc);
}

static void test_string_zero_size_rejected(void **state)
{
	(void)state;
	xmlDocPtr doc = make_doc("<r><v>a</v></r>");
	char buf[1] = {0};
	assert_int_equal(0, pusb_xpath_get_string(doc, "//r/v", buf, 0));
	xmlFreeDoc(doc);
}

/* ── pusb_xpath_get_string_list ── */

static void test_string_list_multiple(void **state)
{
	(void)state;
	xmlDocPtr doc = make_doc("<r><v>alpha</v><v>beta</v><v>gamma</v></r>");
	char e0[64] = {0}, e1[64] = {0}, e2[64] = {0};
	char *vals[3] = {e0, e1, e2};
	assert_int_equal(1, pusb_xpath_get_string_list(doc, "//r/v", vals, sizeof(e0), 3));
	assert_string_equal("alpha", e0);
	assert_string_equal("beta",  e1);
	assert_string_equal("gamma", e2);
	xmlFreeDoc(doc);
}

static void test_string_list_single(void **state)
{
	(void)state;
	xmlDocPtr doc = make_doc("<r><v>onlyone</v></r>");
	char e0[64] = {0}, e1[64] = {0};
	char *vals[2] = {e0, e1};
	assert_int_equal(1, pusb_xpath_get_string_list(doc, "//r/v", vals, sizeof(e0), 2));
	assert_string_equal("onlyone", e0);
	assert_string_equal("", e1);
	xmlFreeDoc(doc);
}

/* ── pusb_xpath_count_nodes ── */

static void test_count_nodes_returns_correct_count(void **state)
{
	(void)state;
	xmlDocPtr doc = make_doc("<r><v>a</v><v>b</v><v>c</v></r>");
	assert_int_equal(3, pusb_xpath_count_nodes(doc, "//r/v"));
	xmlFreeDoc(doc);
}

static void test_count_nodes_returns_zero_for_missing(void **state)
{
	(void)state;
	xmlDocPtr doc = make_doc("<r><v>a</v></r>");
	assert_int_equal(0, pusb_xpath_count_nodes(doc, "//r/missing"));
	xmlFreeDoc(doc);
}

static void test_count_nodes_single_node(void **state)
{
	(void)state;
	xmlDocPtr doc = make_doc("<r><v>only</v></r>");
	assert_int_equal(1, pusb_xpath_count_nodes(doc, "//r/v"));
	xmlFreeDoc(doc);
}

/* ── main ── */

int main(void)
{
	t_pusb_options opts = {0};
	opts.debug = 0;
	opts.quiet = 1;
	pusb_log_init(&opts);

	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_bool_true),
		cmocka_unit_test(test_bool_false),
		cmocka_unit_test(test_bool_missing_node),
		cmocka_unit_test(test_time_bare_int),
		cmocka_unit_test(test_time_seconds),
		cmocka_unit_test(test_time_minutes),
		cmocka_unit_test(test_time_hours),
		cmocka_unit_test(test_time_days),
		cmocka_unit_test(test_time_invalid_suffix),
		cmocka_unit_test(test_time_negative),
		cmocka_unit_test(test_time_overflow),
		cmocka_unit_test(test_time_overflow_via_multiplication),
		cmocka_unit_test(test_time_empty_numeric_part),
		cmocka_unit_test(test_time_non_ascii_suffix),
		cmocka_unit_test(test_int_value),
		cmocka_unit_test(test_int_zero),
		cmocka_unit_test(test_int_overflow),
		cmocka_unit_test(test_int_underflow),
		cmocka_unit_test(test_int_invalid_chars),
		cmocka_unit_test(test_string_value),
		cmocka_unit_test(test_string_whitespace_stripped),
		cmocka_unit_test(test_string_too_long),
		cmocka_unit_test(test_string_exact_buffer_size_rejected),
		cmocka_unit_test(test_string_zero_size_rejected),
		cmocka_unit_test(test_string_list_multiple),
		cmocka_unit_test(test_string_list_single),
		cmocka_unit_test(test_count_nodes_returns_correct_count),
		cmocka_unit_test(test_count_nodes_returns_zero_for_missing),
		cmocka_unit_test(test_count_nodes_single_node),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
