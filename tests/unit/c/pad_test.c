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

#define _GNU_SOURCE
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <stdint.h>
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
	char link_path[PUSB_PAD_PATH_MAX];
	snprintf(link_path, sizeof(link_path), "%s/.pamusb", mnt_dir);
	assert_int_equal(0, symlink("/tmp", link_path));

	t_pusb_options opts = {0};
	pusb_conf_init(&opts);
	snprintf(opts.device.name, sizeof(opts.device.name), "testdev");

	char path_out[PUSB_PAD_PATH_MAX];
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
	char sys_pad_dir[PUSB_PAD_PATH_MAX];
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

	char path_out[PUSB_PAD_PATH_MAX];
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

	char pad_dir[PUSB_PAD_PATH_MAX];
	snprintf(pad_dir, sizeof(pad_dir), "%s/.pamusb", mnt_dir);
	mkdir_p(pad_dir);

	t_pusb_options opts = {0};
	pusb_conf_init(&opts);
	snprintf(opts.device.name, sizeof(opts.device.name), "testdev");

	/* Create symlink at the expected pad file path */
	char symlink_path[PUSB_PAD_PATH_MAX];
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
	char dev_pad_dir[PUSB_PAD_PATH_MAX];
	snprintf(dev_pad_dir, sizeof(dev_pad_dir), "%s/.pamusb", mnt_dir);
	mkdir_p(dev_pad_dir);

	char dev_pad_path[PUSB_PAD_PATH_MAX];
	snprintf(dev_pad_path, sizeof(dev_pad_path), "%s/%s.%s.pad",
	         dev_pad_dir, pw->pw_name, opts.hostname);

	char buf_512[512];
	memset(buf_512, 0xAB, sizeof(buf_512));
	write_file(dev_pad_path, buf_512, sizeof(buf_512));

	/* Create system pad dir and a valid 1024-byte pad file */
	char sys_pad_dir[PUSB_PAD_PATH_MAX];
	snprintf(sys_pad_dir, sizeof(sys_pad_dir), "%s/.pamusb_h3_unit", pw->pw_dir);
	mkdir_p(sys_pad_dir);

	char sys_pad_path[PUSB_PAD_PATH_MAX];
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
	char sys_pad_dir[PUSB_PAD_PATH_MAX];
	snprintf(sys_pad_dir, sizeof(sys_pad_dir), "%s/.pamusb_exp_unit", pw->pw_dir);
	mkdir_p(sys_pad_dir);

	char sys_pad_path[PUSB_PAD_PATH_MAX];
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

	char sys_pad_dir[PUSB_PAD_PATH_MAX];
	snprintf(sys_pad_dir, sizeof(sys_pad_dir), "%s/.pamusb_fresh_unit", pw->pw_dir);
	mkdir_p(sys_pad_dir);

	char sys_pad_path[PUSB_PAD_PATH_MAX];
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
	char sys_pad_dir[PUSB_PAD_PATH_MAX];
	snprintf(sys_pad_dir, sizeof(sys_pad_dir), "%s/.pamusb_miss_unit", pw->pw_dir);
	mkdir_p(sys_pad_dir);

	char sys_pad_path[PUSB_PAD_PATH_MAX];
	snprintf(sys_pad_path, sizeof(sys_pad_path), "%s/pamusb_missing_pad.pad", sys_pad_dir);
	unlink(sys_pad_path); /* make sure it doesn't exist */

	/* No pad file → must update */
	assert_int_equal(1, pusb_pad_should_update(&opts, pw->pw_name));

	rmdir(sys_pad_dir);
}

/* ── F3 regression: missing system pad must deny (not allow) ── */

