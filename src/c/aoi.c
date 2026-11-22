/* Based on cloudwu/aoi. A significant move invalidates cached pairs.
 * Smaller moves only need to revisit pairs in the [radius, 2*radius] band. */
#include "aoi.h"
#include <stdlib.h>
#include <string.h>

#define RADIUS 30.0f
#define RADIUS2 (RADIUS * RADIUS)
#define WATCHER 1
#define MARKER 2
#define MOVED 4

struct object {
	struct object *next;
	uint32_t id;
	unsigned mode;
	size_t refs;  /* One for map membership and one for each cached pair. */
	float position[3];
	float last[3];
};

struct pair {
	struct pair *next;
	struct object *watcher;
	struct object *marker;
};

struct aoi_space {
	struct object **buckets;
	size_t capacity;
	size_t count;
	struct pair *hot;
	struct object **scratch;
	size_t scratch_capacity;
};

static float
distance2(const float a[3], const float b[3]) {
	float x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2];
	return x*x + y*y + z*z;
}

static void
release_object(struct object *obj) {
	if (--obj->refs == 0)
		free(obj);
}

static void
release_pair(struct pair *p) {
	release_object(p->watcher);
	release_object(p->marker);
	free(p);
}

struct aoi_space *
aoi_new(void) {
	struct aoi_space *s = calloc(1, sizeof(*s));
	if (s) {
		s->capacity = 16;
		s->buckets = calloc(s->capacity, sizeof(*s->buckets));
		if (!s->buckets) {
			free(s);
			return NULL;
		}
	}
	return s;
}

void
aoi_release(struct aoi_space *s) {
	while (s->hot) {
		struct pair *p = s->hot;
		s->hot = p->next;
		release_pair(p);
	}
	for (size_t i = 0; i < s->capacity; ++i) {
		struct object *obj = s->buckets[i];
		while (obj) {
			struct object *next = obj->next;
			release_object(obj);
			obj = next;
		}
	}
	free(s->scratch);
	free(s->buckets);
	free(s);
}

static size_t
bucket(uint32_t id, size_t capacity) {
	id ^= id >> 16;
	id *= UINT32_C(0x85ebca6b);
	id ^= id >> 13;
	return id & (capacity - 1);
}

static int
grow_map(struct aoi_space *s) {
	if (s->capacity > SIZE_MAX / 2 / sizeof(*s->buckets))
		return 0;
	size_t cap = s->capacity * 2;
	struct object **slots = calloc(cap, sizeof(*slots));
	if (!slots)
		return 0;
	for (size_t i = 0; i < s->capacity; ++i) {
		struct object *obj = s->buckets[i];
		while (obj) {
			struct object *next = obj->next;
			size_t j = bucket(obj->id, cap);
			obj->next = slots[j];
			slots[j] = obj;
			obj = next;
		}
	}
	free(s->buckets);
	s->buckets = slots;
	s->capacity = cap;
	return 1;
}

int
aoi_update(struct aoi_space *s, uint32_t id, const char *mode, const float pos[3]) {
	struct object **slot = &s->buckets[bucket(id, s->capacity)];
	while (*slot && (*slot)->id != id)
		slot = &(*slot)->next;
	struct object *obj = *slot;
	unsigned roles = 0;
	for (; *mode; ++mode) {
		switch (*mode) {
		case 'w': roles |= WATCHER; break;
		case 'm': roles |= MARKER; break;
		case 'd':
			if (obj) {
				*slot = obj->next;
				obj->mode = MOVED;
				--s->count;
				release_object(obj);
			}
			return 1;
		}
	}
	if (!obj) {
		if (s->count == s->capacity && !grow_map(s))
			return 0;
		obj = malloc(sizeof(*obj));
		if (!obj)
			return 0;
		slot = &s->buckets[bucket(id, s->capacity)];
		*obj = (struct object){ .next = *slot, .id = id, .refs = 1 };
		*slot = obj;
		++s->count;
		obj->mode = roles | MOVED;
		memcpy(obj->last, pos, sizeof(obj->last));
	} else if ((obj->mode & (WATCHER | MARKER)) != roles ||
		distance2(pos, obj->last) >= RADIUS2 / 4) {
		obj->mode = roles | MOVED;
		memcpy(obj->last, pos, sizeof(obj->last));
	}
	memcpy(obj->position, pos, sizeof(obj->position));
	return 1;
}

