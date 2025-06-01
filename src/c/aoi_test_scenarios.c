#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "../../aoi/aoi.h"

// 如果M_PI未定义，手动定义它
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 测试场景类型
typedef enum {
    SCENARIO_RANDOM,       // 随机移动
    SCENARIO_CIRCLE,       // 圆形轨迹
    SCENARIO_CROSSING,     // 交叉路径
    SCENARIO_GRID,         // 网格移动
    SCENARIO_FOLLOW,       // 跟随行为
    SCENARIO_COUNT         // 场景数量
} TestScenario;

// 实体类型
typedef struct {
    uint32_t id;
    float pos[3];
    float v[3];
    char mode[4];
    bool active;
    
    // 用于特定场景的参数
    struct {
        float radius;  // 圆形轨迹半径
        float angle;   // 当前角度
        float speed;   // 角速度
        uint32_t target_id; // 跟随目标ID
    } params;
} TestEntity;

#define MAX_TEST_ENTITIES 100
TestEntity test_entities[MAX_TEST_ENTITIES];
int test_entity_count = 0;

// 交互计数
typedef struct {
    uint32_t watcher;
    uint32_t marker;
    int count;
} InteractionCount;

#define MAX_INTERACTION_COUNTS 1000
InteractionCount interaction_counts[MAX_INTERACTION_COUNTS];
int interaction_count_size = 0;

// 碰撞计数器
int collision_count = 0;

// 找到或创建交互计数记录
static InteractionCount* find_or_create_interaction(uint32_t watcher, uint32_t marker) {
    // 查找现有记录
    for (int i = 0; i < interaction_count_size; i++) {
        if (interaction_counts[i].watcher == watcher && 
            interaction_counts[i].marker == marker) {
            return &interaction_counts[i];
        }
    }
    
    // 创建新记录
    if (interaction_count_size < MAX_INTERACTION_COUNTS) {
        InteractionCount* ic = &interaction_counts[interaction_count_size++];
        ic->watcher = watcher;
        ic->marker = marker;
        ic->count = 0;
        return ic;
    }
    
    return NULL; // 没有空间
}

// AOI回调函数
static void test_callback(void *ud, uint32_t watcher, uint32_t marker) {
    InteractionCount* ic = find_or_create_interaction(watcher, marker);
    if (ic) {
        ic->count++;
    }
    
    printf("交互: %u -> %u\n", watcher, marker);
}

// 创建测试实体
void create_test_entity(uint32_t id, float x, float y, float vx, float vy, const char *mode) {
    if (test_entity_count < MAX_TEST_ENTITIES) {
        TestEntity *e = &test_entities[test_entity_count++];
        e->id = id;
        e->pos[0] = x;
        e->pos[1] = y;
        e->pos[2] = 0;
        e->v[0] = vx;
        e->v[1] = vy;
        e->v[2] = 0;
        e->active = true;
        strncpy(e->mode, mode, 3);
        e->mode[3] = '\0';
        
        // 初始化特殊参数
        e->params.radius = 50.0f + (rand() % 100);
        e->params.angle = (rand() % 360) * M_PI / 180.0f;
        e->params.speed = 0.01f + (rand() % 10) / 100.0f;
        e->params.target_id = rand() % MAX_TEST_ENTITIES;
    }
}

// 随机移动场景更新
void update_random_movement(TestEntity *e) {
    // 偶尔改变方向
    if (rand() % 100 < 2) {
        e->v[0] = (rand() % 20 - 10) / 10.0f;
        e->v[1] = (rand() % 20 - 10) / 10.0f;
    }
    
    // 更新位置
    e->pos[0] += e->v[0];
    e->pos[1] += e->v[1];
    
    // 边界检查
    if (e->pos[0] < 0) {
        e->pos[0] = 0;
        e->v[0] = -e->v[0];
    } else if (e->pos[0] > 800) {
        e->pos[0] = 800;
        e->v[0] = -e->v[0];
    }
    
    if (e->pos[1] < 0) {
        e->pos[1] = 0;
        e->v[1] = -e->v[1];
    } else if (e->pos[1] > 600) {
        e->pos[1] = 600;
        e->v[1] = -e->v[1];
    }
}

