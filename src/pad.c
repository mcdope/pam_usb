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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/random.h>
#include <sys/file.h>
#include <fcntl.h>
#include <pwd.h>
#include <time.h>

#include "conf.h"
#include "log.h"
#include "volume.h"
#include "pad.h"
#include "pusb_testing.h"


/* mkdir(path, 0700) atomically; on EEXIST verify path is a real directory (not a symlink).
 * Returns 1 if newly created, 0 if pre-existed and is a real directory, -1 on error. */
static int pusb_mkdir_safe(const char *path, const char *context)
{
	if (mkdir(path, S_IRUSR | S_IWUSR | S_IXUSR) == 0)
		return 1;
	if (errno != EEXIST)
	{
		int err = errno;
		if (err == EACCES || err == EPERM)
			log_debug("Unable to create directory %s: %s (check AppArmor/SELinux policy)\n", path, strerror(err)); /* DevSkim: ignore DS154189 */
		else
			log_debug("Unable to create directory %s: %s\n", path, strerror(err)); /* DevSkim: ignore DS154189 */
		return -1;
	}
	/* O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC: rejects symlinks (ELOOP), non-directories (ENOTDIR), prevents fd leak */
	int fd = open(path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
	if (fd < 0)
	{
		int err = errno;
		if (err == EACCES || err == EPERM)
			log_error("%s directory %s could not be opened safely: %s (check AppArmor/SELinux policy)\n", context, path, strerror(err)); /* DevSkim: ignore DS154189 */
		else
			log_error("%s directory %s could not be opened safely: %s\n", context, path, strerror(err)); /* DevSkim: ignore DS154189 */
		return -1;
	}
	close(fd);
	return 0;
}

PUSB_STATIC int pusb_pad_build_device_path(
	t_pusb_options *opts,
	const char *mnt_point,
	const char *user,
	char *path_out,
	size_t path_size
)
{
	char path_devpad[PUSB_PAD_PATH_MAX];

	int pn1 = snprintf(path_devpad, sizeof(path_devpad), "%s/%s", mnt_point, opts->device_pad_directory);
	if (pn1 < 0 || (size_t)pn1 >= sizeof(path_devpad))
	{
		log_error("Device pad directory path too long.\n");
		return 0;
	}
	if (pusb_mkdir_safe(path_devpad, "Device pad") < 0)
		return 0;

	int pn2 = snprintf(
		path_out,
		path_size,
		"%s/%s/%s.%s.pad",
		mnt_point,
		opts->device_pad_directory,
		user,
		opts->hostname
	);
	if (pn2 < 0 || (size_t)pn2 >= path_size)
	{
		log_error("Device pad file path too long.\n");
		return 0;
	}
	return 1;
}

PUSB_STATIC int pusb_pad_build_system_path(
	t_pusb_options *opts,
	const char *user,
	char *path_out,
	size_t path_size
)
{
	struct passwd *user_ent = NULL;
	char device_name[128];
	char *device_name_ptr = device_name;
	char dir_path[PUSB_PAD_PATH_MAX];

	if (!(user_ent = getpwnam(user)) || !(user_ent->pw_dir))
	{
		log_error("Unable to retrieve information for user \"%s\": %s\n", user, strerror(errno));
		return 0;
	}

	int sn1 = snprintf(
		dir_path,
		sizeof(dir_path),
		"%s/%s",
		user_ent->pw_dir,
		opts->system_pad_directory
	);
	if (sn1 < 0 || (size_t)sn1 >= sizeof(dir_path))
	{
		log_error("System pad directory path too long.\n");
		return 0;
	}
	int rc = pusb_mkdir_safe(dir_path, "System pad");
	if (rc < 0)
		return 0;
	if (rc > 0)
	{
		/* Open the directory we just created via fd to avoid path-based TOCTOU on chown/chmod */
		int dfd = open(dir_path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
		if (dfd < 0)
		{
			int err = errno;
			log_error("Unable to open newly created directory %s: %s\n", dir_path, strerror(err)); /* DevSkim: ignore DS154189 */
			return 0;
		}
		if (fchown(dfd, user_ent->pw_uid, user_ent->pw_gid) != 0)
		{
			int err = errno;
			if (err == EACCES || err == EPERM)
				log_error("Unable to chown directory %s: %s (check AppArmor/SELinux policy)\n", dir_path, strerror(err)); /* DevSkim: ignore DS154189 */
			else
				log_error("Unable to chown directory %s: %s\n", dir_path, strerror(err)); /* DevSkim: ignore DS154189 */
			if (err != EPERM && err != EROFS && err != ENOTSUP)
			{
				close(dfd);
				return 0;
			}
		}
		if (fchmod(dfd, S_IRUSR | S_IWUSR | S_IXUSR) != 0)
		{
			int err = errno;
			if (err == EACCES || err == EPERM)
				log_error("Unable to chmod directory %s: %s (check AppArmor/SELinux policy)\n", dir_path, strerror(err)); /* DevSkim: ignore DS154189 */
			else
				log_error("Unable to chmod directory %s: %s\n", dir_path, strerror(err)); /* DevSkim: ignore DS154189 */
			if (err != EPERM && err != EROFS && err != ENOTSUP)
			{
				close(dfd);
				return 0;
			}
		}
		close(dfd);
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

	int sn2 = snprintf(
		path_out,
		path_size,
		"%s/%s/%s.pad",
		user_ent->pw_dir,
		opts->system_pad_directory,
		device_name
	);
	if (sn2 < 0 || (size_t)sn2 >= path_size)
	{
		log_error("System pad file path too long.\n");
		return 0;
	}
	return 1;
}

PUSB_STATIC int open_pad_file_in_dir(const char *fullpath, int flags)
{
	char dirbuf[PUSB_PAD_PATH_MAX + 8];
	int n = snprintf(dirbuf, sizeof(dirbuf), "%s", fullpath);
	if (n < 0 || (size_t)n >= sizeof(dirbuf))
		return -1;

	char *sep = strrchr(dirbuf, '/');
	if (sep == NULL || sep == dirbuf)
		return -1;
	*sep = '\0';
	const char *filename = sep + 1;

	int dirfd = open(dirbuf, O_DIRECTORY | O_NOFOLLOW | O_RDONLY | O_CLOEXEC);
	if (dirfd == -1)
		return -1;

	int fd = openat(dirfd, filename, flags | O_NOFOLLOW | O_CLOEXEC, 0600);
	int saved_errno = errno;
	close(dirfd);
	errno = saved_errno;
	return fd;
}

PUSB_STATIC FILE *pusb_pad_open_device(
	t_pusb_options *opts,
	const char *mnt_point,
	const char *user,
	const char *mode
)
{
	char path[PUSB_PAD_PATH_MAX];
	int flags = (mode[0] == 'r') ? O_RDONLY : (O_WRONLY | O_CREAT | O_TRUNC);

	if (!pusb_pad_build_device_path(opts, mnt_point, user, path, sizeof(path)))
	{
		return NULL;
	}
	int fd = open_pad_file_in_dir(path, flags);
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
	char path[PUSB_PAD_PATH_MAX];
	int flags = (mode[0] == 'r') ? O_RDONLY : (O_WRONLY | O_CREAT | O_TRUNC);

	if (!pusb_pad_build_system_path(opts, user, path, sizeof(path)))
	{
		return NULL;
	}
	int fd = open_pad_file_in_dir(path, flags);
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
	int saved_errno;
	struct passwd *user_ent = NULL;

	log_debug("Protecting pad file...\n");
	if (!(user_ent = getpwnam(user)))
	{
		saved_errno = errno;
		log_error("Unable to retrieve information for user \"%s\": %s\n", user, strerror(saved_errno));
		errno = saved_errno;
		return 0;
	}
	if (fchown(fd, user_ent->pw_uid, user_ent->pw_gid) == -1)
	{
		saved_errno = errno;
		log_debug("Unable to change owner of the pad: %s (expected on filesystems not supporting this, like FAT)\n", strerror(saved_errno));
		errno = saved_errno;
		return 0;
	}
	if (fchmod(fd, S_IRUSR | S_IWUSR) == -1)
	{
		saved_errno = errno;
		log_debug("Unable to change mode of the pad: %s\n", strerror(saved_errno));
		errno = saved_errno;
		return 0;
	}
	return 1;
}

PUSB_STATIC int pusb_pad_should_update(t_pusb_options *opts, const char *user)
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
		log_debug("Pads expired %ld seconds ago, updating...\n", (long)(delta - opts->pad_expiration));
		return 1;
	}
	else
	{
		log_debug("Pads were generated %ld seconds ago, not updating.\n", (long)delta);
		return 0;
	}
}

/*
 * generate_random_bytes — fill buf with len cryptographically strong random
 * bytes via getrandom(2). Retries on EINTR. Returns 0 on success, -1 on error.
 * Since Linux 5.6 the urandom and random pools are identical; getrandom(2)
 * is the correct modern interface (no fd, no path, never blocks after boot).
 * Requests > 256 bytes may return a short count when interrupted, hence the
 * retry loop.
 */
PUSB_STATIC int generate_random_bytes(uint8_t *buf, size_t len)
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

/* Open the directory containing `fullpath` and take an exclusive advisory lock on it.
 * Serializes pad updates so a leftover .tmp file can be told apart from a live updater.
 * Returns the locked fd (close it to release the lock) or -1 on error. */
static int pusb_pad_lock_dir(const char *fullpath)
{
	char dirbuf[PUSB_PAD_PATH_MAX + 8];
	int n = snprintf(dirbuf, sizeof(dirbuf), "%s", fullpath);
	if (n < 0 || (size_t)n >= sizeof(dirbuf))
		return -1;

	char *sep = strrchr(dirbuf, '/');
	if (sep == NULL || sep == dirbuf)
		return -1;
	*sep = '\0';

	int dirfd = open(dirbuf, O_DIRECTORY | O_NOFOLLOW | O_RDONLY | O_CLOEXEC);
	if (dirfd == -1)
		return -1;

	if (flock(dirfd, LOCK_EX) == -1)
	{
		int saved_errno = errno;
		close(dirfd);
		errno = saved_errno;
		return -1;
	}
	return dirfd;
}

/* Create both temp pad files (O_EXCL), write `magic` to them, fsync and atomically rename
 * them into place. Cleans up the temp files on every error path. The caller owns `magic`
 * (and zeroes it afterwards) and must hold the update lock for the duration of this call. */
static int pusb_pad_write_and_install(
	const char *user,
	const char *path_device,
	const char *path_system,
	const char *path_device_tmp,
	const char *path_system_tmp,
	const uint8_t *magic,
	size_t magic_size
)
{
	FILE *f_device = NULL;
	FILE *f_system = NULL;

	{
		int fd_dev = open_pad_file_in_dir(path_device_tmp, O_WRONLY | O_CREAT | O_EXCL);
		if (fd_dev < 0 || !(f_device = fdopen(fd_dev, "w")))
		{
			int saved_errno = errno;
			/* O_EXCL already created the file on disk; if only fdopen failed we must
			 * unlink it so we don't leave a fresh orphan behind. */
			if (fd_dev >= 0)
			{
				close(fd_dev);
				unlink(path_device_tmp);
			}
			log_error("Unable to create temp device pad: %s\n", strerror(saved_errno)); /* DevSkim: ignore DS154189 */
			return 0;
		}
	}
	if (!pusb_pad_protect(user, fileno(f_device))) {
		if (errno != EPERM && errno != EROFS && errno != ENOTSUP) {
			log_error("Failed to protect device pad (unexpected error).\n");
			fclose(f_device);
			unlink(path_device_tmp);
			return 0;
		}
	}

	{
		int fd_sys = open_pad_file_in_dir(path_system_tmp, O_WRONLY | O_CREAT | O_EXCL);
		if (fd_sys < 0 || !(f_system = fdopen(fd_sys, "w")))
		{
			int saved_errno = errno;
			if (fd_sys >= 0)
			{
				close(fd_sys);
				unlink(path_system_tmp);
			}
			fclose(f_device);
			unlink(path_device_tmp);
			log_error("Unable to create temp system pad: %s\n", strerror(saved_errno)); /* DevSkim: ignore DS154189 */
			return 0;
		}
	}
	if (!pusb_pad_protect(user, fileno(f_system))) {
		if (errno != EPERM && errno != EROFS && errno != ENOTSUP) {
			log_error("Failed to protect system pad (unexpected error).\n");
			fclose(f_system);
			fclose(f_device);
			unlink(path_system_tmp);
			unlink(path_device_tmp);
			return 0;
		}
	}

	log_debug("Writing pad to the system...\n");
	if (fwrite(magic, sizeof(uint8_t), magic_size, f_system) != magic_size)
	{
		log_error("Failed to write system pad: %s\n", strerror(errno));
		fclose(f_system);
		fclose(f_device);
		unlink(path_system_tmp);
		unlink(path_device_tmp);
		return 0;
	}

	log_debug("Writing pad to the device...\n");
	if (fwrite(magic, sizeof(uint8_t), magic_size, f_device) != magic_size)
	{
		log_error("Failed to write device pad: %s\n", strerror(errno));
		fclose(f_system);
		fclose(f_device);
		unlink(path_system_tmp);
		unlink(path_device_tmp);
		return 0;
	}

	log_debug("Synchronizing filesystems...\n");
	if (fflush(f_system) != 0 || fflush(f_device) != 0)
	{
		log_error("Failed to flush pad data: %s\n", strerror(errno));
		fclose(f_system);
		fclose(f_device);
		unlink(path_system_tmp);
		unlink(path_device_tmp);
		return 0;
	}
	if (fsync(fileno(f_system)) != 0 || fsync(fileno(f_device)) != 0)
	{
		log_error("Failed to sync pad data to disk: %s\n", strerror(errno));
		fclose(f_system);
		fclose(f_device);
		unlink(path_system_tmp);
		unlink(path_device_tmp);
		return 0;
	}
	fclose(f_system);
	fclose(f_device);

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
}

PUSB_STATIC int pusb_pad_update(
	t_pusb_options *opts,
	const char *volume,
	const char *user
)
{
	char path_device[PUSB_PAD_PATH_MAX];
	char path_system[PUSB_PAD_PATH_MAX];
	char path_device_tmp[PUSB_PAD_PATH_MAX + 8];
	char path_system_tmp[PUSB_PAD_PATH_MAX + 8];
	uint8_t magic[PUSB_PAD_SIZE];
	int lock_fd;
	int retval;
	int saved_errno;

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
		explicit_bzero(magic, sizeof(magic));
		return 0;
	}

	/* Serialize the update with an exclusive lock on the system pad directory. This keeps
	 * the CWE-362 guarantee that two concurrent updates can't clobber each other, while
	 * letting us safely clean up .tmp files orphaned by a process killed mid-update: the
	 * kernel releases that process's lock on death, so a survivor can recover. */
	lock_fd = pusb_pad_lock_dir(path_system);
	if (lock_fd < 0)
	{
		log_error("Unable to lock pad directory for update: %s\n", strerror(errno)); /* DevSkim: ignore DS154189 */
		explicit_bzero(magic, sizeof(magic));
		return 0;
	}

	/* Holding the lock, any pre-existing .tmp must be a leftover from an interrupted update
	 * (a live updater would hold this lock), so remove it to let the O_EXCL create succeed.
	 * ENOENT is the normal, no-leftover case. */
	if (unlink(path_device_tmp) == 0)
		log_debug("Removed orphaned device pad temp file.\n");
	if (unlink(path_system_tmp) == 0)
		log_debug("Removed orphaned system pad temp file.\n");

	retval = pusb_pad_write_and_install(
		user, path_device, path_system,
		path_device_tmp, path_system_tmp,
		magic, sizeof(magic)
	);

	saved_errno = errno;
	close(lock_fd);
	errno = saved_errno;

	explicit_bzero(magic, sizeof(magic));
	return retval;
}

