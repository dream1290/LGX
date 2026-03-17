/**
 * LGX Graphics Module v1.2 — Basic Tests
 * Tests device creation, resource management, shaders, pipelines, and stats.
 * Runs headless (no window required).
 */

#include "lgx_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    printf("  [TEST] %-50s", name); \
    tests_run++; \
} while(0)

#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

/* ═══════════════════════════════════════════════════════════════════════════
 * Test: Device Creation
 * ═══════════════════════════════════════════════════════════════════════════ */

static lgx_gfx_device_t* test_device_create(void) {
    TEST("device_create (default config)");
    lgx_gfx_device_config_t config = {
        .struct_size = sizeof(lgx_gfx_device_config_t),
        .app_name = "LGX Graphics Test",
        .app_version = 1,
        .gpu_preference = LGX_GFX_GPU_PREFER_ANY,
        .enable_validation = false,
        .enable_debug_markers = false,
    };

    lgx_gfx_device_t* dev = lgx_gfx_device_create(&config);
    if (dev) {
        PASS();
    } else {
        FAIL("device creation returned NULL");
    }
    return dev;
}

static void test_device_null_config(void) {
    TEST("device_create (NULL config)");
    lgx_gfx_device_t* dev = lgx_gfx_device_create(NULL);
    if (dev) {
        PASS();
        lgx_gfx_device_destroy(dev);
    } else {
        FAIL("device creation with NULL config returned NULL");
    }
}