static void test_missing_system_pad_denied(void **state)
{
	(void)state;
	struct passwd *pw = getpwuid(getuid());
	assert_non_null(pw);

	char mnt_dir[] = "/tmp/pamusb_f3sys_XXXXXX";
	assert_non_null(mkdtemp(mnt_dir));

	t_pusb_options opts = {0};
	pusb_conf_init(&opts);
	snprintf(opts.device.name, sizeof(opts.device.name), "pamusb_f3test");
	/* Use a nonexistent system pad directory so system pad open fails */
	snprintf(opts.system_pad_directory, sizeof(opts.system_pad_directory),
	         ".pamusb_f3_unit_NONEXISTENT");

	char sys_pad_dir[PUSB_PAD_PATH_MAX];
	snprintf(sys_pad_dir, sizeof(sys_pad_dir), "%s/%s", pw->pw_dir,
	         opts.system_pad_directory);
	rmdir(sys_pad_dir);

	/* Create a valid device pad so the test isolates the missing-system-pad branch */
	char dev_pad_dir[PUSB_PAD_PATH_MAX];
	snprintf(dev_pad_dir, sizeof(dev_pad_dir), "%s/.pamusb", mnt_dir);
	mkdir_p(dev_pad_dir);

	char dev_pad_path[PUSB_PAD_PATH_MAX];
	snprintf(dev_pad_path, sizeof(dev_pad_path), "%s/%s.%s.pad",
	         dev_pad_dir, pw->pw_name, opts.hostname);
	uint8_t buf[PUSB_PAD_SIZE];
	memset(buf, 0xBB, sizeof(buf));
	write_file(dev_pad_path, buf, sizeof(buf));

	/* missing system pad must deny (return 0) — pre-fix returned 1 */
	int result = pusb_pad_compare(&opts, mnt_dir, pw->pw_name);
	assert_int_equal(0, result);

	unlink(dev_pad_path);
	rmdir(dev_pad_dir);
	rmdir(mnt_dir);
	rmdir(sys_pad_dir);
}

/* ── First-run: both pads absent → allow (so update can generate them) ── */

static void test_first_run_no_pads_allowed(void **state)
{
	(void)state;
	struct passwd *pw = getpwuid(getuid());
	assert_non_null(pw);

	char mnt_dir[] = "/tmp/pamusb_firstrun_XXXXXX";
	assert_non_null(mkdtemp(mnt_dir));

	t_pusb_options opts = {0};
	pusb_conf_init(&opts);
	snprintf(opts.device.name, sizeof(opts.device.name), "pamusb_firstrun");
	snprintf(opts.system_pad_directory, sizeof(opts.system_pad_directory),
	         ".pamusb_firstrun_unit");

	/* Ensure system pad directory does not exist */
	char sys_pad_dir[PUSB_PAD_PATH_MAX];
	snprintf(sys_pad_dir, sizeof(sys_pad_dir), "%s/%s", pw->pw_dir,
	         opts.system_pad_directory);
	rmdir(sys_pad_dir);

	/* Device pad directory also absent — no pads anywhere (first run) */

	/* Must return 1 (allow) so pusb_pad_update can generate initial pads */
	int result = pusb_pad_compare(&opts, mnt_dir, pw->pw_name);
	assert_int_equal(1, result);

	char mnt_pamusb[PUSB_PAD_PATH_MAX];
	snprintf(mnt_pamusb, sizeof(mnt_pamusb), "%s/.pamusb", mnt_dir);
	rmdir(mnt_pamusb);
	rmdir(mnt_dir);
	rmdir(sys_pad_dir);
}

/* ── F2 regression: generate_random_bytes fills buffer and returns 0 ── */

static void test_generate_random_bytes_fills_buffer(void **state)
{
	(void)state;
	uint8_t buf[PUSB_PAD_SIZE];
	memset(buf, 0, sizeof(buf));

	int ret = generate_random_bytes(buf, sizeof(buf));
	assert_int_equal(0, ret);

	/* Buffer must not be all-zero (CSPRNG output with overwhelming probability) */
	uint8_t acc = 0;
	for (size_t i = 0; i < sizeof(buf); i++)
		acc |= buf[i];
	assert_int_not_equal(0, (int)acc);
}

/* ── F6 regression: open_pad_file_in_dir rejects symlink at parent directory ── */

