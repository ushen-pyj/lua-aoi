#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include "aoi_quad.h"

#define AOI_RADIS 30.0f

#define INVALID_ID (~0)
#define PRE_ALLOC 16
#define AOI_RADIS2 (AOI_RADIS * AOI_RADIS)
#define DIST2(p1,p2) ((p1[0] - p2[0]) * (p1[0] - p2[0]) + (p1[1] - p2[1]) * (p1[1] - p2[1]) + (p1[2] - p2[2]) * (p1[2] - p2[2]))
#define MODE_WATCHER 1
#define MODE_MARKER 2
#define MODE_MOVE 4
#define MODE_DROP 8

// 默认四叉树配置
#define DEFAULT_QUADTREE_WIDTH 1000.0f
#define DEFAULT_QUADTREE_HEIGHT 1000.0f
#define DEFAULT_MAX_OBJECTS 8
#define DEFAULT_MAX_DEPTH 5

struct object {
    int ref;
    uint32_t id;
    int version;
    int mode;
    float last[3];
    float position[3];
    struct quadtree_node *node; // 所在的四叉树节点
};

struct object_set {
    int cap;
    int number;
    struct object **slot;
};

struct pair_list {
    struct pair_list *next;
    struct object *watcher;
    struct object *marker;
    int watcher_version;
    int marker_version;
};

struct map_slot {
    uint32_t id;
    struct object *obj;
    int next;
};

struct map {
    int size;
    int lastfree;
    struct map_slot *slot;
};

// 四叉树节点
struct quadtree_node {
    float x;           // 区域左上角x坐标
    float y;           // 区域左上角y坐标
    float width;       // 区域宽度
    float height;      // 区域高度
    int depth;         // 当前深度
    bool divided;      // 是否已分割
    
    struct object_set *objects;  // 当前节点中的对象
    
    // 子节点
    struct quadtree_node *northwest;
    struct quadtree_node *northeast;
    struct quadtree_node *southwest;
    struct quadtree_node *southeast;
};

struct aoi_space {
    aoi_Alloc alloc;
    void *alloc_ud;
    struct map *object;
    struct pair_list *hot;
    
    // 四叉树配置
    float width;
    float height;
    int max_objects;
    int max_depth;
    
    // 四叉树根节点
    struct quadtree_node *quadtree;

    // 新增临时集合
    struct object_set *watcher_static;
    struct object_set *watcher_move;
    struct object_set *marker_static;
    struct object_set *marker_move;
};

struct aoi_callback_args {
    aoi_Callback *cb;
    void *ud;
    struct aoi_space *space;
};

// 前置声明
static void function_reinsert_object(void *ud, struct object *obj);

// 四叉树操作前置声明
static struct quadtree_node *quadtree_create(struct aoi_space *space, float x, float y, float width, float height, int depth);
static void quadtree_subdivide(struct aoi_space *space, struct quadtree_node *node);
static bool quadtree_insert(struct aoi_space *space, struct quadtree_node *node, struct object *obj);
static void quadtree_remove(struct quadtree_node *node, struct object *obj);
static void quadtree_release(struct aoi_space *space, struct quadtree_node *node);
static void quadtree_query_range(struct aoi_space *space, struct quadtree_node *node, float x, float y, float radius, struct object_set *result);

// 对象集合操作
static struct object_set *
set_new(struct aoi_space *space) {
    struct object_set *set = space->alloc(space->alloc_ud, NULL, sizeof(*set));
    set->cap = PRE_ALLOC;
    set->number = 0;
    set->slot = space->alloc(space->alloc_ud, NULL, set->cap * sizeof(struct object *));
    return set;
}

static void
set_push_back(struct aoi_space *space, struct object_set *set, struct object *obj) {
    if (set->number >= set->cap) {
        int cap = set->cap * 2;
        void *tmp = set->slot;
        set->slot = space->alloc(space->alloc_ud, NULL, cap * sizeof(struct object *));
        memcpy(set->slot, tmp, set->cap * sizeof(struct object *));
        space->alloc(space->alloc_ud, tmp, set->cap * sizeof(struct object *));
        set->cap = cap;
    }
    set->slot[set->number] = obj;
    ++set->number;
}

