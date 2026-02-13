/*
 * LGX Runtime Core - Game Simulation Demo
 * 
 * This demo simulates a realistic game workload to showcase LGX Runtime's
 * capabilities including frame-based allocation, performance monitoring,
 * and hardware adaptation.
 * 
 * Features demonstrated:
 * - Frame arena allocation for per-frame data
 * - Persistent allocation for long-lived objects
 * - Level-scoped allocation for scene data
 * - Performance metrics collection
 * - Hardware tier detection
 * - Real-time frame timing
 */

#include <lgx_runtime.h>
#include <lgx/lgx_runtime_internal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

// Demo configuration
#define DEMO_DURATION_SECONDS 10
#define TARGET_FPS 60
#define FRAME_TIME_US (1000000 / TARGET_FPS)

// Simulated game objects
#define MAX_ENTITIES 1000
#define MAX_PARTICLES 5000
#define ENTITY_SIZE 256
#define PARTICLE_SIZE 64

// Performance tracking
typedef struct {
    uint64_t frame_number;
    double frame_time_ms;
    double allocation_time_us;
    size_t frame_allocations;
    size_t persistent_allocations;
    size_t total_memory_used;
    uint64_t cache_hits;
    uint64_t cache_misses;
} frame_stats_t;

// Demo state
typedef struct {
    lgx_runtime_config_t* config;
    frame_stats_t* frame_history;
    size_t frame_count;
    size_t max_frames;
    
    // Simulated game state
    void** entities;
    size_t entity_count;
    void** particles;
    size_t particle_count;
    void* level_data;
} demo_state_t;

// Utility functions
static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static double get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000.0 + tv.tv_usec;
}

// Initialize demo state
static demo_state_t* demo_init(void) {
    demo_state_t* state = calloc(1, sizeof(demo_state_t));
    if (!state) {
        fprintf(stderr, "Failed to allocate demo state\n");
        return NULL;
    }
    
    state->max_frames = DEMO_DURATION_SECONDS * TARGET_FPS;
    state->frame_history = calloc(state->max_frames, sizeof(frame_stats_t));
    if (!state->frame_history) {
        fprintf(stderr, "Failed to allocate frame history\n");
        free(state);
        return NULL;
    }
    
    state->entities = calloc(MAX_ENTITIES, sizeof(void*));
    state->particles = calloc(MAX_PARTICLES, sizeof(void*));
    
    return state;
}

// Cleanup demo state
static void demo_cleanup(demo_state_t* state) {
    if (!state) return;
    
    free(state->frame_history);
    free(state->entities);
    free(state->particles);
    free(state);
}

// Initialize LGX Runtime
static int init_runtime(demo_state_t* state) {
    printf("Initializing LGX Runtime...\n");
    
    // Create configuration
    state->config = lgx_config_create();
    if (!state->config) {
        fprintf(stderr, "Failed to create runtime config\n");
        return -1;
    }
    
    // Configure runtime
    lgx_config_set_memory_pool_size(state->config, 512 * 1024 * 1024); // 512MB
    lgx_config_set_flags(state->config, 0);
    
    // Initialize runtime
    lgx_result_t result = lgx_runtime_init(state->config);
    if (result != LGX_SUCCESS) {
        fprintf(stderr, "Runtime initialization failed: %s\n", 
                lgx_result_to_string(result));
        lgx_config_destroy(state->config);
        return -1;
    }
    
    // Get hardware status
    lgx_hardware_status_t hw_status = lgx_runtime_get_hardware_status();
    printf("Hardware Tier: ");
    switch (hw_status.achieved_tier) {
        case 0: printf("OPTIMAL\n"); break;
        case 1: printf("COMPATIBLE\n"); break;
        case 2: printf("DEGRADED\n"); break;
        default: printf("UNKNOWN\n"); break;
    }
    
    // Get version
    lgx_version_t version = lgx_runtime_get_version();
    printf("LGX Runtime Version: %d.%d.%d\n", 
           version.major, version.minor, version.patch);
    
    printf("Runtime initialized successfully\n\n");
    return 0;
}

// Simulate entity updates (persistent allocations)
static void update_entities(demo_state_t* state, frame_stats_t* stats) {
    // Spawn new entities occasionally
    if (state->entity_count < MAX_ENTITIES && (rand() % 100) < 5) {
        void* entity = lgx_alloc_persistent(ENTITY_SIZE);
        if (entity) {
            state->entities[state->entity_count++] = entity;
            stats->persistent_allocations++;
        }
    }
    
    // Despawn entities occasionally
    if (state->entity_count > 0 && (rand() % 100) < 3) {
        size_t idx = rand() % state->entity_count;
        lgx_free(state->entities[idx]);
        state->entities[idx] = state->entities[--state->entity_count];
    }
}

