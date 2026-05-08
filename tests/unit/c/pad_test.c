/*
 * Unit tests for src/pad.c static functions (included directly).
 * Provides stubs for pusb_volume_get/pusb_volume_destroy so libudisks2
 * does not need to be linked.
 *
 * Security regression coverage:
 *   H-2: pad directory symlink rejected via lstat()+S_ISLNK()
 *   H-4: pad file open rejects symlink (O_NOFOLLOW, errno==ELOOP)
 *   H-3: truncated pad file → pusb_pad_compare() returns 0 (deny)
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <pwd.h>
#include <time.h>
#include <errno.h>
#include <cmocka.h>

/* Include pad.c source directly to access static functions */
#include "../../../src/pad.c"

/* Stubs — pad.c calls these from pusb_pad_check only, which we don't test here */
t_pusb_volume *pusb_volume_get(t_pusb_options *opts, UDisksClient *udisks)
{
	(void)opts; (void)udisks;
	return NULL;
}
void pusb_volume_destroy(t_pusb_volume *volume)
{
	(void)volume;
}

/* ── helpers ── */

static void mkdir_p(const char *path)
{
	mkdir(path, S_IRUSR | S_IWUSR | S_IXUSR);
}

static void write_file(const char *path, const void *data, size_t len)
{
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	assert_true(fd >= 0);
	assert_int_equal((ssize_t)len, write(fd, data, len));
	close(fd);
}

/* ── H-2 regression: symlink in pad-directory position ── */

static void test_device_pad_dir_symlink_rejected(void **state)
{
	(void)state;
	char mnt_dir[] = "/tmp/pamusb_h2_XXXXXX";
	assert_non_null(mkdtemp(mnt_dir));

	/* Place a symlink where the .pamusb dir should be */
	char link_path[512];
	snprintf(link_path, sizeof(link_path), "%s/.pamusb", mnt_dir);
	assert_int_equal(0, symlink("/tmp", link_path));

	t_pusb_options opts = {0};
	pusb_conf_init(&opts);
	snprintf(opts.device.name, sizeof(opts.device.name), "testdev");

	char path_out[1024 * 5];
	int result = pusb_pad_build_device_path(&opts, mnt_dir, "testuser", path_out, sizeof(path_out));
	assert_int_equal(0, result);

	/* cleanup */
	unlink(link_path);
	rmdir(mnt_dir);
}

static void test_system_pad_dir_symlink_rejected(void **state)
{
	(void)state;
	struct passwd *pw = getpwuid(getuid());
	assert_non_null(pw);

	/* Build path for the system pad dir */
	char sys_pad_dir[512];
	snprintf(sys_pad_dir, sizeof(sys_pad_dir), "%s/.pamusb_h2_test_symlink", pw->pw_dir);

	/* Replace dir with a symlink */
	/* Ensure it doesn't exist as a real dir first */
	rmdir(sys_pad_dir);
	unlink(sys_pad_dir);
	assert_int_equal(0, symlink("/tmp", sys_pad_dir));

	t_pusb_options opts = {0};
	pusb_conf_init(&opts);
	snprintf(opts.system_pad_directory, sizeof(opts.system_pad_directory),
	         ".pamusb_h2_test_symlink");
	snprintf(opts.device.name, sizeof(opts.device.name), "testdev");

	char path_out[1024 * 5];
	int result = pusb_pad_build_system_path(&opts, pw->pw_name, path_out, sizeof(path_out));
	assert_int_equal(0, result);

	unlink(sys_pad_dir);
}

/* ── H-4 regression: O_NOFOLLOW rejects symlink at pad file path ── */

