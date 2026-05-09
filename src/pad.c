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
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/random.h>
#include <fcntl.h>
#include <pwd.h>
#include <time.h>

#include "conf.h"
#include "log.h"
#include "volume.h"
#include "pad.h"

#define PUSB_PAD_SIZE 1024

static int pusb_pad_build_device_path(
	t_pusb_options *opts,
	const char *mnt_point,
	const char *user,
	char *path_out,
	size_t path_size
)
{
	char path_devpad[1024*5];
	struct stat sb;

	snprintf(path_devpad, sizeof(path_devpad), "%s/%s", mnt_point, opts->device_pad_directory);
	if (lstat(path_devpad, &sb) != 0)
	{
		log_debug("Directory %s does not exist, creating it.\n", path_devpad);
		if (mkdir(path_devpad, S_IRUSR | S_IWUSR | S_IXUSR) != 0)
		{
			log_debug("Unable to create directory %s: %s\n", path_devpad, strerror(errno));
			return 0;
		}
	}
	else if (S_ISLNK(sb.st_mode))
	{
		log_error("Device pad directory %s is a symlink, refusing to use it.\n", path_devpad);
		return 0;
	}

	snprintf(
		path_out,
		path_size,
		"%s/%s/%s.%s.pad",
		mnt_point,
		opts->device_pad_directory,
		user,
		opts->hostname
	);
	return 1;
}

static int pusb_pad_build_system_path(
	t_pusb_options *opts,
	const char *user,
	char *path_out,
	size_t path_size
)
{
	struct passwd *user_ent = NULL;
	struct stat sb;
	char device_name[128];
	char *device_name_ptr = device_name;
	char dir_path[1024*5];

	if (!(user_ent = getpwnam(user)) || !(user_ent->pw_dir))
	{
		log_error("Unable to retrieve information for user \"%s\": %s\n", user, strerror(errno));
		return 0;
	}

	snprintf(
		dir_path,
		sizeof(dir_path),
		"%s/%s",
		user_ent->pw_dir,
		opts->system_pad_directory
	);
	if (lstat(dir_path, &sb) != 0)
	{
		log_debug("Directory %s does not exist, creating one.\n", dir_path);
		if (mkdir(dir_path, S_IRUSR | S_IWUSR | S_IXUSR) != 0)
		{
			log_debug("Unable to create directory %s: %s\n", dir_path, strerror(errno));
			return 0;
		}

		if (chown(dir_path, user_ent->pw_uid, user_ent->pw_gid) != 0)
		{
			log_error("Unable to chown directory %s: %s\n", dir_path, strerror(errno));
		}

		chmod(dir_path, S_IRUSR | S_IWUSR | S_IXUSR);
	}
	else if (S_ISLNK(sb.st_mode))
	{
		log_error("System pad directory %s is a symlink, refusing to use it.\n", dir_path);
		return 0;
	}
	/* change slashes in device name to underscores */
	snprintf(device_name, sizeof(device_name), "%s", opts->device.name);
	while (*device_name_ptr)
	{
		if ('/' == *device_name_ptr)
		{
			*device_name_ptr = '_';
		}

		device_name_ptr++;
	}

	snprintf(
		path_out,
		path_size,
		"%s/%s/%s.pad",
		user_ent->pw_dir,
		opts->system_pad_directory,
		device_name
	);
	return 1;
}

static FILE *pusb_pad_open_device(
	t_pusb_options *opts,
	const char *mnt_point,
	const char *user,
	const char *mode
)
{
	char path[1024*5];
	int flags = (mode[0] == 'r') ? O_RDONLY : (O_WRONLY | O_CREAT | O_TRUNC);

	if (!pusb_pad_build_device_path(opts, mnt_point, user, path, sizeof(path)))
	{
		return NULL;
	}
	int fd = open(path, flags | O_NOFOLLOW | O_CLOEXEC, 0600);
	if (fd < 0)
	{
		log_debug("Cannot open device file: %s\n", strerror(errno));
		return NULL;
	}
	FILE *f = fdopen(fd, mode);
	if (!f)
	{
		close(fd);
		log_debug("Cannot fdopen device file: %s\n", strerror(errno));
		return NULL;
	}
	return f;
}

static FILE *pusb_pad_open_system(
	t_pusb_options *opts,
	const char *user,
	const char *mode
)
{
	char path[1024*5];
	int flags = (mode[0] == 'r') ? O_RDONLY : (O_WRONLY | O_CREAT | O_TRUNC);

	if (!pusb_pad_build_system_path(opts, user, path, sizeof(path)))
	{
		return NULL;
	}
	int fd = open(path, flags | O_NOFOLLOW | O_CLOEXEC, 0600);
	if (fd < 0)
	{
		log_debug("Cannot open system file: %s\n", strerror(errno));
		return NULL;
	}
	FILE *f = fdopen(fd, mode);
	if (!f)
	{
		close(fd);
		log_debug("Cannot fdopen system file: %s\n", strerror(errno));
		return NULL;
	}
	return f;
}