// Simulate particle system (frame allocations)
static void update_particles(demo_state_t* state, frame_stats_t* stats) {
    // Spawn particles every frame
    size_t new_particles = 50 + (rand() % 100);
    
    for (size_t i = 0; i < new_particles && state->particle_count < MAX_PARTICLES; i++) {
        void* particle = lgx_alloc_frame(PARTICLE_SIZE);
        if (particle) {
            state->particles[state->particle_count++] = particle;
            stats->frame_allocations++;
        }
    }
}

// Simulate frame rendering
static void render_frame(demo_state_t* state __attribute__((unused)), frame_stats_t* stats) {
    // Allocate temporary render data (frame allocation)
    size_t render_data_size = 4096 + (rand() % 4096);
    void* render_data = lgx_alloc_frame(render_data_size);
    if (render_data) {
        stats->frame_allocations++;
        // Simulate using the data
        memset(render_data, 0, render_data_size);
    }
}

// Run single frame
static void run_frame(demo_state_t* state) {
    frame_stats_t* stats = &state->frame_history[state->frame_count];
    stats->frame_number = state->frame_count;
    
    double frame_start = get_time_ms();
    double alloc_start = get_time_us();
    
    // Reset particle count (frame allocations will be reset)
    state->particle_count = 0;
    
    // Update game systems
    update_entities(state, stats);
    update_particles(state, stats);
    render_frame(state, stats);
    
    double alloc_end = get_time_us();
    stats->allocation_time_us = alloc_end - alloc_start;
    
    // Get memory statistics
    lgx_memory_stats_t mem_stats;
    if (lgx_memory_stats(&mem_stats) == LGX_SUCCESS) {
        stats->total_memory_used = mem_stats.total_allocated;
    }
    
    // Get performance counters
    stats->cache_hits = lgx_get_counter(LGX_COUNTER_CACHE_HITS);
    stats->cache_misses = lgx_get_counter(LGX_COUNTER_CACHE_MISSES);
    
    // Reset frame arena for next frame
    lgx_frame_reset();
    
    double frame_end = get_time_ms();
    stats->frame_time_ms = frame_end - frame_start;
    
    state->frame_count++;
}

// Print progress bar
static void print_progress(size_t current, size_t total) {
    const int bar_width = 50;
    float progress = (float)current / total;
    int pos = bar_width * progress;
    
    printf("\r[");
    for (int i = 0; i < bar_width; i++) {
        if (i < pos) printf("=");
        else if (i == pos) printf(">");
        else printf(" ");
    }
    printf("] %3d%% Frame %zu/%zu", (int)(progress * 100), current, total);
    fflush(stdout);
}

// Calculate statistics
static void calculate_stats(frame_stats_t* frames, size_t count,
                           double* avg_frame_time, double* min_frame_time, 
                           double* max_frame_time, double* p99_frame_time) {
    if (count == 0) return;
    
    *min_frame_time = frames[0].frame_time_ms;
    *max_frame_time = frames[0].frame_time_ms;
    double sum = 0;
    
    // Calculate min, max, avg
    for (size_t i = 0; i < count; i++) {
        double ft = frames[i].frame_time_ms;
        sum += ft;
        if (ft < *min_frame_time) *min_frame_time = ft;
        if (ft > *max_frame_time) *max_frame_time = ft;
    }
    *avg_frame_time = sum / count;
    
    // Calculate P99 (sort and find 99th percentile)
    double* sorted = malloc(count * sizeof(double));
    for (size_t i = 0; i < count; i++) {
        sorted[i] = frames[i].frame_time_ms;
    }
    
    // Simple bubble sort (good enough for demo)
    for (size_t i = 0; i < count - 1; i++) {
        for (size_t j = 0; j < count - i - 1; j++) {
            if (sorted[j] > sorted[j + 1]) {
                double temp = sorted[j];
                sorted[j] = sorted[j + 1];
                sorted[j + 1] = temp;
            }
        }
    }
    
    size_t p99_idx = (size_t)(count * 0.99);
    *p99_frame_time = sorted[p99_idx];
    
    free(sorted);
}