static void
delete_set(struct aoi_space *space, struct object_set *set) {
    if (set->slot) {
        space->alloc(space->alloc_ud, set->slot, sizeof(struct object *) * set->cap);
    }
    space->alloc(space->alloc_ud, set, sizeof(*set));
}

// 对象操作
static struct object *
new_object(struct aoi_space *space, uint32_t id) {
    struct object *obj = space->alloc(space->alloc_ud, NULL, sizeof(*obj));
    obj->ref = 1;
    obj->id = id;
    obj->version = 0;
    obj->mode = 0;
    obj->node = NULL;
    return obj;
}

static void
grab_object(struct object *obj) {
    ++obj->ref;
}

static void
delete_object(void *s, struct object *obj) {
    struct aoi_space *space = s;
    space->alloc(space->alloc_ud, obj, sizeof(*obj));
}

static void
delete_pair_list(struct aoi_space *space) {
    struct pair_list *p = space->hot;
    while (p) {
        struct pair_list *next = p->next;
        space->alloc(space->alloc_ud, p, sizeof(*p));
        p = next;
    }
}

// 哈希表操作
static inline struct map_slot *
mainposition(struct map *m, uint32_t id) {
    uint32_t hash = id & (m->size-1);
    return &m->slot[hash];
}

static void rehash(struct aoi_space *space, struct map *m);

static void
map_insert(struct aoi_space *space, struct map *m, uint32_t id, struct object *obj) {
    struct map_slot *s = mainposition(m, id);
    if (s->id == INVALID_ID) {
        s->id = id;
        s->obj = obj;
        return;
    }
    if (mainposition(m, s->id) != s) {
        struct map_slot *last = mainposition(m, s->id);
        while (last->next != s - m->slot) {
            assert(last->next >= 0);
            last = &m->slot[last->next];
        }
        uint32_t temp_id = s->id;
        struct object *temp_obj = s->obj;
        last->next = s->next;
        s->id = id;
        s->obj = obj;
        s->next = -1;
        if (temp_obj) {
            map_insert(space, m, temp_id, temp_obj);
        }
        return;
    }
    while (m->lastfree >= 0) {
        struct map_slot *temp = &m->slot[m->lastfree--];
        if (temp->id == INVALID_ID) {
            temp->id = id;
            temp->obj = obj;
            temp->next = s->next;
            s->next = (int)(temp - m->slot);
            return;
        }
    }
    rehash(space, m);
    map_insert(space, m, id, obj);
}

static void
rehash(struct aoi_space *space, struct map *m) {
    struct map_slot *old_slot = m->slot;
    int old_size = m->size;
    m->size = 2 * old_size;
    m->lastfree = m->size - 1;
    m->slot = space->alloc(space->alloc_ud, NULL, m->size * sizeof(struct map_slot));
    int i;
    for (i = 0; i < m->size; i++) {
        struct map_slot *s = &m->slot[i];
        s->id = INVALID_ID;
        s->obj = NULL;
        s->next = -1;
    }
    for (i = 0; i < old_size; i++) {
        struct map_slot *s = &old_slot[i];
        if (s->obj) {
            map_insert(space, m, s->id, s->obj);
        }
    }
    space->alloc(space->alloc_ud, old_slot, old_size * sizeof(struct map_slot));
}

static struct object *
map_query(struct aoi_space *space, struct map *m, uint32_t id) {
    struct map_slot *s = mainposition(m, id);
    for (;;) {
        if (s->id == id) {
            if (s->obj == NULL) {
                s->obj = new_object(space, id);
            }
            return s->obj;
        }
        if (s->next < 0) {
            break;
        }
        s = &m->slot[s->next];
    }
    struct object *obj = new_object(space, id);
    map_insert(space, m, id, obj);
    return obj;
}

static struct object *
map_drop(struct map *m, uint32_t id) {
    uint32_t hash = id & (m->size-1);
    struct map_slot *s = &m->slot[hash];
    for (;;) {
        if (s->id == id) {
            struct object *obj = s->obj;
            s->obj = NULL;
            return obj;
        }
        if (s->next < 0) {
            return NULL;
        }
        s = &m->slot[s->next];
    }
}