// 圆形轨迹场景更新
void update_circle_movement(TestEntity *e) {
    e->params.angle += e->params.speed;
    if (e->params.angle > 2 * M_PI) {
        e->params.angle -= 2 * M_PI;
    }
    
    float center_x = 400.0f;
    float center_y = 300.0f;
    
    e->pos[0] = center_x + e->params.radius * cos(e->params.angle);
    e->pos[1] = center_y + e->params.radius * sin(e->params.angle);
}

// 交叉路径场景更新
void update_crossing_movement(TestEntity *e) {
    // 沿直线移动
    e->pos[0] += e->v[0];
    e->pos[1] += e->v[1];
    
    // 到达边界时重新设置位置
    if (e->pos[0] < 0 || e->pos[0] > 800 || e->pos[1] < 0 || e->pos[1] > 600) {
        // 重新从边缘开始，但方向相同
        if (fabs(e->v[0]) > fabs(e->v[1])) {
            // 主要是水平移动
            e->pos[0] = (e->v[0] > 0) ? 0 : 800;
            e->pos[1] = rand() % 600;
        } else {
            // 主要是垂直移动
            e->pos[0] = rand() % 800;
            e->pos[1] = (e->v[1] > 0) ? 0 : 600;
        }
    }
}

// 网格移动场景更新
void update_grid_movement(TestEntity *e) {
    // 每个实体在一个虚拟网格上移动
    e->pos[0] += e->v[0];
    e->pos[1] += e->v[1];
    
    // 到达网格节点时可能改变方向
    if (fmod(e->pos[0], 100) < fabs(e->v[0]) && fmod(e->pos[1], 100) < fabs(e->v[1])) {
        // 在网格交叉点，有概率改变方向
        if (rand() % 100 < 30) {
            // 只选择上下左右四个方向之一
            int dir = rand() % 4;
            switch (dir) {
                case 0: e->v[0] = 1.0f; e->v[1] = 0; break; // 右
                case 1: e->v[0] = -1.0f; e->v[1] = 0; break; // 左
                case 2: e->v[0] = 0; e->v[1] = 1.0f; break; // 下
                case 3: e->v[0] = 0; e->v[1] = -1.0f; break; // 上
            }
        }
    }
    
    // 边界检查
    if (e->pos[0] < 0) {
        e->pos[0] = 0;
        e->v[0] = fabs(e->v[0]);
    } else if (e->pos[0] > 800) {
        e->pos[0] = 800;
        e->v[0] = -fabs(e->v[0]);
    }
    
    if (e->pos[1] < 0) {
        e->pos[1] = 0;
        e->v[1] = fabs(e->v[1]);
    } else if (e->pos[1] > 600) {
        e->pos[1] = 600;
        e->v[1] = -fabs(e->v[1]);
    }
}

// 跟随行为场景更新
void update_follow_movement(TestEntity *e) {
    // 寻找目标
    TestEntity *target = NULL;
    for (int i = 0; i < test_entity_count; i++) {
        if (test_entities[i].id == e->params.target_id && test_entities[i].active) {
            target = &test_entities[i];
            break;
        }
    }
    
    if (target) {
        // 计算到目标的方向
        float dx = target->pos[0] - e->pos[0];
        float dy = target->pos[1] - e->pos[1];
        float dist = sqrt(dx*dx + dy*dy);
        
        if (dist > 0) {
            // 设置速度朝向目标，但保持固定大小
            float speed = sqrt(e->v[0]*e->v[0] + e->v[1]*e->v[1]);
            if (speed < 0.1f) speed = 1.0f; // 默认速度
            
            e->v[0] = dx / dist * speed;
            e->v[1] = dy / dist * speed;
        }
    }
    
    // 更新位置
    e->pos[0] += e->v[0];
    e->pos[1] += e->v[1];
    
    // 边界检查
    if (e->pos[0] < 0) {
        e->pos[0] = 0;
        e->v[0] = fabs(e->v[0]);
    } else if (e->pos[0] > 800) {
        e->pos[0] = 800;
        e->v[0] = -fabs(e->v[0]);
    }
    
    if (e->pos[1] < 0) {
        e->pos[1] = 0;
        e->v[1] = fabs(e->v[1]);
    } else if (e->pos[1] > 600) {
        e->pos[1] = 600;
        e->v[1] = -fabs(e->v[1]);
    }
}

