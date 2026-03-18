# Memory Usage Optimization Plan

**Task**: 12.2 Optimize memory usage  
**Goal**: Achieve <200MB memory overhead (Tier 2 target)  
**Date**: February 9, 2026  

---

##  Objective

Reduce runtime memory footprint to meet Tier 2 target of <200MB overhead.

**Current State**: Unknown (needs measurement)  
**Target**: <200MB (Tier 2), <300MB (Tier 1)  

---

##  Memory Usage Analysis

### Major Memory Consumers

1. **Frame Arenas** (3 × 64MB = 192MB)
   - Triple-buffered for frame rotation
   - Largest single consumer
   - Optimization: Reduce size or use lazy allocation

2. **GPU Memory Pool** (~256MB)
   - Pre-allocated GPU-visible memory
   - Host-visible: 256MB
   - Device-local: 2GB (not counted in RSS)
   - Optimization: Lazy allocation, smaller initial size

3. **Persistent Heap** (~256MB)
   - Slab allocator: 2MB
   - Buddy allocator: 256MB
   - Optimization: Lazy allocation, on-demand growth

4. **Telemetry Buffers** (~16MB)
   - Ring buffer: 8MB
   - Event storage: 8MB
   - Optimization: Lazy initialization, smaller buffers

5. **Runtime Metadata** (~10MB)
   - Performance counters
   - Statistics
   - Configuration
   - Optimization: Minimal impact

**Estimated Total**: ~730MB (exceeds target!)

---

## 🔧 Optimization Strategy

### Phase 1: Lazy Initialization (Task 12.2.3)
**Impact**: 50-70% reduction  
**Effort**: Medium  

Initialize components only when first used:
-  Telemetry (only if enabled)
-  GPU pool (only if GPU allocations requested)
- ⚠️ Frame arenas (needed immediately)
- ⚠️ Persistent heap (needed immediately)

### Phase 2: Reduce Pool Sizes (Task 12.2.2)
**Impact**: 30-50% reduction  
**Effort**: Low  

Based on profiling data:
- Frame arenas: 64MB → 32MB (still generous)
- GPU pool: 256MB → 128MB (grow on demand)
- Persistent heap: 256MB → 128MB (grow on demand)

### Phase 3: On-Demand Growth (Task 12.2.1)
**Impact**: 20-30% reduction  
**Effort**: High  

Start small, grow as needed:
- Frame arenas: Start with 1 arena, add 2nd/3rd on overflow
- GPU pool: Start with 64MB, grow to 256MB
- Persistent heap: Start with 64MB, grow to 256MB

### Phase 4: Memory Monitoring (Task 12.2.4)
**Impact**: Visibility  
**Effort**: Low  

Track memory usage:
- RSS (Resident Set Size)
- Per-allocator usage
- Peak usage
- Growth patterns

---

## 📋 Implementation Plan

### 12.2.1 Reduce Runtime Memory Footprint

**Goal**: Minimize initial memory allocation

**Changes**:
1. Reduce frame arena size: 64MB → 32MB per arena
2. Reduce GPU pool initial size: 256MB → 64MB
3. Reduce persistent heap initial size: 256MB → 64MB
4. Implement on-demand growth for all allocators

**Expected Savings**: 400-500MB

### 12.2.2 Optimize Pool Sizes Based on Profiling

**Goal**: Right-size allocators based on actual usage

**Approach**:
1. Profile typical game workloads
2. Measure actual allocation patterns
3. Adjust pool sizes accordingly
4. Add configuration options for tuning

**Data Needed**:
- Peak frame arena usage
- Peak GPU pool usage
- Peak persistent heap usage
- Allocation frequency

### 12.2.3 Implement Lazy Initialization

**Goal**: Don't allocate until needed

**Components**:
1. **Telemetry**  Already lazy
   - Only initialize if `lgx_telemetry_enable()` called
   
2. **GPU Pool** ⚠️ Needs lazy init
   - Only initialize on first `lgx_gpu_alloc()` call
   - Check if GPU is available first
   
3. **Frame Arenas** ⚠️ Partial lazy init
   - Initialize 1st arena immediately
   - Initialize 2nd/3rd on overflow
   
4. **Persistent Heap** ⚠️ Needs lazy init
   - Start with small size (64MB)
   - Grow on demand

### 12.2.4 Add Memory Usage Monitoring

**Goal**: Track and report memory usage

**Metrics**:
1. **RSS (Resident Set Size)**
   - Total memory used by process
   - Read from `/proc/self/status`
   
2. **Per-Allocator Usage**
   - Frame arena: current + peak
   - GPU pool: current + peak
   - Persistent heap: current + peak
   
3. **Overhead Calculation**
   - Metadata overhead
   - Fragmentation overhead
   - Unused capacity

**API**:
```c
typedef struct {
    size_t rss_bytes;              // Total RSS
    size_t frame_arena_bytes;      // Frame arena usage
    size_t gpu_pool_bytes;         // GPU pool usage
    size_t persistent_heap_bytes;  // Persistent heap usage
    size_t metadata_bytes;         // Metadata overhead
    size_t peak_rss_bytes;         // Peak RSS
} lgx_memory_usage_t;

lgx_result_t lgx_get_memory_usage(lgx_memory_usage_t* usage);
```

### 12.2.5 Validate <200MB Memory Overhead Target