static void
map_foreach(struct map *m, void (*func)(void *ud, struct object *obj), void *ud) {
    int i;
    for (i = 0; i < m->size; i++) {
        if (m->slot[i].obj) {
            func(ud, m->slot[i].obj);
        }
    }
}

static void
map_delete(struct aoi_space *space, struct map *m) {
    space->alloc(space->alloc_ud, m->slot, m->size * sizeof(struct map_slot));
    space->alloc(space->alloc_ud, m, sizeof(*m));
}

static struct map *
map_new(struct aoi_space *space) {
    int i;
    struct map *m = space->alloc(space->alloc_ud, NULL, sizeof(*m));
    m->size = PRE_ALLOC;
    m->lastfree = PRE_ALLOC - 1;
    m->slot = space->alloc(space->alloc_ud, NULL, m->size * sizeof(struct map_slot));
    for (i = 0; i < m->size; i++) {
        struct map_slot *s = &m->slot[i];
        s->id = INVALID_ID;
        s->obj = NULL;
        s->next = -1;
    }
    return m;
}

// 四叉树实现
static struct quadtree_node *
quadtree_create(struct aoi_space *space, float x, float y, float width, float height, int depth) {
    struct quadtree_node *node = space->alloc(space->alloc_ud, NULL, sizeof(*node));
    node->x = x;
    node->y = y;
    node->width = width;
    node->height = height;
    node->depth = depth;
    node->divided = false;
    node->objects = set_new(space);
    node->northwest = NULL;
    node->northeast = NULL;
    node->southwest = NULL;
    node->southeast = NULL;
    return node;
}

static void
quadtree_subdivide(struct aoi_space *space, struct quadtree_node *node) {
    float x = node->x;
    float y = node->y;
    float half_width = node->width / 2;
    float half_height = node->height / 2;
    int next_depth = node->depth + 1;
    
    node->northwest = quadtree_create(space, x, y, half_width, half_height, next_depth);
    node->northeast = quadtree_create(space, x + half_width, y, half_width, half_height, next_depth);
    node->southwest = quadtree_create(space, x, y + half_height, half_width, half_height, next_depth);
    node->southeast = quadtree_create(space, x + half_width, y + half_height, half_width, half_height, next_depth);
    
    node->divided = true;
    
    // 重新分配现有对象到子节点
    for (int i = 0; i < node->objects->number; i++) {
        struct object *obj = node->objects->slot[i];
        bool inserted = false;
        
        if (quadtree_insert(space, node->northwest, obj)) inserted = true;
        else if (quadtree_insert(space, node->northeast, obj)) inserted = true;
        else if (quadtree_insert(space, node->southwest, obj)) inserted = true;
        else if (quadtree_insert(space, node->southeast, obj)) inserted = true;
        
        if (!inserted) {
            // 如果无法插入子节点，保留在当前节点
        }
    }
}

static bool
quadtree_insert(struct aoi_space *space, struct quadtree_node *node, struct object *obj) {
    // 检查对象是否在节点范围内
    float x = obj->position[0];
    float y = obj->position[1];
    
    if (x < node->x || x > node->x + node->width || 
        y < node->y || y > node->y + node->height) {
        return false;
    }
    
    // 如果节点未满且已达最大深度，将对象添加到当前节点
    if (node->objects->number < space->max_objects || node->depth >= space->max_depth) {
        set_push_back(space, node->objects, obj);
        obj->node = node;
        return true;
    }
    
    // 如果节点未分割，则进行分割
    if (!node->divided) {
        quadtree_subdivide(space, node);
    }
    
    // 尝试将对象插入子节点
    if (quadtree_insert(space, node->northwest, obj)) return true;
    if (quadtree_insert(space, node->northeast, obj)) return true;
    if (quadtree_insert(space, node->southwest, obj)) return true;
    if (quadtree_insert(space, node->southeast, obj)) return true;
    
    // 如果无法插入子节点，添加到当前节点
    set_push_back(space, node->objects, obj);
    obj->node = node;
    return true;
}

