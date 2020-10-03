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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#include <time.h>

#include "conf.h"
#include "log.h"
#include "volume.h"
#include "pad.h"

static FILE *pusb_pad_open_device(t_pusb_options *opts,
		const char *mnt_point,
		const char *user,
		const char *mode)
{
	FILE		*f;
	char		path[PATH_MAX];
	struct stat	sb;

	memset(path, 0x00, PATH_MAX);
	snprintf(path, PATH_MAX, "%s/%s", mnt_point, opts->device_pad_directory);
	if (stat(path, &sb) != 0)
	{
		log_debug("Directory %s does not exist, creating one.\n", path);
		if (mkdir(path, S_IRUSR | S_IWUSR | S_IXUSR) != 0)
		{
			log_debug("Unable to create directory %s: %s\n", path,
					strerror(errno));
			return (NULL);
		}
		memset(path, 0x00, PATH_MAX);
	}
	snprintf(path, PATH_MAX, "%s/%s/%s.%s.pad", mnt_point,
			opts->device_pad_directory, user, opts->hostname);
	f = fopen(path, mode);
	if (!f)
	{
		log_debug("Cannot open device file: %s\n", strerror(errno));
		return (NULL);
	}
	return (f);
}

static FILE *pusb_pad_open_system(t_pusb_options *opts,
		const char *user,
		const char *mode)
{
	FILE			*f;
	char			path[PATH_MAX];
	struct passwd	*user_ent = NULL;
	struct stat		sb;
	char   device_name[PATH_MAX];
	char * device_name_ptr = device_name;

	if (!(user_ent = getpwnam(user)) || !(user_ent->pw_dir))
	{
		log_error("Unable to retrieve information for user \"%s\": %s\n",
			  	user,
				strerror(errno));
		return (0);
	}
	memset(path, 0x00, PATH_MAX);
	snprintf(path, PATH_MAX, "%s/%s", user_ent->pw_dir,
			opts->system_pad_directory);
	if (stat(path, &sb) != 0)
	{
		log_debug("Directory %s does not exist, creating one.\n", path);
		if (mkdir(path, S_IRUSR | S_IWUSR | S_IXUSR) != 0)
		{
			log_debug("Unable to create directory %s: %s\n", path,
					strerror(errno));
			return (NULL);
		}
		chown(path, user_ent->pw_uid, user_ent->pw_gid);
		chmod(path, S_IRUSR | S_IWUSR | S_IXUSR);
	}
	/* change slashes in device name to underscores */
	strcpy(device_name, opts->device.name);
	while(*device_name_ptr) {
		if('/' == *device_name_ptr) *device_name_ptr = '_';
		device_name_ptr++;
	}

	memset(path, 0x00, PATH_MAX);
	snprintf(path, PATH_MAX, "%s/%s/%s.pad", user_ent->pw_dir,
			opts->system_pad_directory, device_name);
	f = fopen(path, mode);
	if (!f)
	{
		log_debug("Cannot open system file: %s\n", strerror(errno));
		return (NULL);
	}
	return (f);
}

static int pusb_pad_protect(const char *user, int fd)
{
	struct passwd	*user_ent = NULL;

	log_debug("Protecting pad file...\n");
	if (!(user_ent = getpwnam(user)))
	{
		log_error("Unable to retrieve information for user \"%s\": %s\n",
			  	user,
				strerror(errno));
		return (0);
	}
	if (fchown(fd, user_ent->pw_uid, user_ent->pw_gid) == -1)
	{
		log_debug("Unable to change owner of the pad: %s\n",
				strerror(errno));
		return (0);
	}
	if (fchmod(fd, S_IRUSR | S_IWUSR) == -1)
	{
		log_debug("Unable to change mode of the pad: %s\n",
				strerror(errno));
		return (0);
	}
	return (1);
}