static int pusb_pad_protect(const char *user, int fd)
{
	struct passwd *user_ent = NULL;

	log_debug("Protecting pad file...\n");
	if (!(user_ent = getpwnam(user)))
	{
		log_error("Unable to retrieve information for user \"%s\": %s\n", user, strerror(errno));
		return 0;
	}
	if (fchown(fd, user_ent->pw_uid, user_ent->pw_gid) == -1)
	{
		log_debug("Unable to change owner of the pad: %s\n", strerror(errno));
		return 0;
	}
	if (fchmod(fd, S_IRUSR | S_IWUSR) == -1)
	{
		log_debug("Unable to change mode of the pad: %s\n", strerror(errno));
		return 0;
	}
	return 1;
}

static int pusb_pad_should_update(t_pusb_options *opts, const char *user)
{
	FILE *f_system = NULL;
	struct stat st;
	time_t now;
	time_t delta;

	log_debug("Checking whether pads are expired or not...\n");
	if (!(f_system = pusb_pad_open_system(opts, user, "r")))
	{
		log_debug("Unable to open system pad, pads must be generated.\n");
		return 1;
	}
	if (fstat(fileno(f_system), &st) == -1)
	{
		fclose(f_system);
		return 1;
	}
	fclose(f_system);

	if (time(&now) == ((time_t)-1))
	{
		log_error("Unable to fetch current time.\n");
		return 1;
	}

	delta = now - st.st_mtime;

	if (delta > opts->pad_expiration)
	{
		log_debug("Pads expired %u seconds ago, updating...\n", delta - opts->pad_expiration);
		return 1;
	}
	else
	{
		log_debug("Pads were generated %u seconds ago, not updating.\n", delta);
		return 0;
	}
	return 1;
}

/*
 * generate_random_bytes — fill buf with len cryptographically strong random
 * bytes via getrandom(2). Retries on EINTR. Returns 0 on success, -1 on error.
 * Since Linux 5.6 the urandom and random pools are identical; getrandom(2)
 * is the correct modern interface (no fd, no path, never blocks after boot).
 * Requests > 256 bytes may return a short count when interrupted, hence the
 * retry loop.
 */
static int generate_random_bytes(uint8_t *buf, size_t len)
{
	size_t offset = 0;

	while (offset < len)
	{
		ssize_t ret = getrandom(buf + offset, len - offset, 0);
		if (ret < 0)
		{
			if (errno == EINTR)
				continue;
			log_error("getrandom() failed: %s\n", strerror(errno));
			return -1;
		}
		offset += (size_t)ret;
	}
	return 0;
}

static int pusb_pad_update(
	t_pusb_options *opts,
	const char *volume,
	const char *user
)
{
	FILE *f_device = NULL;
	FILE *f_system = NULL;
	char path_device[1024*5];
	char path_system[1024*5];
	char path_device_tmp[1024*5 + 8];
	char path_system_tmp[1024*5 + 8];
	uint8_t magic[PUSB_PAD_SIZE];

	if (!pusb_pad_should_update(opts, user))
	{
		return 1;
	}

	log_info("Regenerating new pads...\n");

	if (!pusb_pad_build_device_path(opts, volume, user, path_device, sizeof(path_device)))
	{
		log_error("Unable to determine device pad path.\n");
		return 0;
	}

	if (!pusb_pad_build_system_path(opts, user, path_system, sizeof(path_system)))
	{
		log_error("Unable to determine system pad path.\n");
		return 0;
	}

	snprintf(path_device_tmp, sizeof(path_device_tmp), "%s.tmp", path_device);
	snprintf(path_system_tmp, sizeof(path_system_tmp), "%s.tmp", path_system);

	log_debug("Generating %d bytes unique pad...\n", PUSB_PAD_SIZE);
	if (generate_random_bytes(magic, sizeof(magic)) != 0)
	{
		log_error("Failed to generate random pad data.\n");
		return 0;
	}

	{
		int fd_dev = open(path_device_tmp, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0600);
		if (fd_dev < 0 || !(f_device = fdopen(fd_dev, "w")))
		{
			if (fd_dev >= 0) close(fd_dev);
			log_error("Unable to create temp device pad: %s\n", strerror(errno));
			goto cleanup;
		}
	}
	pusb_pad_protect(user, fileno(f_device));

	{
		int fd_sys = open(path_system_tmp, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0600);
		if (fd_sys < 0 || !(f_system = fdopen(fd_sys, "w")))
		{
			if (fd_sys >= 0) close(fd_sys);
			log_error("Unable to create temp system pad: %s\n", strerror(errno));
			fclose(f_device);
			f_device = NULL;
			unlink(path_device_tmp);
			goto cleanup;
		}
	}
	pusb_pad_protect(user, fileno(f_system));

	log_debug("Writing pad to the system...\n");
	if (fwrite(magic, sizeof(uint8_t), sizeof(magic), f_system) != sizeof(magic))
	{
		log_error("Failed to write system pad: %s\n", strerror(errno));
		fclose(f_system);
		f_system = NULL;
		fclose(f_device);
		f_device = NULL;
		unlink(path_system_tmp);
		unlink(path_device_tmp);
		goto cleanup;
	}

	log_debug("Writing pad to the device...\n");
	if (fwrite(magic, sizeof(uint8_t), sizeof(magic), f_device) != sizeof(magic))
	{
		log_error("Failed to write device pad: %s\n", strerror(errno));
		fclose(f_system);
		f_system = NULL;
		fclose(f_device);
		f_device = NULL;
		unlink(path_system_tmp);
		unlink(path_device_tmp);
		goto cleanup;
	}

	log_debug("Synchronizing filesystems...\n");
	fsync(fileno(f_system));
	fsync(fileno(f_device));
	fclose(f_system);
	f_system = NULL;
	fclose(f_device);
	f_device = NULL;

	explicit_bzero(magic, sizeof(magic));

	if (rename(path_system_tmp, path_system) != 0)
	{
		log_error("Failed to install system pad: %s\n", strerror(errno));
		unlink(path_system_tmp);
		unlink(path_device_tmp);
		return 0;
	}

	if (rename(path_device_tmp, path_device) != 0)
	{
		log_error("Failed to install device pad: %s\n", strerror(errno));
		unlink(path_device_tmp);
		return 0;
	}

	log_debug("One time pads updated.\n");
	return 1;

cleanup:
	explicit_bzero(magic, sizeof(magic));
	if (f_system) fclose(f_system);
	if (f_device) fclose(f_device);
	return 0;
}