// 根据场景更新实体
void update_entity_by_scenario(TestEntity *e, TestScenario scenario) {
    switch (scenario) {
        case SCENARIO_RANDOM:
            update_random_movement(e);
            break;
        case SCENARIO_CIRCLE:
            update_circle_movement(e);
            break;
        case SCENARIO_CROSSING:
            update_crossing_movement(e);
            break;
        case SCENARIO_GRID:
            update_grid_movement(e);
            break;
        case SCENARIO_FOLLOW:
            update_follow_movement(e);
            break;
        default:
            update_random_movement(e);
            break;
    }
}

// 更新测试实体
void update_test_entity(struct aoi_space *space, int index, TestScenario scenario) {
    TestEntity *e = &test_entities[index];
    if (!e->active) return;
    
    // 根据场景更新位置
    update_entity_by_scenario(e, scenario);
    
    // 更新AOI系统中的位置
    aoi_update(space, e->id, e->mode, e->pos);
}

// 检查碰撞
void check_collisions() {
    const float COLLISION_DISTANCE = 5.0f;
    
    for (int i = 0; i < test_entity_count; i++) {
        if (!test_entities[i].active) continue;
        
        for (int j = i + 1; j < test_entity_count; j++) {
            if (!test_entities[j].active) continue;
            
            float dx = test_entities[i].pos[0] - test_entities[j].pos[0];
            float dy = test_entities[i].pos[1] - test_entities[j].pos[1];
            float dist_sqr = dx*dx + dy*dy;
            
            if (dist_sqr < COLLISION_DISTANCE * COLLISION_DISTANCE) {
                collision_count++;
                
                // 交换速度实现简单的碰撞反弹（可选）
                float tmp_vx = test_entities[i].v[0];
                float tmp_vy = test_entities[i].v[1];
                
                test_entities[i].v[0] = test_entities[j].v[0];
                test_entities[i].v[1] = test_entities[j].v[1];
                
                test_entities[j].v[0] = tmp_vx;
                test_entities[j].v[1] = tmp_vy;
            }
        }
    }
}

// 设置随机移动场景
void setup_random_scenario(struct aoi_space *space) {
    test_entity_count = 0;
    
    // 创建一些随机移动的实体
    for (int i = 0; i < 20; i++) {
        char *mode = (i % 3 == 0) ? "w" : ((i % 3 == 1) ? "m" : "wm");
        create_test_entity(
            i + 1,
            rand() % 800,
            rand() % 600,
            (rand() % 20 - 10) / 5.0f,
            (rand() % 20 - 10) / 5.0f,
            mode
        );
    }
}

// 设置圆形轨迹场景
void setup_circle_scenario(struct aoi_space *space) {
    test_entity_count = 0;
    
    // 创建沿圆形轨迹移动的实体
    for (int i = 0; i < 12; i++) {
        char *mode = (i % 3 == 0) ? "w" : ((i % 3 == 1) ? "m" : "wm");
        float angle = (i * 30) * M_PI / 180.0f;
        float radius = 100.0f + (i / 3) * 50.0f;
        
        create_test_entity(
            i + 1,
            400 + radius * cos(angle),
            300 + radius * sin(angle),
            0, 0,
            mode
        );
        
        test_entities[i].params.radius = radius;
        test_entities[i].params.angle = angle;
        test_entities[i].params.speed = 0.01f + (i % 4) * 0.005f;
    }
}

// 设置交叉路径场景
void setup_crossing_scenario(struct aoi_space *space) {
    test_entity_count = 0;
    
    // 创建沿直线交叉路径移动的实体
    for (int i = 0; i < 10; i++) {
        char *mode = (i % 2 == 0) ? "w" : "m";
        float speed = 1.0f + (rand() % 10) / 10.0f;
        
        if (i < 5) {
            // 水平移动
            create_test_entity(
                i + 1,
                0,
                100 + i * 80,
                speed,
                0,
                mode
            );
        } else {
            // 垂直移动
            create_test_entity(
                i + 1,
                100 + (i - 5) * 100,
                0,
                0,
                speed,
                "wm"
            );
        }
    }
}

