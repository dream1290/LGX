/**
 * Frame Arena Allocation Tagging Test
 * 
 * Tests the allocation tagging feature (Task 3.4.5.4.3) which allows
 * labeling allocations by subsystem for better debugging and profiling.
 * 
 * This simulates a real AAA game workload with multiple subsystems:
 * - Physics engine
 * - Rendering system
 * - Audio system
 * - AI system
 * - Networking
 */

#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simulate different subsystem allocations
static void simulate_physics_frame(void) {
    // Physics typically allocates collision data, rigid bodies, constraints
    void* collision_data = lgx_frame_alloc_tagged(4096, "physics");
    void* rigid_bodies = lgx_frame_alloc_tagged(8192, "physics");
    void* constraints = lgx_frame_alloc_tagged(2048, "physics");
    void* broadphase = lgx_frame_alloc_tagged(1024, "physics");
    
    (void)collision_data;
    (void)rigid_bodies;
    (void)constraints;
    (void)broadphase;
}

static void simulate_rendering_frame(void) {
    // Rendering allocates command buffers, draw calls, uniform data
    void* cmd_buffer = lgx_frame_alloc_tagged(16384, "rendering");
    void* draw_calls = lgx_frame_alloc_tagged(32768, "rendering");
    void* uniforms = lgx_frame_alloc_tagged(8192, "rendering");
    void* vertex_data = lgx_frame_alloc_tagged(65536, "rendering");
    
    (void)cmd_buffer;
    (void)draw_calls;
    (void)uniforms;
    (void)vertex_data;
}

static void simulate_audio_frame(void) {
    // Audio allocates mix buffers, voice data, effects
    void* mix_buffer = lgx_frame_alloc_tagged(4096, "audio");
    void* voice_data = lgx_frame_alloc_tagged(2048, "audio");
    void* effects = lgx_frame_alloc_tagged(1024, "audio");
    
    (void)mix_buffer;
    (void)voice_data;
    (void)effects;
}

static void simulate_ai_frame(void) {
    // AI allocates pathfinding data, behavior trees, navigation
    void* pathfinding = lgx_frame_alloc_tagged(8192, "ai");
    void* behavior_trees = lgx_frame_alloc_tagged(4096, "ai");
    void* navigation = lgx_frame_alloc_tagged(2048, "ai");
    
    (void)pathfinding;
    (void)behavior_trees;
    (void)navigation;
}

static void simulate_networking_frame(void) {
    // Networking allocates packet buffers, serialization data
    void* packet_buffer = lgx_frame_alloc_tagged(2048, "networking");
    void* serialization = lgx_frame_alloc_tagged(1024, "networking");
    
    (void)packet_buffer;
    (void)serialization;
}

static void simulate_untagged_allocations(void) {
    // Some allocations without tags (should show as "untagged")
    void* misc1 = lgx_frame_alloc(512);
    void* misc2 = lgx_frame_alloc(1024);
    
    (void)misc1;
    (void)misc2;
}

int main(void) {
    printf("=== Frame Arena Allocation Tagging Test ===\n\n");
    
    // Initialize runtime
    lgx_runtime_config_t config = {
        .memory_pool_size = 256 * 1024 * 1024,  // 256MB
        .log_path = NULL,
        .flags = 0,
        .frame_arena_size = 64 * 1024 * 1024,   // 64MB
        .frame_arena_max_size = 256 * 1024 * 1024  // 256MB max
    };
    
    lgx_result_t result = lgx_runtime_init(&config);
    if (result != LGX_SUCCESS) {
        printf("❌ Failed to initialize runtime\n");
        return 1;
    }
    
    printf("✅ Runtime initialized\n\n");
    
    // Simulate 10 frames with tagged allocations
    printf("Simulating 10 frames with tagged allocations...\n");
    for (int frame = 0; frame < 10; frame++) {
        // Simulate subsystem allocations
        simulate_physics_frame();
        simulate_rendering_frame();
        simulate_audio_frame();
        simulate_ai_frame();
        simulate_networking_frame();
        simulate_untagged_allocations();
        
        // Reset for next frame
        lgx_frame_reset();
    }
    
    printf("✅ Completed 10 frames\n\n");
    
    // Dump statistics to see tag breakdown
    printf("=== Frame Arena Statistics with Tags ===\n\n");
    lgx_frame_arena_dump_stats(stdout);
    
    // Export to JSON
    printf("\n=== Exporting to JSON ===\n");
    result = lgx_frame_arena_dump("/tmp/frame_arena_tags.json");
    if (result == LGX_SUCCESS) {
        printf("✅ Exported allocation map with tags to /tmp/frame_arena_tags.json\n");
        printf("   You can visualize this with: python3 scripts/visualize_frame_arena.py /tmp/frame_arena_tags.json\n");
    } else {
        printf("❌ Failed to export allocation map\n");
    }
    
    // Verify tag tracking
    printf("\n=== Verification ===\n");
    printf("Expected tags:\n");
    printf("  - physics (should have ~15KB per frame)\n");
    printf("  - rendering (should have ~122KB per frame)\n");
    printf("  - audio (should have ~7KB per frame)\n");
    printf("  - ai (should have ~14KB per frame)\n");
    printf("  - networking (should have ~3KB per frame)\n");
    printf("  - untagged (should have ~1.5KB per frame)\n");
    printf("\nCheck the statistics above to verify tag tracking is working correctly.\n");
    
    // Cleanup
    lgx_runtime_shutdown();
    printf("\n✅ Test completed successfully\n");
    
    return 0;
}
