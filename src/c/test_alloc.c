/* Linker wrappers fail one AOI allocation per run. */
#include "aoi.h"
#undef NDEBUG
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__real_realloc(void *, size_t);

static int remaining;

static int
fail(void) {
	return remaining > 0 && --remaining == 0;
}

void *__wrap_malloc(size_t n) { return fail() ? NULL : __real_malloc(n); }
void *__wrap_calloc(size_t n, size_t size) { return fail() ? NULL : __real_calloc(n, size); }
void *__wrap_realloc(void *p, size_t n) { return fail() ? NULL : __real_realloc(p, n); }

static void
count_hit(void *ud, uint32_t w, uint32_t m) {
	assert(w == 0 && m >= 1 && m <= 20);
	++*(int *)ud;
}

static void
scene(int fail_at) {
	remaining = fail_at;
	struct aoi_space *s = aoi_new();
	if (!s) {
		assert(remaining == 0);
		return;
	}
	for (uint32_t i = 0; i <= 20; ++i) {
		float pos[3] = { i % 2 ? 31 : 0, 0, 0 };
		const char *role = i ? "m" : "w";
		if (!aoi_update(s, i, role, pos))
			assert(aoi_update(s, i, role, pos));
		/* Allocate the initial scratch space before the map grows. */
		if (i == 0) {
			int hits = 0;
			if (!aoi_message(s, count_hit, &hits))
				assert(aoi_message(s, count_hit, &hits));
			assert(hits == 0);
		}
	}
	int hits = 0;
	if (!aoi_message(s, count_hit, &hits)) {
		hits = 0;
		assert(aoi_message(s, count_hit, &hits));
	}
	assert(hits == 10);
	for (uint32_t i = 1; i <= 20; i += 2)
		assert(aoi_update(s, i, "m", (float[3]){29, 0, 0}));
	hits = 0;
	assert(aoi_message(s, count_hit, &hits));
	assert(hits == 10);
	aoi_release(s);
}

int
main(void) {
	for (int n = 1; ; ++n) {
		scene(n);
		if (remaining > 0)
			break;
	}
	puts("AOI allocation tests passed");
	return 0;
}