static void test_open_pad_file_in_dir_rejects_dir_symlink(void **state)
{
	(void)state;

	/* A real directory containing a real file */
	char real_dir[] = "/tmp/pamusb_f6real_XXXXXX";
	assert_non_null(mkdtemp(real_dir));
	char real_file[PUSB_PAD_PATH_MAX];
	snprintf(real_file, sizeof(real_file), "%s/test.pad", real_dir);
	write_file(real_file, "x", 1);

	/* A temp dir containing a symlink that points at real_dir */
	char base_dir[] = "/tmp/pamusb_f6base_XXXXXX";
	assert_non_null(mkdtemp(base_dir));
	char sym_dir[PUSB_PAD_PATH_MAX];
	snprintf(sym_dir, sizeof(sym_dir), "%s/pads", base_dir);
	assert_int_equal(0, symlink(real_dir, sym_dir));

	/* Path whose parent component is a symlink */
	char path[PUSB_PAD_PATH_MAX];
	snprintf(path, sizeof(path), "%s/test.pad", sym_dir);

	/* open_pad_file_in_dir must fail: O_NOFOLLOW rejects symlink as directory */
	int fd = open_pad_file_in_dir(path, O_RDONLY);
	assert_true(fd < 0);
	if (fd >= 0) close(fd);

	unlink(sym_dir);
	unlink(real_file);
	rmdir(real_dir);
	rmdir(base_dir);
}

/* ── F10 regression: device pad path truncation denied ── */

static void test_device_pad_path_too_long_denied(void **state)
{
	(void)state;

	t_pusb_options opts = {0};
	pusb_conf_init(&opts);
	/* Fill device_pad_directory to near PATH_MAX (4095 'a' chars) */
	memset(opts.device_pad_directory, 'a', sizeof(opts.device_pad_directory) - 1);
	opts.device_pad_directory[sizeof(opts.device_pad_directory) - 1] = '\0';

	/* mnt_point of 1100 chars: 1100 + 1 + 4095 = 5196 > 5120 -- triggers truncation check */
	char long_mnt[1101];
	memset(long_mnt, 'b', sizeof(long_mnt) - 1);
	long_mnt[sizeof(long_mnt) - 1] = '\0';

	char path_out[PUSB_PAD_PATH_MAX];
	int result = pusb_pad_build_device_path(&opts, long_mnt, "user", path_out, sizeof(path_out));
	assert_int_equal(0, result);
}

/* ── F4 regression: timingsafe_memcmp correctness ── */

static void test_timingsafe_memcmp_equal(void **state)
{
	(void)state;
	uint8_t a[64], b[64];
	memset(a, 0x5A, sizeof(a));
	memset(b, 0x5A, sizeof(b));
	assert_int_equal(0, timingsafe_memcmp(a, b, sizeof(a)));
}

static void test_timingsafe_memcmp_differ(void **state)
{
	(void)state;
	uint8_t a[64], b[64];
	memset(a, 0x5A, sizeof(a));
	memset(b, 0x5A, sizeof(b));
	b[63] = 0xA5;
	assert_int_not_equal(0, timingsafe_memcmp(a, b, sizeof(a)));
}

/* ── CWE-362 regression: O_EXCL on temp file prevents concurrent update race ── */

