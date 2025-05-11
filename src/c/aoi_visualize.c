#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <SDL.h>  
#include <time.h> 
#include "../../aoi/aoi.h"

#ifdef _WIN32
#define SDL_MAIN_HANDLED
#endif

#ifdef _WIN32
#include <windows.h>
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return main(__argc, __argv);
}
#endif

#define WINDOW_WIDTH 1000
#define WINDOW_HEIGHT 800
#define ENTITY_SIZE 5
#define MAX_ENTITIES 1000
#define AOI_RADIUS 30.0f 

// 全局日志文件
FILE *logFile = NULL;

// 日志函数
void logMessage(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    // 输出到控制台
    vprintf(format, args);
    
    // 输出到日志文件
    if (logFile) {
        vfprintf(logFile, format, args);
        fflush(logFile);
    }
    
    va_end(args);
}

// 实体类型和状态
typedef struct {
    uint32_t id;
    float pos[3];
    float v[3];
    char mode[4];
    SDL_Color color;
    bool active;
} Entity;

// 实体交互记录
typedef struct {
    uint32_t watcher;
    uint32_t marker;
    int frames_remaining;
} Interaction;

#define MAX_INTERACTIONS 1000
Interaction interactions[MAX_INTERACTIONS];
int interaction_count = 0;

// 实体数组
Entity entities[MAX_ENTITIES];
int entity_count = 0;

static void aoi_visualize_callback(void *ud, uint32_t watcher, uint32_t marker) {
    for (int i = 0; i < interaction_count; i++) {
        if (interactions[i].watcher == watcher && interactions[i].marker == marker) {
            interactions[i].frames_remaining = 60; // 刷新显示时间
            logMessage("刷新交互: %u -> %u\n", watcher, marker);
            return;
        }
    }
    if (interaction_count < MAX_INTERACTIONS) {
        interactions[interaction_count].watcher = watcher;
        interactions[interaction_count].marker = marker;
        interactions[interaction_count].frames_remaining = 60; // 显示1秒
        interaction_count++;
        logMessage("新增交互: %u -> %u\n", watcher, marker);
    }
}

void create_entity(uint32_t id, float x, float y, float vx, float vy, const char *mode) {
    if (entity_count < MAX_ENTITIES) {
        Entity *e = &entities[entity_count++];
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
        
        // 根据模式设置颜色
        if (strcmp(mode, "w") == 0) {
            // 观察者为蓝色
            e->color = (SDL_Color){0, 0, 255, 255};
        } else if (strcmp(mode, "m") == 0) {
            // 标记者为红色
            e->color = (SDL_Color){255, 0, 0, 255};
        } else if (strcmp(mode, "wm") == 0) {
            // 既是观察者又是标记者为紫色
            e->color = (SDL_Color){255, 0, 255, 255};
        } else {
            // 默认为绿色
            e->color = (SDL_Color){0, 255, 0, 255};
        }
    }
}

void update_entity(struct aoi_space *space, int index) {
    Entity *e = &entities[index];
    if (!e->active) return;
    
    for (int i = 0; i < 3; i++) {
        e->pos[i] += e->v[i];
        
        if (e->pos[i] < 0) {
            e->pos[i] = 0;
            e->v[i] = -e->v[i];
        } else if (e->pos[i] > (i == 0 ? WINDOW_WIDTH : WINDOW_HEIGHT)) {
            e->pos[i] = (i == 0 ? WINDOW_WIDTH : WINDOW_HEIGHT);
            e->v[i] = -e->v[i];
        }
    }
    
    aoi_update(space, e->id, e->mode, e->pos);
}

void draw_entity(SDL_Renderer *renderer, int index) {
    Entity *e = &entities[index];
    if (!e->active) return;
    
    SDL_SetRenderDrawColor(renderer, e->color.r, e->color.g, e->color.b, e->color.a);
    
    SDL_Rect rect = {
        (int)(e->pos[0] - ENTITY_SIZE/2),
        (int)(e->pos[1] - ENTITY_SIZE/2),
        ENTITY_SIZE,
        ENTITY_SIZE
    };
    SDL_RenderFillRect(renderer, &rect);
    
    if (strcmp(e->mode, "w") == 0 || strcmp(e->mode, "wm") == 0) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 200);
        int radius = (int)AOI_RADIUS;
        int x0 = (int)e->pos[0];
        int y0 = (int)e->pos[1];
        int x = radius;
        int y = 0;
        int err = 0;
        
        while (x >= y) {
            SDL_RenderDrawPoint(renderer, x0 + x, y0 + y);
            SDL_RenderDrawPoint(renderer, x0 + y, y0 + x);
            SDL_RenderDrawPoint(renderer, x0 - y, y0 + x);
            SDL_RenderDrawPoint(renderer, x0 - x, y0 + y);
            SDL_RenderDrawPoint(renderer, x0 - x, y0 - y);
            SDL_RenderDrawPoint(renderer, x0 - y, y0 - x);
            SDL_RenderDrawPoint(renderer, x0 + y, y0 - x);
            SDL_RenderDrawPoint(renderer, x0 + x, y0 - y);
            
            if (err <= 0) {
                y += 1;
                err += 2*y + 1;
            }
            
            if (err > 0) {
                x -= 1;
                err -= 2*x + 1;
            }
        }
    }
}

