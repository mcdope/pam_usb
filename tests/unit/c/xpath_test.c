/*
 * Unit tests for src/xpath.c
 * Tests pusb_xpath_get_bool, _get_time, _get_int, _get_string, _get_string_list
 */

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
		cmocka_unit_test(test_int_value),
		cmocka_unit_test(test_int_zero),
		cmocka_unit_test(test_string_value),
		cmocka_unit_test(test_string_whitespace_stripped),
		cmocka_unit_test(test_string_too_long),
		cmocka_unit_test(test_string_list_multiple),
		cmocka_unit_test(test_string_list_single),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