**Goal**: Measure and verify memory usage

**Validation**:
1. Run memory overhead benchmark
2. Measure RSS before/after init
3. Verify < 200MB overhead
4. Document actual usage

---

##  Memory Targets

### Tier 1 (MVP): <300MB
- Frame arenas: 96MB (3 × 32MB)
- GPU pool: 64MB (lazy, grow to 128MB)
- Persistent heap: 64MB (lazy, grow to 128MB)
- Telemetry: 8MB (lazy)
- Metadata: 10MB
- **Total**: ~242MB 

### Tier 2 (Competitive): <200MB
- Frame arenas: 64MB (2 × 32MB, 3rd lazy)
- GPU pool: 32MB (lazy, grow to 64MB)
- Persistent heap: 32MB (lazy, grow to 64MB)
- Telemetry: 4MB (lazy)
- Metadata: 10MB
- **Total**: ~142MB 

### Aggressive: <150MB
- Frame arenas: 32MB (1 × 32MB, 2nd/3rd lazy)
- GPU pool: 16MB (lazy, grow to 32MB)
- Persistent heap: 16MB (lazy, grow to 32MB)
- Telemetry: 2MB (lazy)
- Metadata: 10MB
- **Total**: ~76MB 

---

##  Implementation Details

### Lazy GPU Pool Initialization

```c
static bool g_gpu_pool_initialized = false;
static pthread_mutex_t g_gpu_init_mutex = PTHREAD_MUTEX_INITIALIZER;

void* lgx_gpu_alloc(size_t size, size_t alignment, lgx_gpu_memory_type_t type) {
    // Lazy initialization
    if (!g_gpu_pool_initialized) {
        pthread_mutex_lock(&g_gpu_init_mutex);
        if (!g_gpu_pool_initialized) {
            lgx_result_t result = lgx_gpu_pool_init_lazy();
            if (result != LGX_SUCCESS) {
                pthread_mutex_unlock(&g_gpu_init_mutex);
                return NULL;
            }
            g_gpu_pool_initialized = true;
        }
        pthread_mutex_unlock(&g_gpu_init_mutex);
    }
    
    // Proceed with allocation
    return lgx_gpu_pool_alloc_internal(size, alignment, type);
}
```

### On-Demand Frame Arena Growth

```c
typedef struct {
    void* arenas[3];
    size_t arena_size;
    int num_arenas;  // Start with 1, grow to 3
    int current_arena;
} frame_arena_state_t;

void* lgx_frame_alloc(size_t size) {
    frame_arena_state_t* state = &g_frame_arena_state;
    
    // Try current arena
    void* ptr = bump_alloc(state->arenas[state->current_arena], size);
    if (ptr) return ptr;
    
    // Arena full, try to add another arena
    if (state->num_arenas < 3) {
        state->num_arenas++;
        state->arenas[state->num_arenas - 1] = allocate_arena(state->arena_size);
    }
    
    // Rotate to next arena
    state->current_arena = (state->current_arena + 1) % state->num_arenas;
    return bump_alloc(state->arenas[state->current_arena], size);
}
```

### Memory Usage Monitoring

```c
size_t get_rss_bytes(void) {
    FILE* f = fopen("/proc/self/status", "r");
    if (!f) return 0;
    
    char line[256];
    size_t rss_kb = 0;
    
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%zu", &rss_kb);
            break;
        }
    }
    
    fclose(f);
    return rss_kb * 1024;  // Convert to bytes
}
```

---

## ⚠️ Trade-offs

### Lazy Initialization
**Pros**:
- Reduced initial memory footprint
- Only pay for what you use
- Better for simple applications

**Cons**:
- First allocation slower (initialization cost)
- Unpredictable latency
- Complexity in thread-safety

### Smaller Pool Sizes
**Pros**:
- Lower memory usage
- Better cache utilization
- Faster initialization

**Cons**:
- More frequent growth/reallocation
- Potential fragmentation
- May hit limits in heavy workloads

### On-Demand Growth
**Pros**:
- Start small, grow as needed
- Optimal memory usage
- Adapts to workload

**Cons**:
- Growth overhead
- Unpredictable performance
- Complexity in implementation

---

##  Execution Plan

### Week 1: Measurement & Analysis
- [ ] Implement memory monitoring (12.2.4)
- [ ] Run memory overhead benchmark
- [ ] Profile typical workloads
- [ ] Identify optimization opportunities

### Week 2: Implementation
- [ ] Reduce pool sizes (12.2.2)
- [ ] Implement lazy initialization (12.2.3)
- [ ] Implement on-demand growth (12.2.1)
- [ ] Add configuration options

### Week 3: Validation
- [ ] Run memory overhead benchmark
- [ ] Verify <200MB target (12.2.5)
- [ ] Check for regressions
- [ ] Update documentation

**Total Time**: 2-3 weeks  
**Expected Outcome**: <200MB memory overhead 

---

##  Success Criteria

- [ ] Memory monitoring implemented
- [ ] Lazy initialization for optional components
- [ ] Reduced pool sizes based on profiling
- [ ] RSS < 200MB after initialization
- [ ] No functional regressions
- [ ] All tests pass

---

**Status**: Planning complete, ready to implement  
**Priority**: High (Tier 2 target)  
**Risk**: Medium (trade-offs between memory and performance)
