/*
 * Unit tests for src/pam.c
 * Tests pusb_do_auth decision paths via pam_sm_authenticate and pam_sm_acct_mgmt.
 * All external calls (PAM, conf, device, local) are intercepted at link time
 * with --wrap so no real hardware or config file is needed.
 */

#define PAM_SM_AUTH
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>
#include <security/pam_modules.h>

#include "../../../src/conf.h"

/* ── forward declarations of the functions under test ── */
extern int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv);
extern int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc, const char **argv);

/* ── mock state ── */
static int         g_service_retval;
static int         g_rhost_retval;
static const char *g_service;
static const char *g_user;
static int         g_user_retval;
static const char *g_rhost;
static int         g_conf_init_retval;
static int         g_conf_parse_retval;
static int         g_enable;
static int         g_deny_remote;
static int         g_local_login_retval;
static int         g_device_check_retval;

static void reset_mocks(void)
{
	g_service_retval      = PAM_SUCCESS;
	g_rhost_retval        = PAM_SUCCESS;
	g_service             = "sshd";
	g_user                = "testuser";
	g_user_retval         = PAM_SUCCESS;
	g_rhost               = NULL;
	g_conf_init_retval    = 1;
	g_conf_parse_retval   = 1;
	g_enable              = 1;
	g_deny_remote         = 0;
	g_local_login_retval  = 1;
	g_device_check_retval = 1;
}

/* ── PAM wrappers ── */
int __wrap_pam_get_item(pam_handle_t *pamh, int item_type, const void **item)
{
	(void)pamh;
	if (item_type == PAM_SERVICE)
	{
		*item = g_service;
		return g_service_retval;
	}
	if (item_type == PAM_RHOST)
	{
		*item = g_rhost;
		return g_rhost_retval;
	}
	return PAM_AUTH_ERR;
}

int __wrap_pam_get_user(pam_handle_t *pamh, const char **user, const char *prompt)
{
	(void)pamh; (void)prompt;
	*user = g_user;
	return g_user_retval;
}

/* ── conf / device / local wrappers ── */
int __wrap_pusb_conf_init(t_pusb_options *opts)
{
	memset(opts, 0, sizeof(*opts));
	opts->enable = 1;
	return g_conf_init_retval;
}

int __wrap_pusb_conf_parse(const char *file, t_pusb_options *opts,
                            const char *user, const char *service)
{
	(void)file; (void)user; (void)service;
	opts->enable      = g_enable;
	opts->deny_remote = g_deny_remote;
	return g_conf_parse_retval;
}

void __wrap_pusb_conf_free(t_pusb_options *opts) { (void)opts; }

int __wrap_pusb_local_login(t_pusb_options *opts, const char *user, const char *service)
{
	(void)opts; (void)user; (void)service;
	return g_local_login_retval;
}

int __wrap_pusb_device_check(t_pusb_options *opts, const char *user)
{
	(void)opts; (void)user;
	return g_device_check_retval;
}

/* ── tests: pam_sm_authenticate ── */

static void test_auth_pam_service_failure(void **state)
{
	(void)state;
	reset_mocks();
	g_service_retval = PAM_AUTH_ERR;
	assert_int_equal(PAM_AUTH_ERR, pam_sm_authenticate(NULL, 0, 0, NULL));
}

static void test_auth_pam_user_failure(void **state)
{
	(void)state;
	reset_mocks();
	g_user_retval = PAM_AUTH_ERR;
	g_user        = NULL;
	assert_int_equal(PAM_AUTH_ERR, pam_sm_authenticate(NULL, 0, 0, NULL));
}

static void test_auth_conf_parse_failure(void **state)
{
	(void)state;
	reset_mocks();
	g_conf_parse_retval = 0;
	assert_int_equal(PAM_AUTH_ERR, pam_sm_authenticate(NULL, 0, 0, NULL));
}

static void test_auth_disabled_returns_ignore(void **state)
{
	(void)state;
	reset_mocks();
	g_enable = 0;
	assert_int_equal(PAM_IGNORE, pam_sm_authenticate(NULL, 0, 0, NULL));
}

static void test_auth_deny_remote_rhost_set_returns_ignore(void **state)
{
	(void)state;
	reset_mocks();
	g_deny_remote = 1;
	g_rhost       = "192.168.1.1";
	assert_int_equal(PAM_IGNORE, pam_sm_authenticate(NULL, 0, 0, NULL));
}

static void test_auth_deny_remote_rhost_retrieval_fails(void **state)
{
	(void)state;
	reset_mocks();
	g_deny_remote  = 1;
	g_rhost_retval = PAM_AUTH_ERR;
	assert_int_equal(PAM_AUTH_ERR, pam_sm_authenticate(NULL, 0, 0, NULL));
}

static void test_auth_local_login_denied(void **state)
{
	(void)state;
	reset_mocks();
	g_local_login_retval = 0;
	assert_int_equal(PAM_AUTH_ERR, pam_sm_authenticate(NULL, 0, 0, NULL));
}

static void test_auth_device_check_success(void **state)
{
	(void)state;
	reset_mocks();
	assert_int_equal(PAM_SUCCESS, pam_sm_authenticate(NULL, 0, 0, NULL));
}

static void test_auth_device_check_failure(void **state)
{
	(void)state;
	reset_mocks();
	g_device_check_retval = 0;
	assert_int_equal(PAM_AUTH_ERR, pam_sm_authenticate(NULL, 0, 0, NULL));
}

/* ── tests: pam_sm_acct_mgmt ── */

static void test_acct_mgmt_success(void **state)
{
	(void)state;
	reset_mocks();
	assert_int_equal(PAM_SUCCESS, pam_sm_acct_mgmt(NULL, 0, 0, NULL));
}

static void test_acct_mgmt_disabled_returns_ignore(void **state)
{
	(void)state;
	reset_mocks();
	g_enable = 0;
	assert_int_equal(PAM_IGNORE, pam_sm_acct_mgmt(NULL, 0, 0, NULL));
}

static void test_acct_mgmt_device_check_failure(void **state)
{
	(void)state;
	reset_mocks();
	g_device_check_retval = 0;
	assert_int_equal(PAM_AUTH_ERR, pam_sm_acct_mgmt(NULL, 0, 0, NULL));
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_auth_pam_service_failure),
		cmocka_unit_test(test_auth_pam_user_failure),
		cmocka_unit_test(test_auth_conf_parse_failure),
		cmocka_unit_test(test_auth_disabled_returns_ignore),
		cmocka_unit_test(test_auth_deny_remote_rhost_set_returns_ignore),
		cmocka_unit_test(test_auth_deny_remote_rhost_retrieval_fails),
		cmocka_unit_test(test_auth_local_login_denied),
		cmocka_unit_test(test_auth_device_check_success),
		cmocka_unit_test(test_auth_device_check_failure),
		cmocka_unit_test(test_acct_mgmt_success),
		cmocka_unit_test(test_acct_mgmt_disabled_returns_ignore),
		cmocka_unit_test(test_acct_mgmt_device_check_failure),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