static void
quadtree_remove(struct quadtree_node *node, struct object *obj) {
    // 从节点的对象列表中移除对象
    if (node == NULL || obj == NULL) return;
    
    for (int i = 0; i < node->objects->number; i++) {
        if (node->objects->slot[i] == obj) {
            // 移除对象（将最后一个对象移到当前位置）
            if (i != node->objects->number - 1) {
                node->objects->slot[i] = node->objects->slot[node->objects->number - 1];
            }
            node->objects->number--;
            obj->node = NULL;
            break;
        }
    }
}

static void
quadtree_release(struct aoi_space *space, struct quadtree_node *node) {
    if (node == NULL) return;
    
    // 递归释放子节点
    if (node->divided) {
        quadtree_release(space, node->northwest);
        quadtree_release(space, node->northeast);
        quadtree_release(space, node->southwest);
        quadtree_release(space, node->southeast);
    }
    
    delete_set(space, node->objects);
    space->alloc(space->alloc_ud, node, sizeof(*node));
}

static void
quadtree_query_range(struct aoi_space *space, struct quadtree_node *node, float x, float y, float radius, struct object_set *result) {
    if (node == NULL) return;
    
    // 检查圆形范围是否与节点相交
    float dx = x - fmaxf(node->x, fminf(x, node->x + node->width));
    float dy = y - fmaxf(node->y, fminf(y, node->y + node->height));
    
    if (dx * dx + dy * dy > radius * radius) {
        return; // 节点与圆形范围不相交
    }
    
    // 添加当前节点中的对象到结果集
    for (int i = 0; i < node->objects->number; i++) {
        struct object *obj = node->objects->slot[i];
        float dist_x = obj->position[0] - x;
        float dist_y = obj->position[1] - y;
        
        if (dist_x * dist_x + dist_y * dist_y <= radius * radius) {
            // 检查对象是否已在结果集中
            bool found = false;
            for (int j = 0; j < result->number; j++) {
                if (result->slot[j] == obj) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                set_push_back(space, result, obj);
            }
        }
    }
    
    // 如果节点已分割，递归查询子节点
    if (node->divided) {
        quadtree_query_range(space, node->northwest, x, y, radius, result);
        quadtree_query_range(space, node->northeast, x, y, radius, result);
        quadtree_query_range(space, node->southwest, x, y, radius, result);
        quadtree_query_range(space, node->southeast, x, y, radius, result);
    }
}

// 辅助函数
static inline void 
copy_position(float des[3], float src[3]) {
    des[0] = src[0];
    des[1] = src[1];
    des[2] = src[2];
}

static bool
change_mode(struct object *obj, bool set_watcher, bool set_marker) {
    bool change = false;
    if (obj->mode == 0) {
        if (set_watcher) {
            obj->mode = MODE_WATCHER;
        }
        if (set_marker) {
            obj->mode |= MODE_MARKER;
        }
        return true;
    }
    if (set_watcher) {
        if (!(obj->mode & MODE_WATCHER)) {
            obj->mode |= MODE_WATCHER;
            change = true;
        }
    } else {
        if (obj->mode & MODE_WATCHER) {
            obj->mode &= ~MODE_WATCHER;
            change = true;
        }
    }
    if (set_marker) {
        if (!(obj->mode & MODE_MARKER)) {
            obj->mode |= MODE_MARKER;
            change = true;
        }
    } else {
        if (obj->mode & MODE_MARKER) {
            obj->mode &= ~MODE_MARKER;
            change = true;
        }
    }
    return change;
}

static inline bool
is_near(float p1[3], float p2[3]) {
    return DIST2(p1, p2) < AOI_RADIS2 * 0.25f;
}

static inline float
dist2(struct object *p1, struct object *p2) {
    float d = DIST2(p1->position, p2->position);
    return d;
}

static void
drop_object(struct aoi_space *space, struct object *obj) {
    --obj->ref;
    if (obj->ref <= 0) {
        // 从四叉树中移除对象
        if (obj->node != NULL) {
            quadtree_remove(obj->node, obj);
        }
        map_drop(space->object, obj->id);
        delete_object(space, obj);
    }
}

static void
drop_pair(struct aoi_space *space, struct pair_list *p) {
    drop_object(space, p->watcher);
    drop_object(space, p->marker);
    space->alloc(space->alloc_ud, p, sizeof(*p));
}