// 设置网格移动场景
void setup_grid_scenario(struct aoi_space *space) {
    test_entity_count = 0;
    
    // 创建在网格上移动的实体
    for (int i = 0; i < 16; i++) {
        int row = i / 4;
        int col = i % 4;
        char *mode = (i % 3 == 0) ? "w" : ((i % 3 == 1) ? "m" : "wm");
        
        int dir = rand() % 4;
        float vx = 0, vy = 0;
        
        switch (dir) {
            case 0: vx = 1.0f; break;
            case 1: vx = -1.0f; break;
            case 2: vy = 1.0f; break;
            case 3: vy = -1.0f; break;
        }
        
        create_test_entity(
            i + 1,
            100 + col * 200,
            100 + row * 150,
            vx,
            vy,
            mode
        );
    }
}

// 设置跟随行为场景
void setup_follow_scenario(struct aoi_space *space) {
    test_entity_count = 0;
    
    // 首先创建几个目标
    for (int i = 0; i < 3; i++) {
        create_test_entity(
            i + 1,
            200 + i * 200,
            300,
            (rand() % 20 - 10) / 10.0f,
            (rand() % 20 - 10) / 10.0f,
            "m"
        );
    }
    
    // 然后创建跟随者
    for (int i = 0; i < 12; i++) {
        int target_idx = i % 3;
        
        create_test_entity(
            i + 4,
            rand() % 800,
            rand() % 600,
            0, 0,
            "w"
        );
        
        test_entities[i + 3].params.target_id = target_idx + 1;
    }
}

// 设置测试场景
void setup_test_scenario(struct aoi_space *space, TestScenario scenario) {
    switch (scenario) {
        case SCENARIO_RANDOM:
            setup_random_scenario(space);
            break;
        case SCENARIO_CIRCLE:
            setup_circle_scenario(space);
            break;
        case SCENARIO_CROSSING:
            setup_crossing_scenario(space);
            break;
        case SCENARIO_GRID:
            setup_grid_scenario(space);
            break;
        case SCENARIO_FOLLOW:
            setup_follow_scenario(space);
            break;
        default:
            setup_random_scenario(space);
            break;
    }
}

// 运行测试场景
void run_test_scenario(struct aoi_space *space, TestScenario scenario, int frames) {
    // 设置场景
    setup_test_scenario(space, scenario);
    
    // 重置统计
    interaction_count_size = 0;
    collision_count = 0;
    
    // 运行指定帧数
    for (int frame = 0; frame < frames; frame++) {
        // 更新所有实体
        for (int i = 0; i < test_entity_count; i++) {
            update_test_entity(space, i, scenario);
        }
        
        // 处理AOI消息
        aoi_message(space, test_callback, NULL);
        
        // 检查碰撞
        check_collisions();
    }
    
    // 输出统计信息
    printf("========== 场景 %d 测试结果 ==========\n", scenario);
    printf("运行帧数: %d\n", frames);
    printf("实体数量: %d\n", test_entity_count);
    printf("交互记录数: %d\n", interaction_count_size);
    printf("碰撞次数: %d\n", collision_count);
    
    // 输出交互频率前10的实体对
    printf("\n交互频率最高的实体对:\n");
    
    // 简单排序
    for (int i = 0; i < interaction_count_size - 1; i++) {
        for (int j = i + 1; j < interaction_count_size; j++) {
            if (interaction_counts[j].count > interaction_counts[i].count) {
                InteractionCount temp = interaction_counts[i];
                interaction_counts[i] = interaction_counts[j];
                interaction_counts[j] = temp;
            }
        }
    }
    
    // 打印前10个或更少
    int count_to_print = (interaction_count_size < 10) ? interaction_count_size : 10;
    for (int i = 0; i < count_to_print; i++) {
        printf("  观察者 %u -> 标记者 %u: %d次交互\n", 
               interaction_counts[i].watcher, 
               interaction_counts[i].marker,
               interaction_counts[i].count);
    }
    
    printf("\n");
}

// 主测试函数
void run_all_tests(struct aoi_space *space, int frames_per_scenario) {
    for (int scenario = 0; scenario < SCENARIO_COUNT; scenario++) {
        run_test_scenario(space, scenario, frames_per_scenario);
    }
}

// 测试主函数
int main(int argc, char *argv[]) {
    // 初始化随机数生成器
    srand(time(NULL));
    
    // 创建AOI空间
    struct aoi_space *space = aoi_new();
    if (!space) {
        fprintf(stderr, "无法创建AOI空间\n");
        return 1;
    }
    
    // 运行所有测试场景
    run_all_tests(space, 1000);
    
    // 清理资源
    aoi_release(space);
    
    return 0;
} 