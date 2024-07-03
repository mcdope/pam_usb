#define PAM_SM_AUTH
#include <security/pam_modules.h>
#include <security/_pam_macros.h>
#include <string.h>

#include "version.h"
#include "conf.h"
#include "log.h"
#include "local.h"
#include "device.h"

PAM_EXTERN
int pam_sm_authenticate(
	pam_handle_t *pamh,
	int flags,
	int argc,
	const char **argv
)
{
	t_pusb_options opts;
	const char *service = NULL;
	const char *user = NULL;
	const char *rhost = NULL;
	char *conf_file = PUSB_CONF_FILE;
	int retval;

	memset(&opts, 0, sizeof(t_pusb_options)); // Initialize opts
	pusb_log_init(&opts);

	retval = pam_get_item(pamh, PAM_SERVICE, (const void **)&service);
	if (retval != PAM_SUCCESS)
	{
		log_error("Unable to retrieve the PAM service name.\n");
		return PAM_AUTH_ERR;
	}

	if (pam_get_user(pamh, &user, NULL) != PAM_SUCCESS || !user || !*user)
	{
		log_error("Unable to retrieve the PAM user name.\n");
		return PAM_AUTH_ERR;
	}

	if (argc > 1 && !strcmp(argv[0], "-c"))
	{
		conf_file = (char *)argv[1];
	}

	if (!pusb_conf_init(&opts))
	{
		return PAM_AUTH_ERR;
	}

	if (!pusb_conf_parse(conf_file, &opts, user, service))
	{
		return PAM_AUTH_ERR;
	}

	if (!opts.enable)
	{
		log_debug("Not enabled, exiting...\n");
		return PAM_IGNORE;
	}

	if (opts.deny_remote)
	{
		retval = pam_get_item(pamh, PAM_RHOST, (const void **)&rhost);
		if (retval != PAM_SUCCESS)
		{
			log_error("Unable to retrieve PAM_RHOST.\n");
			return PAM_AUTH_ERR;
		}
		else if (rhost != NULL && *rhost != '\0')
		{
			log_debug("RHOST is set (%s), must be a remote request - disabling myself for this request!\n", rhost);
			return PAM_IGNORE;
		}
	}

	log_info("Authentication request for user \"%s\" (%s)\n", user, service);

	if (pusb_local_login(&opts, user, service) != 1)
	{
		log_error("Access denied.\n");
		return PAM_AUTH_ERR;
	}
	if (pusb_device_check(&opts, user))
	{
		log_info("Access granted.\n");
		return PAM_SUCCESS;
	}
	log_error("Access denied.\n");
	return PAM_AUTH_ERR;
}

PAM_EXTERN
int pam_sm_setcred(
	pam_handle_t *pamh,
	int flags,
	int argc,
	const char **argv
)
{
	return PAM_SUCCESS;
}

PAM_EXTERN
int pam_sm_acct_mgmt(
	pam_handle_t *pamh,
	int flags,
	int argc,
	const char **argv
)
{
	t_pusb_options opts;
	const char *service = NULL;
	const char *user = NULL;
	const char *rhost = NULL;
	char *conf_file = PUSB_CONF_FILE;
	int retval;

	memset(&opts, 0, sizeof(t_pusb_options)); // Initialize opts
	pusb_log_init(&opts);

	retval = pam_get_item(pamh, PAM_SERVICE, (const void **)&service);
	if (retval != PAM_SUCCESS)
	{
		log_error("Unable to retrieve the PAM service name.\n");
		return PAM_AUTH_ERR;
	}

	if (pam_get_user(pamh, &user, NULL) != PAM_SUCCESS || !user || !*user)
	{
		log_error("Unable to retrieve the PAM user name.\n");
		return PAM_AUTH_ERR;
	}

	if (argc > 1 && !strcmp(argv[0], "-c"))
	{
		conf_file = (char *)argv[1];
	}

	if (!pusb_conf_init(&opts))
	{
		return PAM_AUTH_ERR;
	}

	if (!pusb_conf_parse(conf_file, &opts, user, service))
	{
		return PAM_AUTH_ERR;
	}

	if (!opts.enable)
	{
		log_debug("Not enabled, exiting...\n");
		return PAM_IGNORE;
	}

	if (opts.deny_remote)
	{
		retval = pam_get_item(pamh, PAM_RHOST, (const void **)&rhost);
		if (retval != PAM_SUCCESS)
		{
			log_error("Unable to retrieve PAM_RHOST.\n");
			return PAM_AUTH_ERR;
		}
		else if (rhost != NULL && *rhost != '\0')
		{
			log_debug("RHOST is set (%s), must be a remote request - disabling myself for this request!\n", rhost);
			return PAM_IGNORE;
		}
	}

	log_info("pam_usb v%s\n", PUSB_VERSION);
	log_info("Account request for user \"%s\" (%s)\n", user, service);

	if (pusb_local_login(&opts, user, service) != 1)
	{
		log_error("Access denied.\n");
		return PAM_AUTH_ERR;
	}
	if (pusb_device_check(&opts, user))
	{
		log_info("Access granted.\n");
		return PAM_SUCCESS;
	}
	log_error("Access denied.\n");
	return PAM_AUTH_ERR;
}


#ifdef PAM_STATIC

struct pam_module _pam_usb_modstruct = {
	"pam_usb",
	pam_sm_authenticate,
	pam_sm_setcred,
	pam_sm_acct_mgmt,
	NULL,
	NULL,
	NULL
};

#endif