/* Keep moved objects at the front and stationary ones at the back. */
struct group {
	struct object **slots;
	size_t moving;
	size_t stationary;
};

static void
group_add(struct group *g, struct object *obj) {
	if (obj->mode & MOVED)
		g->slots[g->moving++] = obj;
	else
		g->slots[--g->stationary] = obj;
}

static int
collect_pairs(struct object *w, struct object **markers, size_t count,
	struct pair **pending, aoi_Callback *collect, void *ud) {
	for (size_t i = 0; i < count; ++i) {
		struct object *m = markers[i];
		if (w == m)
			continue;
		float d = distance2(w->position, m->position);
		if (d < RADIUS2) {
			collect(ud, w->id, m->id);
		} else if (d <= 4 * RADIUS2) {
			struct pair *p = malloc(sizeof(*p));
			if (!p)
				return 0;
			*p = (struct pair){ *pending, w, m };
			*pending = p;
		}
	}
	return 1;
}

int
aoi_message(struct aoi_space *s, aoi_Callback *collect, void *ud) {
	if (s->count > s->scratch_capacity) {
		if (s->capacity > SIZE_MAX / 2 / sizeof(*s->scratch))
			return 0;
		void *buf = realloc(s->scratch, 2 * s->capacity * sizeof(*s->scratch));
		if (!buf)
			return 0;
		s->scratch = buf;
		s->scratch_capacity = s->capacity;
	}
	struct group w = { s->scratch, 0, s->count };
	struct group m = { s->scratch, 0, s->count };
	if (s->count)
		m.slots += s->count;
	for (size_t i = 0; i < s->capacity; ++i) {
		for (struct object *o = s->buckets[i]; o; o = o->next) {
			if (o->mode & WATCHER)
				group_add(&w, o);
			if (o->mode & MARKER)
				group_add(&m, o);
		}
	}

	struct pair *pending = NULL;
	for (size_t i = 0; i < w.moving; ++i) {
		if (!collect_pairs(w.slots[i], m.slots, m.moving, &pending, collect, ud) ||
			!collect_pairs(w.slots[i], m.slots + m.stationary, s->count - m.stationary,
				&pending, collect, ud))
			goto failed;
	}
	for (size_t i = w.stationary; i < s->count; ++i) {
		if (!collect_pairs(w.slots[i], m.slots, m.moving, &pending, collect, ud))
			goto failed;
	}

	/* Commit only after all allocations succeed. A retry sees the same moves. */
	struct pair **link = &s->hot;
	while (*link) {
		struct pair *p = *link;
		if (!((p->watcher->mode | p->marker->mode) & MOVED)) {
			float d = distance2(p->watcher->position, p->marker->position);
			if (d < RADIUS2)
				collect(ud, p->watcher->id, p->marker->id);
			else if (d <= 4 * RADIUS2) {
				link = &p->next;
				continue;
			}
		}
		*link = p->next;
		release_pair(p);
	}
	while (pending) {
		struct pair *p = pending;
		pending = p->next;
		++p->watcher->refs;
		++p->marker->refs;
		p->next = s->hot;
		s->hot = p;
	}
	for (size_t i = 0; i < w.moving; ++i)
		w.slots[i]->mode &= ~MOVED;
	for (size_t i = 0; i < m.moving; ++i)
		m.slots[i]->mode &= ~MOVED;
	return 1;

failed:
	while (pending) {
		struct pair *next = pending->next;
		free(pending);
		pending = next;
	}
	return 0;
}
