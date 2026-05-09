/*
 * Fake libevdev implementation for unit testing src/evdev.c.
 *
 * Tests populate g_mock_devices[] before calling pusb_has_virtual_input_device().
 * The --wrap flags for opendir/readdir/closedir/open/close control which "device
 * files" are enumerated; fake_libevdev_new_from_fd() then returns data from the
 * mock table indexed by file descriptor (used as array index).
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <linux/input.h>
#include <libevdev/libevdev.h>

/* ── Mock device table ── */

#define FAKE_EVDEV_MAX_DEVICES 8

typedef struct {
	int  bustype;
	const char *phys;    /* NULL means no physical path */
	unsigned int event_types; /* bitmask: bit EV_KEY, EV_REL, EV_ABS */
	int  init_fails;     /* if non-zero, libevdev_new_from_fd returns -ENODEV */
} fake_device_t;

fake_device_t g_mock_devices[FAKE_EVDEV_MAX_DEVICES];
int           g_mock_device_count = 0;

void fake_evdev_reset(void)
{
	memset(g_mock_devices, 0, sizeof(g_mock_devices));
	g_mock_device_count = 0;
}

/* ── Fake DIR / dirent for /dev/input ── */

static struct dirent g_mock_dirents[FAKE_EVDEV_MAX_DEVICES];
static int           g_mock_dirent_idx = 0;

/* ── libevdev struct (opaque in real header, we define it here) ── */

struct libevdev {
	int  bustype;
	const char *phys;
	unsigned int event_types;
};

/* ── libevdev API implementations ── */

int libevdev_new_from_fd(int fd, struct libevdev **dev)
{
	/* fd is used as an index into g_mock_devices */
	int idx = fd;
	if (idx < 0 || idx >= g_mock_device_count) {
		*dev = NULL;
		return -ENODEV;
	}
	if (g_mock_devices[idx].init_fails) {
		*dev = NULL;
		return -ENODEV;
	}
	*dev = (struct libevdev *)malloc(sizeof(struct libevdev));
	if (!*dev) return -ENOMEM;
	(*dev)->bustype     = g_mock_devices[idx].bustype;
	(*dev)->phys        = g_mock_devices[idx].phys;
	(*dev)->event_types = g_mock_devices[idx].event_types;
	return 0;
}

int libevdev_get_id_bustype(const struct libevdev *dev)
{
	return dev ? dev->bustype : -1;
}

const char *libevdev_get_phys(const struct libevdev *dev)
{
	return dev ? dev->phys : NULL;
}

int libevdev_has_event_type(const struct libevdev *dev, unsigned int type)
{
	if (!dev) return 0;
	return (dev->event_types & (1u << type)) ? 1 : 0;
}

void libevdev_free(struct libevdev *dev)
{
	free(dev);
}

/* ── Wrapped libc functions ── */

static int g_fake_dir_sentinel;  /* non-NULL sentinel for fake DIR handle */

DIR *__wrap_opendir(const char *name)
{
	(void)name;
	g_mock_dirent_idx = 0;
	if (g_mock_device_count == 0)
		return NULL;
	return (DIR *)&g_fake_dir_sentinel;
}

struct dirent *__wrap_readdir(DIR *dirp)
{
	(void)dirp;
	if (g_mock_dirent_idx >= g_mock_device_count)
		return NULL;
	int idx = g_mock_dirent_idx++;
	memset(&g_mock_dirents[idx], 0, sizeof(struct dirent));
	snprintf(g_mock_dirents[idx].d_name, sizeof(g_mock_dirents[idx].d_name),
			 "event%d", idx);
	return &g_mock_dirents[idx];
}

int __wrap_closedir(DIR *dirp)
{
	(void)dirp;
	return 0;
}

int __wrap_open(const char *pathname, int flags, ...)
{
	(void)pathname;
	(void)flags;
	/* Extract the device index from the filename "eventN" */
	const char *base = strrchr(pathname, '/');
	base = base ? base + 1 : pathname;
	if (strncmp(base, "event", 5) != 0)
		return -1;
	int idx = atoi(base + 5);
	if (idx < 0 || idx >= g_mock_device_count) {
		errno = ENOENT;
		return -1;
	}
	return idx;  /* use index as fd */
}

int __wrap_close(int fd)
{
	(void)fd;
	return 0;
}