static int timingsafe_memcmp(const void *a, const void *b, size_t n)
{
	const uint8_t *pa = (const uint8_t *)a;
	const uint8_t *pb = (const uint8_t *)b;
	uint8_t diff = 0;
	size_t i;

	for (i = 0; i < n; i++)
		diff |= pa[i] ^ pb[i];

	return (int)diff;
}

static int pusb_pad_compare(
	t_pusb_options *opts,
	const char *volume,
	const char *user
)
{
	FILE *f_device = NULL;
	FILE *f_system = NULL;
	uint8_t magic_device[PUSB_PAD_SIZE];
	uint8_t magic_system[PUSB_PAD_SIZE];
	int retval = 0;
	size_t bytes_read;

	if (!(f_system = pusb_pad_open_system(opts, user, "r")))
	{
		log_error("Cannot open system pad, denying auth.\n");
		goto cleanup_nosecret;
	}

	if (!(f_device = pusb_pad_open_device(opts, volume, user, "r")))
	{
		log_error("Cannot open device pad, denying auth.\n");
		goto cleanup_nosecret;
	}

	log_debug("Loading device pad...\n");
	bytes_read = fread(magic_device, sizeof(uint8_t), PUSB_PAD_SIZE, f_device);
	if (bytes_read != PUSB_PAD_SIZE)
	{
		log_error("Device pad is incomplete (%zu/%zu bytes).\n", bytes_read, (size_t)PUSB_PAD_SIZE);
		goto cleanup;
	}

	log_debug("Loading system pad...\n");
	bytes_read = fread(magic_system, sizeof(uint8_t), PUSB_PAD_SIZE, f_system);
	if (bytes_read != PUSB_PAD_SIZE)
	{
		log_error("System pad is incomplete (%zu/%zu bytes).\n", bytes_read, (size_t)PUSB_PAD_SIZE);
		goto cleanup;
	}

	retval = (timingsafe_memcmp(magic_system, magic_device, PUSB_PAD_SIZE) == 0);
	if (retval)
		log_debug("Pad match.\n");

cleanup:
	explicit_bzero(magic_device, sizeof(magic_device));
	explicit_bzero(magic_system, sizeof(magic_system));
cleanup_nosecret:
	if (f_system) fclose(f_system);
	if (f_device) fclose(f_device);
	return retval;
}

int pusb_pad_check(
	t_pusb_options *opts,
	UDisksClient *udisks,
	const char *user
)
{
	t_pusb_volume *volume = NULL;
	int retval = 0;

	volume = pusb_volume_get(opts, udisks);
	if (!volume)
	{
		return 0;
	}

	retval = pusb_pad_compare(opts, volume->mount_point, user);
	if (retval)
	{
		if (pusb_pad_update(opts, volume->mount_point, user) != 1) {
			log_error("Pad check succeeded, but updating failed!\n");
			retval = 0;
		}
	}
	else
	{
		log_error("Pad checking failed!\n");
	}

	pusb_volume_destroy(volume);
	return retval;
}