static void test_pad_update_rejected_on_stale_tmp(void **state)
{
	(void)state;
	struct passwd *pw = getpwuid(getuid());
	assert_non_null(pw);

	char mnt_dir[] = "/tmp/pamusb_oexcl_XXXXXX";
	assert_non_null(mkdtemp(mnt_dir));

	t_pusb_options opts = {0};
	pusb_conf_init(&opts);
	snprintf(opts.device.name, sizeof(opts.device.name), "pamusb_oexcl_test");
	snprintf(opts.system_pad_directory, sizeof(opts.system_pad_directory),
	         ".pamusb_oexcl_unit");

	/* Create system pad dir but no pad file — makes pusb_pad_should_update return 1 */
	char sys_pad_dir[PUSB_PAD_PATH_MAX];
	snprintf(sys_pad_dir, sizeof(sys_pad_dir), "%s/.pamusb_oexcl_unit", pw->pw_dir);
	mkdir_p(sys_pad_dir);

	/* Create device pad dir */
	char dev_pad_dir[PUSB_PAD_PATH_MAX];
	snprintf(dev_pad_dir, sizeof(dev_pad_dir), "%s/.pamusb", mnt_dir);
	mkdir_p(dev_pad_dir);

	/* Pre-create a stale device pad temp file to simulate a leftover from a prior crash */
	char dev_tmp_path[PUSB_PAD_PATH_MAX + 8];
	snprintf(dev_tmp_path, sizeof(dev_tmp_path), "%s/%s.%s.pad.tmp",
	         dev_pad_dir, pw->pw_name, opts.hostname);
	write_file(dev_tmp_path, "stale", 5);

	/* pusb_pad_update must fail: O_EXCL rejects the pre-existing temp file */
	int result = pusb_pad_update(&opts, mnt_dir, pw->pw_name);
	assert_int_equal(0, result);

	/* cleanup */
	unlink(dev_tmp_path);
	rmdir(dev_pad_dir);
	rmdir(mnt_dir);
	rmdir(sys_pad_dir);
}

/* ── CWE-367 regression: mkdir-first eliminates TOCTOU in pad dir creation ── */

static void test_device_pad_dir_existing_accepted(void **state)
{
	(void)state;
	char mnt_dir[] = "/tmp/pamusb_toctou_dev_XXXXXX";
	assert_non_null(mkdtemp(mnt_dir));

	/* Pre-create the device pad directory so mkdir() will see EEXIST */
	char pad_dir[PUSB_PAD_PATH_MAX];
	snprintf(pad_dir, sizeof(pad_dir), "%s/.pamusb", mnt_dir);
	mkdir_p(pad_dir);

	t_pusb_options opts = {0};
	pusb_conf_init(&opts);
	snprintf(opts.device.name, sizeof(opts.device.name), "testdev");

	char path_out[PUSB_PAD_PATH_MAX];
	int result = pusb_pad_build_device_path(&opts, mnt_dir, "testuser", path_out, sizeof(path_out));
	assert_int_equal(1, result);

	rmdir(pad_dir);
	rmdir(mnt_dir);
}

static void test_system_pad_dir_existing_accepted(void **state)
{
	(void)state;
	struct passwd *pw = getpwuid(getuid());
	assert_non_null(pw);

	/* Pre-create the system pad directory so mkdir() will see EEXIST */
	char sys_pad_dir[PUSB_PAD_PATH_MAX];
	snprintf(sys_pad_dir, sizeof(sys_pad_dir), "%s/.pamusb_toctou_sys_unit", pw->pw_dir);
	mkdir_p(sys_pad_dir);

	t_pusb_options opts = {0};
	pusb_conf_init(&opts);
	snprintf(opts.device.name, sizeof(opts.device.name), "pamusb_toctou_sys");
	snprintf(opts.system_pad_directory, sizeof(opts.system_pad_directory),
	         ".pamusb_toctou_sys_unit");

	char path_out[PUSB_PAD_PATH_MAX];
	int result = pusb_pad_build_system_path(&opts, pw->pw_name, path_out, sizeof(path_out));
	assert_int_equal(1, result);

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
		cmocka_unit_test(test_first_run_no_pads_allowed),
		cmocka_unit_test(test_missing_system_pad_denied),
		cmocka_unit_test(test_open_pad_file_in_dir_rejects_dir_symlink),
		cmocka_unit_test(test_device_pad_path_too_long_denied),
		cmocka_unit_test(test_generate_random_bytes_fills_buffer),
		cmocka_unit_test(test_timingsafe_memcmp_equal),
		cmocka_unit_test(test_timingsafe_memcmp_differ),
		cmocka_unit_test(test_pad_update_rejected_on_stale_tmp),
		cmocka_unit_test(test_device_pad_dir_existing_accepted),
		cmocka_unit_test(test_system_pad_dir_existing_accepted),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