static void test_device_pad_file_symlink_rejected(void **state)
{
	(void)state;
	char mnt_dir[] = "/tmp/pamusb_h4_XXXXXX";
	assert_non_null(mkdtemp(mnt_dir));

	char pad_dir[512];
	snprintf(pad_dir, sizeof(pad_dir), "%s/.pamusb", mnt_dir);
	mkdir_p(pad_dir);

	t_pusb_options opts = {0};
	pusb_conf_init(&opts);
	snprintf(opts.device.name, sizeof(opts.device.name), "testdev");

	/* Create symlink at the expected pad file path */
	char symlink_path[512];
	snprintf(symlink_path, sizeof(symlink_path), "%s/testuser.%s.pad",
	         pad_dir, opts.hostname);
	assert_int_equal(0, symlink("/etc/passwd", symlink_path));

	/* Open should fail because O_NOFOLLOW rejects symlinks (errno == ELOOP) */
	FILE *f = pusb_pad_open_device(&opts, mnt_dir, "testuser", "r");
	assert_null(f);

	/* cleanup */
	unlink(symlink_path);
	rmdir(pad_dir);
	rmdir(mnt_dir);
}

/* ── H-3 regression: truncated device pad → deny ── */

static void test_truncated_device_pad_denied(void **state)
{
	(void)state;
	struct passwd *pw = getpwuid(getuid());
	assert_non_null(pw);

	/* Mount-point temp dir */
	char mnt_dir[] = "/tmp/pamusb_h3_XXXXXX";
	assert_non_null(mkdtemp(mnt_dir));

	t_pusb_options opts = {0};
	pusb_conf_init(&opts);
	snprintf(opts.device.name, sizeof(opts.device.name), "pamusb_h3test");
	snprintf(opts.system_pad_directory, sizeof(opts.system_pad_directory),
	         ".pamusb_h3_unit");

	/* Create device pad dir and a 512-byte (truncated) pad file */
	char dev_pad_dir[512];
	snprintf(dev_pad_dir, sizeof(dev_pad_dir), "%s/.pamusb", mnt_dir);
	mkdir_p(dev_pad_dir);

	char dev_pad_path[512];
	snprintf(dev_pad_path, sizeof(dev_pad_path), "%s/%s.%s.pad",
	         dev_pad_dir, pw->pw_name, opts.hostname);

	char buf_512[512];
	memset(buf_512, 0xAB, sizeof(buf_512));
	write_file(dev_pad_path, buf_512, sizeof(buf_512));

	/* Create system pad dir and a valid 1024-byte pad file */
	char sys_pad_dir[512];
	snprintf(sys_pad_dir, sizeof(sys_pad_dir), "%s/.pamusb_h3_unit", pw->pw_dir);
	mkdir_p(sys_pad_dir);

	char sys_pad_path[512];
	snprintf(sys_pad_path, sizeof(sys_pad_path), "%s/pamusb_h3test.pad", sys_pad_dir);
	char buf_1024[1024];
	memset(buf_1024, 0xAB, sizeof(buf_1024));
	write_file(sys_pad_path, buf_1024, sizeof(buf_1024));

	/* compare must return 0 (deny) because device pad is only 512 bytes */
	int result = pusb_pad_compare(&opts, mnt_dir, pw->pw_name);
	assert_int_equal(0, result);

	/* cleanup */
	unlink(dev_pad_path);
	rmdir(dev_pad_dir);
	rmdir(mnt_dir);
	unlink(sys_pad_path);
	rmdir(sys_pad_dir);
}

/* ── Pad expiration: pusb_pad_should_update ── */

static void test_pad_expired(void **state)
{
	(void)state;
	struct passwd *pw = getpwuid(getuid());
	assert_non_null(pw);

	t_pusb_options opts = {0};
	pusb_conf_init(&opts);
	snprintf(opts.device.name, sizeof(opts.device.name), "pamusb_exptest");
	snprintf(opts.system_pad_directory, sizeof(opts.system_pad_directory),
	         ".pamusb_exp_unit");
	opts.pad_expiration = 3600;

	/* Create system pad dir and file */
	char sys_pad_dir[512];
	snprintf(sys_pad_dir, sizeof(sys_pad_dir), "%s/.pamusb_exp_unit", pw->pw_dir);
	mkdir_p(sys_pad_dir);

	char sys_pad_path[512];
	snprintf(sys_pad_path, sizeof(sys_pad_path), "%s/pamusb_exptest.pad", sys_pad_dir);
	char dummy[8] = {0};
	write_file(sys_pad_path, dummy, sizeof(dummy));

	/* Set mtime to now - pad_expiration - 1 (expired) */
	struct timeval tv[2];
	time_t expired_time = time(NULL) - opts.pad_expiration - 1;
	tv[0].tv_sec = expired_time; tv[0].tv_usec = 0;
	tv[1].tv_sec = expired_time; tv[1].tv_usec = 0;
	utimes(sys_pad_path, tv);

	assert_int_equal(1, pusb_pad_should_update(&opts, pw->pw_name));

	unlink(sys_pad_path);
	rmdir(sys_pad_dir);
}

