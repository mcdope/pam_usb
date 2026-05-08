/*
 * Unit tests for src/conf.c
 * Tests pusb_conf_init defaults, pusb_conf_parse superuser filtering,
 * Generic vendor/model preservation, and error paths.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cmocka.h>
#include "../../../src/conf.h"
#include "../../../src/log.h"

#define FIXTURE_CONF "tests/unit/c/fixtures/pam_usb_test.conf"

/* ── pusb_conf_init defaults ── */

static void test_conf_init_defaults(void **state)
{
	(void)state;
	t_pusb_options opts;
	assert_int_equal(1, pusb_conf_init(&opts));
	assert_int_equal(1, opts.enable);
	assert_int_equal(1, opts.one_time_pad);
	assert_int_equal(1, opts.deny_remote);
	assert_true(opts.probe_timeout > 0);
	assert_true(opts.pad_expiration > 0);
	assert_true(opts.hostname[0] != '\0');
}

/* ── superuser filtering ── */

static void test_superuser_device_present_for_superuser_service(void **state)
{
	(void)state;
	t_pusb_options opts;
	pusb_conf_init(&opts);
	assert_int_equal(1, pusb_conf_parse(FIXTURE_CONF, &opts, "user_mixed", "sudo"));

	int found_stick2 = 0;
	for (int i = 0; i < 10; i++) {
		if (strcmp(opts.device_list[i].name, "stick2") == 0) {
			found_stick2 = 1;
			assert_int_equal(1, opts.device_list[i].superuser);
		}
	}
	assert_int_equal(1, found_stick2);
}

static void test_nonsuperuser_device_removed_for_superuser_service(void **state)
{
	(void)state;
	t_pusb_options opts;
	pusb_conf_init(&opts);
	assert_int_equal(1, pusb_conf_parse(FIXTURE_CONF, &opts, "user_mixed", "sudo"));

	for (int i = 0; i < 10; i++) {
		if (opts.device_list[i].name[0] != '\0') {
			if (strcmp(opts.device_list[i].name, "stick1") == 0) {
				fail_msg("stick1 (non-superuser) survived superuser filter for sudo service");
			}
		}
	}
}

static void test_plain_user_no_superuser_device_gets_empty_list(void **state)
{
	(void)state;
	t_pusb_options opts;
	pusb_conf_init(&opts);
	/* user_plain has only stick1, which is not superuser */
	assert_int_equal(1, pusb_conf_parse(FIXTURE_CONF, &opts, "user_plain", "sudo"));

	for (int i = 0; i < 10; i++) {
		assert_true(opts.device_list[i].name[0] == '\0');
	}
}

static void test_backward_compat_no_superuser_service(void **state)
{
	(void)state;
	t_pusb_options opts;
	pusb_conf_init(&opts);
	/* "login" service has no superuser option, so all devices must survive */
	assert_int_equal(1, pusb_conf_parse(FIXTURE_CONF, &opts, "user_mixed", "login"));

	int found_stick1 = 0, found_stick2 = 0;
	for (int i = 0; i < 10; i++) {
		if (strcmp(opts.device_list[i].name, "stick1") == 0) found_stick1 = 1;
		if (strcmp(opts.device_list[i].name, "stick2") == 0) found_stick2 = 1;
	}
	assert_int_equal(1, found_stick1);
	assert_int_equal(1, found_stick2);
}

/* ── Generic vendor/model preservation ── */

static void test_generic_vendor_model_preserved(void **state)
{
	(void)state;
	t_pusb_options opts;
	pusb_conf_init(&opts);
	assert_int_equal(1, pusb_conf_parse(FIXTURE_CONF, &opts, "user_generic", "login"));

	int found = 0;
	for (int i = 0; i < 10; i++) {
		if (strcmp(opts.device_list[i].name, "stick3") == 0) {
			found = 1;
			assert_string_equal("Generic", opts.device_list[i].vendor);
			assert_string_equal("Generic", opts.device_list[i].model);
		}
	}
	assert_int_equal(1, found);
}

