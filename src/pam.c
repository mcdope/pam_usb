/*
 * Copyright (c) 2003-2007 Andrea Luzzardi <scox@sig11.org>
 *
 * This file is part of the pam_usb project. pam_usb is free software;
 * you can redistribute it and/or modify it under the terms of the GNU General
 * Public License version 2, as published by the Free Software Foundation.
 *
 * pam_usb is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
 * Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

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

		/**
		 * <PolicyKit workaround>
		 * polkit is REALLY unhappy if we don't do emit PAM_PROMPT_ECHO_OFF
		 * Sadly this requires the user to press a button after authenticating, this is unavoidable by polkit design
		 * @see https://gemini.google.com/share/6ca3bdb2436f
		 **/
		if (strcmp(service, "polkit-1") == 0) {
			log_debug("PolicyKit authentication detected, applying workaround.\n");
			const struct pam_conv *conv;
			int polkit_conv_retval = pam_get_item(pamh, PAM_CONV, (const void **)&conv);

			if (polkit_conv_retval == PAM_SUCCESS && conv != NULL && conv->conv != NULL) {
				int polkit_retval;
				char *response = NULL;

				polkit_retval = pam_prompt(pamh, PAM_PROMPT_ECHO_OFF, &response, "Password: ");

				if (polkit_retval == PAM_SUCCESS) {
					if (response != NULL) {
						free(response);
					}
				}
			}
		}
		// </PolicyKit workaround>

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