static void test_pad_fresh(void **state)
{
	(void)state;
	struct passwd *pw = getpwuid(getuid());
	assert_non_null(pw);

	t_pusb_options opts = {0};
	pusb_conf_init(&opts);
	snprintf(opts.device.name, sizeof(opts.device.name), "pamusb_freshtest");
	snprintf(opts.system_pad_directory, sizeof(opts.system_pad_directory),
	         ".pamusb_fresh_unit");
	opts.pad_expiration = 3600;

	char sys_pad_dir[512];
	snprintf(sys_pad_dir, sizeof(sys_pad_dir), "%s/.pamusb_fresh_unit", pw->pw_dir);
	mkdir_p(sys_pad_dir);

	char sys_pad_path[512];
	snprintf(sys_pad_path, sizeof(sys_pad_path), "%s/pamusb_freshtest.pad", sys_pad_dir);
	char dummy[8] = {0};
	write_file(sys_pad_path, dummy, sizeof(dummy));

	/* Set mtime to now - 1 (fresh — well within expiration) */
	struct timeval tv[2];
	time_t fresh_time = time(NULL) - 1;
	tv[0].tv_sec = fresh_time; tv[0].tv_usec = 0;
	tv[1].tv_sec = fresh_time; tv[1].tv_usec = 0;
	utimes(sys_pad_path, tv);

	assert_int_equal(0, pusb_pad_should_update(&opts, pw->pw_name));

	unlink(sys_pad_path);
	rmdir(sys_pad_dir);
}

static void test_pad_missing(void **state)
{
	(void)state;
	struct passwd *pw = getpwuid(getuid());
	assert_non_null(pw);

	t_pusb_options opts = {0};
	pusb_conf_init(&opts);
	snprintf(opts.device.name, sizeof(opts.device.name), "pamusb_missing_pad");
	snprintf(opts.system_pad_directory, sizeof(opts.system_pad_directory),
	         ".pamusb_miss_unit");

	/* Ensure the pad dir exists but the pad file does not */
	char sys_pad_dir[512];
	snprintf(sys_pad_dir, sizeof(sys_pad_dir), "%s/.pamusb_miss_unit", pw->pw_dir);
	mkdir_p(sys_pad_dir);

	char sys_pad_path[512];
	snprintf(sys_pad_path, sizeof(sys_pad_path), "%s/pamusb_missing_pad.pad", sys_pad_dir);
	unlink(sys_pad_path); /* make sure it doesn't exist */

	/* No pad file → must update */
	assert_int_equal(1, pusb_pad_should_update(&opts, pw->pw_name));

	rmdir(sys_pad_dir);
}

/* ── main ── */

int main(void)
{
	t_pusb_options init_opts = {0};
	init_opts.debug = 0;
	init_opts.quiet = 1;
	pusb_log_init(&init_opts);

	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_device_pad_dir_symlink_rejected),
		cmocka_unit_test(test_system_pad_dir_symlink_rejected),
		cmocka_unit_test(test_device_pad_file_symlink_rejected),
		cmocka_unit_test(test_truncated_device_pad_denied),
		cmocka_unit_test(test_pad_expired),
		cmocka_unit_test(test_pad_fresh),
		cmocka_unit_test(test_pad_missing),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