/* ── Error paths ── */

static void test_parse_nonexistent_file(void **state)
{
	(void)state;
	t_pusb_options opts;
	pusb_conf_init(&opts);
	assert_int_equal(0, pusb_conf_parse("/nonexistent/file.conf", &opts, "testuser", "login"));
}

static void test_parse_invalid_xml(void **state)
{
	(void)state;
	char tmpfile[] = "/tmp/pamusb_test_invalid_XXXXXX";
	int fd = mkstemp(tmpfile);
	assert_true(fd >= 0);
	const char *bad_xml = "<not xml";
	write(fd, bad_xml, strlen(bad_xml));
	close(fd);

	t_pusb_options opts;
	pusb_conf_init(&opts);
	assert_int_equal(0, pusb_conf_parse(tmpfile, &opts, "testuser", "login"));
	unlink(tmpfile);
}

static void test_parse_username_too_long(void **state)
{
	(void)state;
	/* CONF_USER_MAXLEN is 32; a username of 33 chars must be rejected */
	t_pusb_options opts;
	pusb_conf_init(&opts);
	assert_int_equal(0, pusb_conf_parse(FIXTURE_CONF, &opts,
		"abcdefghijklmnopqrstuvwxyz1234567", "login"));
}

static void test_parse_user_not_in_conf(void **state)
{
	(void)state;
	t_pusb_options opts;
	pusb_conf_init(&opts);
	assert_int_equal(0, pusb_conf_parse(FIXTURE_CONF, &opts, "nobody_configured", "login"));
}

static void test_superuser_attribute_case_sensitive(void **state)
{
	(void)state;
	/* superuser="TRUE" (uppercase) must NOT match XPath device[@superuser='true'] */
	char tmpfile[] = "/tmp/pamusb_test_case_XXXXXX";
	int fd = mkstemp(tmpfile);
	assert_true(fd >= 0);
	const char *xml =
		"<?xml version=\"1.0\"?>"
		"<configuration>"
		"  <devices>"
		"    <device id=\"devA\"><vendor>V</vendor><model>M</model>"
		"      <serial>S001</serial><volume_uuid>UUID-A</volume_uuid></device>"
		"  </devices>"
		"  <users>"
		"    <user id=\"testuser\">"
		"      <device superuser=\"TRUE\">devA</device>"
		"    </user>"
		"  </users>"
		"  <services>"
		"    <service id=\"sudo\"><option name=\"superuser\">true</option></service>"
		"  </services>"
		"</configuration>";
	write(fd, xml, strlen(xml));
	close(fd);

	t_pusb_options opts;
	pusb_conf_init(&opts);
	assert_int_equal(1, pusb_conf_parse(tmpfile, &opts, "testuser", "sudo"));

	/* service requires superuser; devA has superuser="TRUE" (case mismatch) so it's
	   NOT flagged as superuser and must be filtered out */
	for (int i = 0; i < 10; i++) {
		assert_true(opts.device_list[i].name[0] == '\0');
	}
	unlink(tmpfile);
}

/* ── main ── */

int main(void)
{
	t_pusb_options init_opts = {0};
	init_opts.debug = 0;
	init_opts.quiet = 1;
	pusb_log_init(&init_opts);

	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_conf_init_defaults),
		cmocka_unit_test(test_superuser_device_present_for_superuser_service),
		cmocka_unit_test(test_nonsuperuser_device_removed_for_superuser_service),
		cmocka_unit_test(test_plain_user_no_superuser_device_gets_empty_list),
		cmocka_unit_test(test_backward_compat_no_superuser_service),
		cmocka_unit_test(test_generic_vendor_model_preserved),
		cmocka_unit_test(test_parse_nonexistent_file),
		cmocka_unit_test(test_parse_invalid_xml),
		cmocka_unit_test(test_parse_username_too_long),
		cmocka_unit_test(test_parse_user_not_in_conf),
		cmocka_unit_test(test_superuser_attribute_case_sensitive),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