static void
flush_pair(struct aoi_space *space, aoi_Callback cb, void *ud) {
    struct pair_list **last = &(space->hot);
    struct pair_list *p = *last;
    while (p) {
        struct pair_list *next = p->next;
        if (p->watcher->version != p->watcher_version ||
            p->marker->version != p->marker_version ||
            (p->watcher->mode & MODE_DROP) ||
            (p->marker->mode & MODE_DROP)
            ) {
            drop_pair(space, p);
            *last = next;
        } else {
            float distance2 = dist2(p->watcher, p->marker);
            if (distance2 > AOI_RADIS2 * 4) {
                drop_pair(space, p);
                *last = next;
            } else if (distance2 < AOI_RADIS2) {
                cb(ud, p->watcher->id, p->marker->id);
                drop_pair(space, p);
                *last = next;
            } else {
                last = &(p->next);
            }
        }
        p = next;
    }
}

static void
gen_pair(struct aoi_space *space, struct object *watcher, struct object *marker, aoi_Callback cb, void *ud) {
    if (watcher == marker) {
        return;
    }
    float distance2 = dist2(watcher, marker);
    if (distance2 < AOI_RADIS2) {
        cb(ud, watcher->id, marker->id);
        return;
    }
    if (distance2 > AOI_RADIS2 * 4) {
        return;
    }
    struct pair_list *p = space->alloc(space->alloc_ud, NULL, sizeof(*p));
    p->watcher = watcher;
    grab_object(watcher);
    p->marker = marker;
    grab_object(marker);
    p->watcher_version = watcher->version;
    p->marker_version = marker->version;
    p->next = space->hot;
    space->hot = p;
}

// 新增gen_pair_list函数
static void
gen_pair_list(struct aoi_space *space, struct object_set *watcher, struct object_set *marker, aoi_Callback cb, void *ud) {
    int i, j;
    for (i = 0; i < watcher->number; i++) {
        for (j = 0; j < marker->number; j++) {
            gen_pair(space, watcher->slot[i], marker->slot[j], cb, ud);
        }
    }
}

// 新增set_push函数
static void
set_push(void *s, struct object *obj) {
    struct aoi_space *space = s;
    int mode = obj->mode;
    
    if (mode & MODE_DROP) {
        return;
    }
    
    if (mode & MODE_WATCHER) {
        if (mode & MODE_MOVE) {
            set_push_back(space, space->watcher_move, obj);
            obj->mode &= ~MODE_MOVE;
        } else {
            set_push_back(space, space->watcher_static, obj);
        }
    }
    
    if (mode & MODE_MARKER) {
        if (mode & MODE_MOVE) {
            set_push_back(space, space->marker_move, obj);
            obj->mode &= ~MODE_MOVE;
        } else {
            set_push_back(space, space->marker_static, obj);
        }
    }
}

// 公共API实现
struct aoi_space * 
aoi_create(aoi_Alloc alloc, void *ud) {
    struct aoi_space *space = alloc(ud, NULL, sizeof(*space));
    space->alloc = alloc;
    space->alloc_ud = ud;
    space->object = map_new(space);
    space->hot = NULL;
    
    // 设置四叉树默认配置
    space->width = DEFAULT_QUADTREE_WIDTH;
    space->height = DEFAULT_QUADTREE_HEIGHT;
    space->max_objects = DEFAULT_MAX_OBJECTS;
    space->max_depth = DEFAULT_MAX_DEPTH;
    
    // 创建四叉树根节点
    space->quadtree = quadtree_create(space, 0, 0, space->width, space->height, 0);
    
    // 初始化临时集合指针为NULL
    space->watcher_static = NULL;
    space->watcher_move = NULL;
    space->marker_static = NULL;
    space->marker_move = NULL;
    
    return space;
}

void 
aoi_release(struct aoi_space *space) {
    map_foreach(space->object, delete_object, space);
    map_delete(space, space->object);
    delete_pair_list(space);
    
    // 释放四叉树
    quadtree_release(space, space->quadtree);
    
    // 释放临时集合（如果存在）
    if (space->watcher_static != NULL) {
        delete_set(space, space->watcher_static);
    }
    if (space->watcher_move != NULL) {
        delete_set(space, space->watcher_move);
    }
    if (space->marker_static != NULL) {
        delete_set(space, space->marker_static);
    }
    if (space->marker_move != NULL) {
        delete_set(space, space->marker_move);
    }
    
    space->alloc(space->alloc_ud, space, sizeof(*space));
}