void draw_interactions(SDL_Renderer *renderer) {
    int remaining = 0;
    
    for (int i = 0; i < interaction_count; i++) {
        if (interactions[i].frames_remaining > 0) {
            uint32_t watcher_id = interactions[i].watcher;
            uint32_t marker_id = interactions[i].marker;
            
            Entity *watcher = NULL;
            Entity *marker = NULL;
            
            for (int j = 0; j < entity_count; j++) {
                if (entities[j].id == watcher_id) watcher = &entities[j];
                if (entities[j].id == marker_id) marker = &entities[j];
                
                if (watcher && marker) break;
            }
            
            if (watcher && marker && watcher->active && marker->active) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
                SDL_RenderDrawLine(renderer, 
                    (int)watcher->pos[0], 
                    (int)watcher->pos[1], 
                    (int)marker->pos[0], 
                    (int)marker->pos[1]);
            }
            
            interactions[i].frames_remaining--;
            
            if (interactions[i].frames_remaining > 0) {
                interactions[remaining++] = interactions[i];
            }
        }
    }
    
    interaction_count = remaining;
}

int main(int argc, char *argv[]) {
    srand((unsigned int)time(NULL));
    
    #ifdef _WIN32
    SDL_SetMainReady();
    #endif
    
    logFile = fopen("aoi_visualize_log.txt", "w");
    if (!logFile) {
        printf("无法创建日志文件\n");
    }
    
    logMessage("程序开始运行\n");
    
    logMessage("系统信息: %s\n", SDL_GetPlatform());
    logMessage("SDL启动前错误状态: %s\n", SDL_GetError());
    
    SDL_ClearError();
    
    SDL_version compiled;
    SDL_version linked;
    SDL_VERSION(&compiled);
    SDL_GetVersion(&linked);
    logMessage("SDL编译版本: %d.%d.%d\n", compiled.major, compiled.minor, compiled.patch);
    logMessage("SDL运行时版本: %d.%d.%d\n", linked.major, linked.minor, linked.patch);
    
    // 初始化SDL
    logMessage("尝试初始化SDL...\n");
    
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        logMessage("SDL初始化失败: %s\n", SDL_GetError());
        if (logFile) fclose(logFile);
        return 1;
    }
    
    logMessage("SDL初始化成功，创建窗口...\n");
    
    // 输出显示驱动信息
    logMessage("可用的显示驱动数量: %d\n", SDL_GetNumVideoDrivers());
    for (int i = 0; i < SDL_GetNumVideoDrivers(); i++) {
        logMessage("显示驱动 %d: %s\n", i, SDL_GetVideoDriver(i));
    }
    logMessage("当前使用的显示驱动: %s\n", SDL_GetCurrentVideoDriver());
    
    // 创建窗口 (SDL2 API)
    SDL_Window *window = SDL_CreateWindow(
        "AOI可视化测试",                // 窗口标题
        SDL_WINDOWPOS_UNDEFINED,       // x位置
        SDL_WINDOWPOS_UNDEFINED,       // y位置
        WINDOW_WIDTH,                  // 宽度
        WINDOW_HEIGHT,                 // 高度
        SDL_WINDOW_RESIZABLE           // 使用可调整大小的标志
    );
    
    if (!window) {
        logMessage("窗口创建失败: %s\n", SDL_GetError());
        SDL_Quit();
        if (logFile) fclose(logFile);
        return 1;
    }
    
    logMessage("窗口创建成功，创建渲染器...\n");
    
    // 创建渲染器 (SDL2 API)
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    
    if (!renderer) {
        logMessage("渲染器创建失败: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        if (logFile) fclose(logFile);
        return 1;
    }
    
    logMessage("渲染器创建成功\n");
    
    // 初始化AOI空间
    struct aoi_space *space = aoi_new();
    if (!space) {
        logMessage("AOI空间创建失败\n");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        if (logFile) fclose(logFile);
        return 1;
    }
    logMessage("AOI空间创建成功\n");
    
    // 创建测试实体，确保实体之间的距离合适
    float spacing = AOI_RADIUS * 2.5f;  // 确保实体之间有足够的间距
    // 创建几个观察者 - 位置缩放以适应新的窗口
    create_entity(1, 100, 100, 0.3, 0.2, "w");
    create_entity(2, 200, 200, -0.2, 0.3, "w");
    create_entity(3, 300, 150, 0.1, -0.3, "w");
    
    // 创建几个标记者 - 放在观察者周围
    create_entity(4, 100 + AOI_RADIUS * 0.8f, 100, 0.2, 0.1, "m");  // 靠近观察者1
    create_entity(5, 200, 200 + AOI_RADIUS * 0.8f, -0.1, 0.2, "m"); // 靠近观察者2
    create_entity(6, 300 + AOI_RADIUS * 0.8f, 150, 0.2, -0.2, "m"); // 靠近观察者3
    
    // 创建几个既是观察者又是标记者的实体
    create_entity(7, 150, 250, -0.2, -0.1, "wm");
    create_entity(8, 350, 150, 0.1, 0.2, "wm");
    
    for (int i = 0; i < entity_count; i++) {
        aoi_update(space, entities[i].id, entities[i].mode, entities[i].pos);
    }
    
    bool running = true;
    SDL_Event event;
    Uint32 lastFrame = SDL_GetTicks();
    const int FPS = 60;
    const int frameDelay = 1000 / FPS;
    
    interaction_count = 0;
    
    for (int i = 0; i < entity_count; i++) {
        entities[i].v[0] *= 0.5f;
        entities[i].v[1] *= 0.5f;
    }
    
    while (running) {
        // 处理事件
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                }
                else if (event.key.keysym.sym == SDLK_1 || 
                         event.key.keysym.sym == SDLK_2 || 
                         event.key.keysym.sym == SDLK_3) {
                    // 按1、2、3键新增特定模式的实体
                    uint32_t new_id = entity_count + 10; // 避免ID冲突
                    float x = rand() % WINDOW_WIDTH;
                    float y = rand() % WINDOW_HEIGHT;
                    float vx = (rand() % 10 - 5) / 20.0f; // 减小速度
                    float vy = (rand() % 10 - 5) / 20.0f; // 减小速度
                    
                    // 根据按键选择模式
                    const char* mode;
                    if (event.key.keysym.sym == SDLK_1) {
                        mode = "w";  // 按1创建观察者
                    } else if (event.key.keysym.sym == SDLK_2) {
                        mode = "m";  // 按2创建标记者
                    } else {
                        mode = "wm"; // 按3创建既是观察者又是标记者的实体
                    }
                    
                    create_entity(new_id, x, y, vx, vy, mode);
                    
                    // 将新实体加入到AOI空间
                    aoi_update(space, new_id, mode, entities[entity_count-1].pos);
                    
                    logMessage("创建了新实体 ID:%u, 模式:%s, 位置:(%.1f,%.1f)\n", 
                              new_id, mode, x, y);
                }
            }
        }
        
        // 计算帧率
        Uint32 frameStart = SDL_GetTicks();
        Uint32 frameTime = frameStart - lastFrame;
        lastFrame = frameStart;
        
        // 清屏
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        
        // 更新实体位置
        for (int i = 0; i < entity_count; i++) {
            update_entity(space, i);
        }
        
        // AOI处理
        // 每帧重置交互计数，确保显示当前帧的最新交互状态
        aoi_message(space, aoi_visualize_callback, NULL);
        
        // 绘制实体和交互，先绘制交互线条，后绘制实体
        draw_interactions(renderer);
        for (int i = 0; i < entity_count; i++) {
            draw_entity(renderer, i);
        }
        
        // 呈现画面
        SDL_RenderPresent(renderer);
        
        // 控制帧率
        Uint32 frameDuration = SDL_GetTicks() - frameStart;
        if (frameDelay > frameDuration) {
            SDL_Delay(frameDelay - frameDuration);
        }
    }
    
    // 清理资源
    logMessage("清理资源...\n");
    aoi_release(space);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    logMessage("程序正常结束\n");
    
    if (logFile) {
        fclose(logFile);
    }
    
    return 0;
} 