// Print results
static void print_results(demo_state_t* state) {
    printf("\n\n");
    printf("================================================================================\n");
    printf("                        LGX RUNTIME DEMO RESULTS                               \n");
    printf("================================================================================\n\n");
    
    // Calculate statistics
    double avg_frame_time = 0, min_frame_time = 0, max_frame_time = 0, p99_frame_time = 0;
    calculate_stats(state->frame_history, state->frame_count,
                   &avg_frame_time, &min_frame_time, &max_frame_time, &p99_frame_time);
    
    // Frame timing
    printf("Frame Timing:\n");
    printf("  Total Frames:        %zu\n", state->frame_count);
    printf("  Average Frame Time:  %.3f ms (%.1f FPS)\n", 
           avg_frame_time, 1000.0 / avg_frame_time);
    printf("  Min Frame Time:      %.3f ms\n", min_frame_time);
    printf("  Max Frame Time:      %.3f ms\n", max_frame_time);
    printf("  P99 Frame Time:      %.3f ms\n", p99_frame_time);
    printf("\n");
    
    // Allocation statistics
    size_t total_frame_allocs = 0;
    size_t total_persistent_allocs = 0;
    double total_alloc_time = 0;
    
    for (size_t i = 0; i < state->frame_count; i++) {
        total_frame_allocs += state->frame_history[i].frame_allocations;
        total_persistent_allocs += state->frame_history[i].persistent_allocations;
        total_alloc_time += state->frame_history[i].allocation_time_us;
    }
    
    printf("Allocation Statistics:\n");
    printf("  Total Frame Allocations:      %zu\n", total_frame_allocs);
    printf("  Total Persistent Allocations: %zu\n", total_persistent_allocs);
    printf("  Avg Allocations per Frame:    %.1f\n", 
           (double)total_frame_allocs / state->frame_count);
    printf("  Avg Allocation Time:          %.2f μs\n", 
           total_alloc_time / state->frame_count);
    printf("\n");
    
    // Cache statistics
    frame_stats_t* last_frame = &state->frame_history[state->frame_count - 1];
    uint64_t total_accesses = last_frame->cache_hits + last_frame->cache_misses;
    double hit_rate = total_accesses > 0 ? 
        (double)last_frame->cache_hits / total_accesses * 100.0 : 0.0;
    
    printf("Cache Performance:\n");
    printf("  Cache Hits:          %lu\n", last_frame->cache_hits);
    printf("  Cache Misses:        %lu\n", last_frame->cache_misses);
    printf("  Hit Rate:            %.2f%%\n", hit_rate);
    printf("\n");
    
    // Memory usage
    printf("Memory Usage:\n");
    printf("  Peak Memory:         %.2f MB\n", 
           last_frame->total_memory_used / (1024.0 * 1024.0));
    printf("  Active Entities:     %zu\n", state->entity_count);
    printf("\n");
    
    printf("================================================================================\n");
}

// Save results to CSV
static void save_results_csv(demo_state_t* state, const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Failed to open %s for writing\n", filename);
        return;
    }
    
    fprintf(fp, "frame,frame_time_ms,allocation_time_us,frame_allocations,"
                "persistent_allocations,total_memory_mb,cache_hits,cache_misses\n");
    
    for (size_t i = 0; i < state->frame_count; i++) {
        frame_stats_t* stats = &state->frame_history[i];
        fprintf(fp, "%lu,%.3f,%.2f,%zu,%zu,%.2f,%lu,%lu\n",
                stats->frame_number,
                stats->frame_time_ms,
                stats->allocation_time_us,
                stats->frame_allocations,
                stats->persistent_allocations,
                stats->total_memory_used / (1024.0 * 1024.0),
                stats->cache_hits,
                stats->cache_misses);
    }
    
    fclose(fp);
    printf("Results saved to %s\n", filename);
}

// Main demo
int main(int argc __attribute__((unused)), char** argv __attribute__((unused))) {
    printf("================================================================================\n");
    printf("                    LGX RUNTIME CORE - GAME SIMULATION DEMO                    \n");
    printf("================================================================================\n\n");
    
    printf("This demo simulates a realistic game workload to showcase LGX Runtime's\n");
    printf("performance characteristics including:\n");
    printf("  - Frame-based memory allocation\n");
    printf("  - Persistent object management\n");
    printf("  - Real-time performance monitoring\n");
    printf("  - Hardware adaptation\n\n");
    
    printf("Demo Configuration:\n");
    printf("  Duration:            %d seconds\n", DEMO_DURATION_SECONDS);
    printf("  Target FPS:          %d\n", TARGET_FPS);
    printf("  Max Entities:        %d\n", MAX_ENTITIES);
    printf("  Max Particles:       %d\n\n", MAX_PARTICLES);
    
    // Initialize demo
    demo_state_t* state = demo_init();
    if (!state) {
        return 1;
    }
    
    // Initialize runtime
    if (init_runtime(state) != 0) {
        demo_cleanup(state);
        return 1;
    }
    
    // Run demo
    printf("Running simulation...\n");
    srand(time(NULL));
    
    double start_time = get_time_ms();
    
    while (state->frame_count < state->max_frames) {
        run_frame(state);
        print_progress(state->frame_count, state->max_frames);
        
        // Frame pacing
        usleep(FRAME_TIME_US / 2); // Sleep for half frame time to simulate work
    }
    
    double end_time = get_time_ms();
    double total_time = (end_time - start_time) / 1000.0;
    
    printf("\n\nSimulation completed in %.2f seconds\n", total_time);
    
    // Print results
    print_results(state);
    
    // Save results
    save_results_csv(state, "demo_results.csv");
    
    // Cleanup
    lgx_runtime_shutdown();
    lgx_config_destroy(state->config);
    demo_cleanup(state);
    
    printf("\nDemo completed successfully!\n");
    return 0;
}