static int pusb_pad_should_update(t_pusb_options *opts, const char *user)
{
	FILE		*f_system = NULL;
	struct stat st;
	time_t		now;
	time_t		delta;

	log_debug("Checking whether pads are expired or not...\n");
	if (!(f_system = pusb_pad_open_system(opts, user, "r")))
	{
		log_debug("Unable to open system pad, pads must be generated.\n");
		return (1);
	}
	if (fstat(fileno(f_system), &st) == -1)
	{
		fclose(f_system);
		return (1);
	}
	fclose(f_system);

	if (time(&now) == ((time_t)-1))
	{
		log_error("Unable to fetch current time.\n");
		return (1);
	}

	delta = now - st.st_mtime;

	if (delta > opts->pad_expiration)
	{
		log_debug("Pads expired %u seconds ago, updating...\n",
				delta - opts->pad_expiration);
		return (1);
	}
	else
	{
		log_debug("Pads were generated %u seconds ago, not updating.\n",
				delta);
		return (0);
	}
	return (1);
}

static void pusb_pad_update(t_pusb_options *opts,
		const char *volume,
		const char *user)
{
	FILE	*f_device = NULL;
	FILE	*f_system = NULL;
	char	magic[1024];
	int		i;
	unsigned int seed;
	int devrandom;

	if (!pusb_pad_should_update(opts, user))
		return ;
	log_info("Regenerating new pads...\n");
	if (!(f_device = pusb_pad_open_device(opts, volume, user, "w+")))
	{
		log_error("Unable to update pads.\n");
		return ;
	}
	pusb_pad_protect(user, fileno(f_device));

	if (!(f_system = pusb_pad_open_system(opts, user, "w+")))
	{
		log_error("Unable to update pads.\n");
		fclose(f_device);
		return ;
	}
	pusb_pad_protect(user, fileno(f_system));

	log_debug("Generating %d bytes unique pad...\n", sizeof(magic));
	devrandom = open("/dev/random", O_RDONLY);
	if (devrandom < 0 || read(devrandom, &seed, sizeof seed) != sizeof seed) {
		log_debug("/dev/random seeding failed...\n");
		seed = getpid() * time(NULL); /* low-entropy fallback */
	}
	if (devrandom > 0)
		close(devrandom);
	srand(seed);
	for (i = 0; i < sizeof(magic); ++i)
		magic[i] = (char)rand();
	log_debug("Writing pad to the device...\n");
	fwrite(magic, sizeof(char), sizeof(magic), f_system);
	log_debug("Writing pad to the system...\n");
	fwrite(magic, sizeof(char), sizeof(magic), f_device);
	log_debug("Synchronizing filesystems...\n");
	fclose(f_system);
	fclose(f_device);
	sync();
	log_debug("One time pads updated.\n");
}

static int pusb_pad_compare(t_pusb_options *opts, const char *volume,
		const char *user)
{
	FILE	*f_device = NULL;
	FILE	*f_system = NULL;
	char	magic_device[1024];
	char	magic_system[1024];
	int		retval;

	if (!(f_system = pusb_pad_open_system(opts, user, "r")))
		return (1);
	if (!(f_device = pusb_pad_open_device(opts, volume, user, "r")))
	{
		fclose(f_system);
		return (0);
	}
	log_debug("Loading device pad...\n");
	fread(magic_device, sizeof(char), sizeof(magic_device), f_device);
	log_debug("Loading system pad...\n");
	fread(magic_system, sizeof(char), sizeof(magic_system), f_system);
	retval = memcmp(magic_system, magic_device, sizeof(magic_system));
	fclose(f_system);
	fclose(f_device);
	if (!retval)
		log_debug("Pad match.\n");
	return (retval == 0);
}

int pusb_pad_check(t_pusb_options *opts,
		UDisksClient *udisks,
		const char *user)
{
	t_pusb_volume	*volume = NULL;
	int		retval = 0;

	volume = pusb_volume_get(opts, udisks);
	if (!volume)
		return (0);

	retval = pusb_pad_compare(opts, volume->mount_point, user);
	if (retval)
		pusb_pad_update(opts, volume->mount_point, user);
	else
		log_error("Pad checking failed!\n");

	pusb_volume_destroy(volume);
	return (retval);
}