static void test_device_destroy_null(void) {
    TEST("device_destroy (NULL)");
    lgx_gfx_device_destroy(NULL);  /* Should not crash */
    PASS();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test: GPU Info
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_gpu_info(lgx_gfx_device_t* dev) {
    TEST("device_get_gpu_info");
    lgx_gfx_gpu_info_t info;
    lgx_result_t r = lgx_gfx_device_get_gpu_info(dev, &info);
    if (r == LGX_SUCCESS && strlen(info.gpu_name) > 0 && info.vulkan_api_version > 0) {
        printf("PASS (%s, Vulkan %u.%u)\n",
               info.gpu_name,
               (info.vulkan_api_version >> 22) & 0x7FU,
               (info.vulkan_api_version >> 12) & 0x3FFU);
        tests_passed++;
    } else {
        FAIL("bad gpu info");
    }
}

static void test_gpu_info_null(void) {
    TEST("device_get_gpu_info (NULL params)");
    lgx_gfx_gpu_info_t info;
    lgx_result_t r1 = lgx_gfx_device_get_gpu_info(NULL, &info);
    lgx_result_t r2 = lgx_gfx_device_get_gpu_info(NULL, NULL);
    if (r1 != LGX_SUCCESS && r2 != LGX_SUCCESS) {
        PASS();
    } else {
        FAIL("should return error for NULL params");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test: Buffers
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_buffer_create_destroy(lgx_gfx_device_t* dev) {
    TEST("buffer_create/destroy (vertex, host-visible)");
    lgx_gfx_buffer_t* buf = lgx_gfx_buffer_create(dev, 4096,
                                                     LGX_GFX_BUFFER_VERTEX, true);
    if (buf) {
        lgx_gfx_buffer_destroy(buf);
        PASS();
    } else {
        FAIL("buffer creation failed");
    }
}

static void test_buffer_map_upload(lgx_gfx_device_t* dev) {
    TEST("buffer_map/upload (host-visible)");
    lgx_gfx_buffer_t* buf = lgx_gfx_buffer_create(dev, 256,
                                                     LGX_GFX_BUFFER_VERTEX, true);
    if (!buf) { FAIL("buffer creation failed"); return; }

    float data[] = { 1.0f, 2.0f, 3.0f, 4.0f };
    lgx_result_t r = lgx_gfx_buffer_upload(buf, data, sizeof(data), 0);
    if (r == LGX_SUCCESS) {
        PASS();
    } else {
        FAIL("upload failed");
    }
    lgx_gfx_buffer_destroy(buf);
}

static void test_buffer_device_local(lgx_gfx_device_t* dev) {
    TEST("buffer_create (device-local)");
    lgx_gfx_buffer_t* buf = lgx_gfx_buffer_create(dev, 4096,
                                                     LGX_GFX_BUFFER_VERTEX, false);
    if (buf) {
        lgx_gfx_buffer_destroy(buf);
        PASS();
    } else {
        FAIL("device-local buffer creation failed");
    }
}

static void test_buffer_null_params(void) {
    TEST("buffer_create (NULL device)");
    lgx_gfx_buffer_t* buf = lgx_gfx_buffer_create(NULL, 4096, LGX_GFX_BUFFER_VERTEX, true);
    if (!buf) {
        PASS();
    } else {
        FAIL("should return NULL for NULL device");
        lgx_gfx_buffer_destroy(buf);
    }
}

static void test_buffer_destroy_null(void) {
    TEST("buffer_destroy (NULL)");
    lgx_gfx_buffer_destroy(NULL);
    PASS();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test: Images
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_image_create_destroy(lgx_gfx_device_t* dev) {
    TEST("image_create/destroy (RGBA8, sampled)");
    lgx_gfx_image_t* img = lgx_gfx_image_create(dev, 256, 256,
                                                   LGX_GFX_FORMAT_RGBA8_UNORM,
                                                   LGX_GFX_IMAGE_SAMPLED);
    if (img) {
        lgx_gfx_image_destroy(img);
        PASS();
    } else {
        FAIL("image creation failed");
    }
}

static void test_image_depth(lgx_gfx_device_t* dev) {
    TEST("image_create (depth D32)");
    lgx_gfx_image_t* img = lgx_gfx_image_create(dev, 1920, 1080,
                                                   LGX_GFX_FORMAT_D32_SFLOAT,
                                                   LGX_GFX_IMAGE_DEPTH_TARGET);
    if (img) {
        lgx_gfx_image_destroy(img);
        PASS();
    } else {
        FAIL("depth image creation failed");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test: Shaders
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Minimal valid SPIR-V vertex shader (just gl_Position = vec4(0)) */
static const uint32_t minimal_vert_spirv[] = {
    0x07230203, 0x00010000, 0x00080001, 0x0000000d,
    0x00000000, 0x00020011, 0x00000001, 0x0006000b,
    0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e,
    0x00000000, 0x0003000e, 0x00000000, 0x00000001,
    0x0005000f, 0x00000000, 0x00000002, 0x6e69616d,
    0x00000000, 0x00030003, 0x00000002, 0x000001c2,
    0x00040005, 0x00000002, 0x6e69616d, 0x00000000,
    0x00020013, 0x00000003, 0x00030021, 0x00000004,
    0x00000003, 0x00050036, 0x00000003, 0x00000002,
    0x00000000, 0x00000004, 0x000200f8, 0x00000005,
    0x000100fd, 0x00010038,
};

static void test_shader_create(lgx_gfx_device_t* dev) {
    TEST("shader_create (SPIR-V from memory)");
    lgx_gfx_shader_t* shader = lgx_gfx_shader_create(dev, minimal_vert_spirv,
                                                       sizeof(minimal_vert_spirv),
                                                       LGX_GFX_SHADER_VERTEX, "main");
    if (shader) {
        lgx_gfx_shader_destroy(shader);
        PASS();
    } else {
        FAIL("shader creation failed");
    }
}

static void test_shader_null_params(void) {
    TEST("shader_create (NULL params)");
    lgx_gfx_shader_t* s1 = lgx_gfx_shader_create(NULL, minimal_vert_spirv,
                                                    sizeof(minimal_vert_spirv),
                                                    LGX_GFX_SHADER_VERTEX, "main");
    lgx_gfx_shader_t* s2 = lgx_gfx_shader_create(NULL, NULL, 0,
                                                    LGX_GFX_SHADER_VERTEX, "main");
    if (!s1 && !s2) {
        PASS();
    } else {
        FAIL("should return NULL");
    }
}

static void test_shader_load_missing(lgx_gfx_device_t* dev) {
    TEST("shader_load (missing file)");
    lgx_gfx_shader_t* shader = lgx_gfx_shader_load(dev, "/nonexistent/shader.spv",
                                                      LGX_GFX_SHADER_VERTEX);
    if (!shader) {
        PASS();
    } else {
        FAIL("should return NULL for missing file");
        lgx_gfx_shader_destroy(shader);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test: Statistics
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_stats(lgx_gfx_device_t* dev) {
    TEST("get_stats");
    lgx_gfx_stats_t stats;
    lgx_result_t r = lgx_gfx_get_stats(dev, &stats);
    if (r == LGX_SUCCESS && strlen(stats.gpu_name) > 0 &&
        stats.vulkan_api_version > 0) {
        PASS();
    } else {
        FAIL("bad stats");
    }
}

static void test_stats_frame_reset(lgx_gfx_device_t* dev) {
    TEST("reset_frame_stats");
    lgx_gfx_reset_frame_stats(dev);
    lgx_gfx_stats_t stats;
    lgx_gfx_get_stats(dev, &stats);
    if (stats.draw_calls == 0 && stats.pipeline_switches == 0 &&
        stats.vertices_submitted == 0) {
        PASS();
    } else {
        FAIL("frame stats not zeroed");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test: Shader Cache
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_shader_cache(lgx_gfx_device_t* dev) {
    TEST("shader_cache_init/destroy");
    lgx_gfx_shader_cache_config_t config = {
        .struct_size = sizeof(lgx_gfx_shader_cache_config_t),
        .cache_directory = "/tmp/lgx_test_cache",
        .max_cache_size_mb = 64,
        .enable_disk_cache = true,
    };
    lgx_result_t r = lgx_gfx_shader_cache_init(dev, &config);
    if (r == LGX_SUCCESS) {
        lgx_gfx_shader_cache_destroy(dev);
        PASS();
    } else {
        FAIL("shader cache init failed");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test: Wait Idle
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_wait_idle(lgx_gfx_device_t* dev) {
    TEST("device_wait_idle");
    lgx_result_t r = lgx_gfx_device_wait_idle(dev);
    if (r == LGX_SUCCESS) {
        PASS();
    } else {
        FAIL("wait_idle failed");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  LGX Graphics Module v1.2 — Basic Tests                    ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    /* NULL-safety tests (no device needed) */
    test_device_destroy_null();
    test_device_null_config();
    test_buffer_null_params();
    test_buffer_destroy_null();
    test_shader_null_params();
    test_gpu_info_null();

    /* Device-dependent tests */
    lgx_gfx_device_t* dev = test_device_create();
    if (!dev) {
        printf("\n[SKIP] Cannot create Vulkan device — skipping GPU tests\n");
        printf("\n═══ Results: %d/%d passed ═══\n\n", tests_passed, tests_run);
        return (tests_passed == tests_run) ? 0 : 1;
    }

    test_gpu_info(dev);
    test_wait_idle(dev);
    test_buffer_create_destroy(dev);
    test_buffer_map_upload(dev);
    test_buffer_device_local(dev);
    test_image_create_destroy(dev);
    test_image_depth(dev);
    test_shader_create(dev);
    test_shader_load_missing(dev);
    test_shader_cache(dev);
    test_stats(dev);
    test_stats_frame_reset(dev);

    lgx_gfx_device_destroy(dev);

    printf("\n═══ Results: %d/%d passed ═══\n\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