void
aoi_update(struct aoi_space *space, uint32_t id, const char *modestring, float pos[3]) {
    struct object *obj = map_query(space, space->object, id);
    int i;
    bool set_watcher = false;
    bool set_marker = false;
    
    for (i = 0; modestring[i]; ++i) {
        char m = modestring[i];
        switch (m) {
        case 'w':
            set_watcher = true;
            break;
        case 'm':
            set_marker = true;
            break;
        case 'd':
            if (!(obj->mode & MODE_DROP)) {
                obj->mode = MODE_DROP;
                
                // 从四叉树中移除对象
                if (obj->node != NULL) {
                    quadtree_remove(obj->node, obj);
                }
                
                drop_object(space, obj);
            }
            return;
        }
    }
    
    if (obj->mode & MODE_DROP) {
        obj->mode &= ~MODE_DROP;
        grab_object(obj);
    }
    
    bool changed = change_mode(obj, set_watcher, set_marker);
    bool position_changed = !is_near(pos, obj->position);
    
    // 如果位置改变，从原四叉树节点中移除
    if (position_changed && obj->node != NULL) {
        quadtree_remove(obj->node, obj);
    }
    
    copy_position(obj->position, pos);
    if (changed || position_changed) {
        // 新对象、对象模式变更或位置变更
        copy_position(obj->last, pos);
        obj->mode |= MODE_MOVE;
        ++obj->version;
        
        // 将对象插入四叉树
        quadtree_insert(space, space->quadtree, obj);
    }
}

void 
aoi_message(struct aoi_space *space, aoi_Callback cb, void *ud) {
    flush_pair(space, cb, ud);
    
    // 为空间创建临时集合
    space->watcher_static = set_new(space);
    space->watcher_move = set_new(space);
    space->marker_static = set_new(space);
    space->marker_move = set_new(space);
    
    // 对所有对象进行分类
    map_foreach(space->object, set_push, space);
    
    // 按照原来的方式处理三种组合
    gen_pair_list(space, space->watcher_static, space->marker_move, cb, ud);
    gen_pair_list(space, space->watcher_move, space->marker_static, cb, ud);
    gen_pair_list(space, space->watcher_move, space->marker_move, cb, ud);
    
    // 清理临时集合
    delete_set(space, space->watcher_static);
    delete_set(space, space->watcher_move);
    delete_set(space, space->marker_static);
    delete_set(space, space->marker_move);
    
    // 重置集合指针
    space->watcher_static = NULL;
    space->watcher_move = NULL; 
    space->marker_static = NULL;
    space->marker_move = NULL;
}

void 
aoi_set_quadtree_config(struct aoi_space *space, float width, float height, int max_objects, int max_depth) {
    // 保存旧的四叉树
    struct quadtree_node *old_quadtree = space->quadtree;
    
    // 更新配置
    space->width = width;
    space->height = height;
    space->max_objects = max_objects;
    space->max_depth = max_depth;
    
    // 创建新的四叉树
    space->quadtree = quadtree_create(space, 0, 0, width, height, 0);
    
    // 将所有对象从旧四叉树移动到新四叉树
    map_foreach(space->object, function_reinsert_object, space);
    
    // 释放旧四叉树
    quadtree_release(space, old_quadtree);
}

// 重新插入对象到新四叉树的函数
static void
function_reinsert_object(void *ud, struct object *obj) {
    struct aoi_space *space = ud;
    obj->node = NULL; // 清除旧节点引用
    quadtree_insert(space, space->quadtree, obj);
}

static void *
default_alloc(void *ud, void *ptr, size_t sz) {
    if (ptr == NULL) {
        void *p = malloc(sz);
        return p;
    }
    free(ptr);
    return NULL;
}

struct aoi_space * 
aoi_new() {
    return aoi_create(default_alloc, NULL);
} 