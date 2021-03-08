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

#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include "conf.h"
#include "log.h"
#include "device.h"
#include "local.h"

static void pusb_check_conf_dump(t_pusb_options *opts, const char *username,
		const char *service)
{
	fprintf(stdout, "Configuration dump for user %s (service: %s):\n",
			username, service);
	fprintf(stdout, "enable\t\t\t: %s\n", opts->enable ? "true" : "false");
	fprintf(stdout, "debug\t\t\t: %s\n", opts->debug ? "true" : "false");
	fprintf(stdout, "quiet\t\t\t: %s\n", opts->quiet ? "true" : "false");
	fprintf(stdout, "color_log\t\t: %s\n", opts->color_log ? "true" : "false");
	fprintf(stdout, "one_time_pad\t\t: %s\n",
			opts->one_time_pad ? "true" : "false");
	fprintf(stdout, "deny_remote\t\t: %s\n",
			opts->deny_remote ? "true" : "false");
	fprintf(stdout, "pad_expiration\t\t: %u seconds\n", (unsigned int)opts->pad_expiration);
	fprintf(stdout, "probe_timeout\t\t: %d seconds\n", (unsigned int)opts->probe_timeout);
	fprintf(stdout, "hostname\t\t: %s\n", opts->hostname);
	fprintf(stdout, "system_pad_directory\t: %s\n",
			opts->system_pad_directory);
	fprintf(stdout, "device_pad_directory\t: %s\n",
			opts->device_pad_directory);
}

static int pusb_check_perform_authentication(t_pusb_options *opts,
		const char *user,
		const char *service)
{
	int		retval;

	if (!opts->enable)
	{
		log_debug("Not enabled, exiting...\n");
		return (0);
	}
	log_info("Authentication request for user \"%s\" (%s)\n",
			user, service);
	if (!pusb_local_login(opts, user, service))
	{
		log_error("Access denied.\n");
		return (0);
	}
	retval = pusb_device_check(opts, user);
	if (retval)
		log_info("Access granted.\n");
	else
		log_error("Access denied.\n");
	return (retval);
}

static void pusb_check_usage(const char *name)
{
	fprintf(stderr, "Usage: %s [--help] [--debug] [--config=path] [--service=name] [--dump] [--quiet] [--debug]" \
			" <username>\n", name);
}

int main(int argc, char **argv)
{
	t_pusb_options	opts;
	char			*conf_file = PUSB_CONF_FILE;
	char			*service = "pamusb-check";
	char			*user = NULL;
	int				quiet = 0;
	int				dump = 0;
	int				debug = 0;
	int				opt;
	int				opt_index = 0;
	extern char		*optarg;
	char			*short_options = "hc:s:dqD";
	struct option	long_options[] = {
		{ "help", 0, 0, 0 },
		{ "config", 1, 0, 0 },
		{ "service", 1, 0, 0 },
		{ "dump", 0, 0, 0 },
		{ "quiet", 0, 0, 0 },
		{ "debug", 0, 0, 0 },
		{ 0, 0, 0, 0 }
	};

	while ((opt = getopt_long(argc, argv, short_options, long_options,
					&opt_index)) != EOF)
	{
		if (opt == 'h' || (!opt && !strcmp(long_options[opt_index].name, "help")))
		{
			pusb_check_usage(argv[0]);
			return (1);
		}
		else if (opt == 'c' || (!opt && !strcmp(long_options[opt_index].name, "config")))
			conf_file = optarg;
		else if (opt == 's' || (!opt && !strcmp(long_options[opt_index].name, "service")))
			service = optarg;
		else if (opt == 'd' || (!opt && !strcmp(long_options[opt_index].name, "dump")))
			dump = 1;
		else if (opt == 'q' || (!opt && !strcmp(long_options[opt_index].name, "quiet")))
			quiet = 1;
		else if (opt == 'D' || (!opt && !strcmp(long_options[opt_index].name, "debug")))
			debug = 1;
		else if (opt == '?')
		{
			pusb_check_usage(argv[0]);
			return (1);
		}
	}

	if ((argc - 1) == optind)
		user = argv[optind];
	else
	{
		pusb_check_usage(argv[0]);
		return (1);
	}

	if (quiet && debug)
	{
		fprintf(stderr, "Error: You cannot use --quiet and --debug together.");
		return (1);
	}

	pusb_log_init(&opts);
	if (!pusb_conf_init(&opts))
		return (1);
	if (!pusb_conf_parse(conf_file, &opts, user, service))
		return (1);
	if (quiet)
	{
		opts.quiet = 1;
		opts.debug = 0;
	}
	else if (debug)
	{
		opts.quiet = 0;
		opts.debug = 1;
	}
	if (dump)
	{
		pusb_check_conf_dump(&opts, user, service);
		return (1);
	}
	return (!pusb_check_perform_authentication(&opts, user, service));
}