PUSB_STATIC int timingsafe_memcmp(const void *a, const void *b, size_t n)
{
	const uint8_t *pa = (const uint8_t *)a;
	const uint8_t *pb = (const uint8_t *)b;
	volatile uint8_t diff = 0;
	size_t i;

	for (i = 0; i < n; i++)
		diff |= pa[i] ^ pb[i];

	return (int)diff;
}

PUSB_STATIC int pusb_pad_compare(
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
		/* System pad absent. Peek at the device pad:
		 * both absent → first run, allow so update can generate both;
		 * device present but system gone → deleted by attacker, deny (F3). */
		FILE *f_dev_check = pusb_pad_open_device(opts, volume, user, "r");
		if (f_dev_check != NULL)
		{
			fclose(f_dev_check);
			log_error("System pad missing but device pad exists, denying auth.\n");
			return 0;
		}
		log_debug("No pads found, allowing first-time generation.\n");
		return 1;
	}

	if (!(f_device = pusb_pad_open_device(opts, volume, user, "r")))
	{
		log_error("Cannot open device pad, denying auth.\n");
		fclose(f_system);
		return 0;
	}

	log_debug("Loading device pad...\n");
	bytes_read = fread(magic_device, sizeof(uint8_t), PUSB_PAD_SIZE, f_device);
	if (bytes_read != PUSB_PAD_SIZE)
	{
		log_error("Device pad is incomplete (%zu/%zu bytes).\n", bytes_read, (size_t)PUSB_PAD_SIZE);
		explicit_bzero(magic_device, sizeof(magic_device));
		fclose(f_device);
		fclose(f_system);
		return 0;
	}

	log_debug("Loading system pad...\n");
	bytes_read = fread(magic_system, sizeof(uint8_t), PUSB_PAD_SIZE, f_system);
	if (bytes_read != PUSB_PAD_SIZE)
	{
		log_error("System pad is incomplete (%zu/%zu bytes).\n", bytes_read, (size_t)PUSB_PAD_SIZE);
		explicit_bzero(magic_device, sizeof(magic_device));
		explicit_bzero(magic_system, sizeof(magic_system));
		fclose(f_device);
		fclose(f_system);
		return 0;
	}

	retval = (timingsafe_memcmp(magic_system, magic_device, PUSB_PAD_SIZE) == 0);
	if (retval)
		log_debug("Pad match.\n");

	explicit_bzero(magic_device, sizeof(magic_device));
	explicit_bzero(magic_system, sizeof(magic_system));
	fclose(f_system);
	fclose(f_device);
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
