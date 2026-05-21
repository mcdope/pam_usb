/*
 * Unit tests for src/mem.c
 *
 * Verifies that xfree() calls explicit_bzero() before releasing the old
 * allocation so that sensitive heap-allocated data (pad bytes, device
 * serials, XPath queries) is cleared before the allocator reclaims the
 * region.
 *
 * explicit_bzero is wrapped at link time with --wrap=explicit_bzero so the
 * test can observe the call without relying on post-free memory inspection.
 */

#define _GNU_SOURCE
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <malloc.h>
#include <cmocka.h>

#include "../../../src/mem.c"

/* ── explicit_bzero mock ── */

static int    g_bzero_called = 0;
static size_t g_bzero_len    = 0;

void __wrap_explicit_bzero(void *ptr, size_t len)
{
	g_bzero_called++;
	g_bzero_len = len;
	if (ptr)
		memset(ptr, 0, len);
}

/* ── xfree(NULL) must not crash ── */

static void test_xfree_null_safe(void **state)
{
	(void)state;
	g_bzero_called = 0;
	xfree(NULL);
	assert_int_equal(0, g_bzero_called);
}

/* ── xfree() must call explicit_bzero with the full allocation size ──
 *
 * Allocate a known-size buffer, fill it with a sentinel, then free it.
 * The explicit_bzero wrapper records whether it was called and with what
 * length. malloc_usable_size() may return slightly more than the requested
 * size, so we assert len >= requested rather than len == requested.
 */
static void test_xfree_clears_memory(void **state)
{
	(void)state;
	const size_t sz = 64;
	char *ptr = xmalloc(sz);
	memset(ptr, 0xAA, sz);

	g_bzero_called = 0;
	g_bzero_len    = 0;

	xfree(ptr);

	assert_int_equal(1, g_bzero_called);
	assert_true(g_bzero_len >= sz);
}


int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_xfree_null_safe),
		cmocka_unit_test(test_xfree_clears_memory),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
