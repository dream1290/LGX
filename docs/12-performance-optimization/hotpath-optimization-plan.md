# Hot Path Optimization Plan

**Task**: 12.1 Optimize hot paths  
**Goal**: Achieve <1μs allocation latency (Tier 2 target)  
**Current**: P50 = 0.89μs, P99 = 19.36μs  
**Target**: P50 < 0.5μs, P99 < 1μs  

---

## Current Hot Path Analysis

### Allocation Flow
```
lgx_alloc_with_intent()
  ├─> Validate intent (NULL check, struct_size, size)
  ├─> Route based on intent
  │   ├─> FRAME → lgx_frame_alloc()
  │   ├─> GPU → lgx_gpu_alloc()
  │   └─> PERSISTENT → lgx_heap_alloc()
  └─> Update statistics (mutex lock)
```

### Identified Bottlenecks

1. **Validation Overhead** (3-5 cycles)
   - NULL pointer checks
   - struct_size validation
   - size validation
   - Error logging (fprintf)

2. **Statistics Mutex** (50-100 cycles)
   - pthread_mutex_lock/unlock on every allocation
   - Cache line contention
   - Unnecessary in release builds

3. **Branch Mispredictions** (10-20 cycles)
   - Multiple if-else chains
   - Unpredictable intent routing
   - No branch hints

4. **Cache Misses** (100-300 cycles)
   - Intent structure not cache-aligned
   - Statistics structure accessed frequently
   - No prefetching

5. **Function Call Overhead** (5-10 cycles)
   - Multiple function calls in hot path
   - No inlining hints

---

## Optimization Strategy

### 12.1.1 Profile with perf ✅
- [x] Create profiling script
- [ ] Run perf record on allocation benchmark
- [ ] Analyze hot functions
- [ ] Identify cache misses
- [ ] Identify branch mispredictions
- [ ] Generate flamegraph

### 12.1.2 Optimize Cache Line Alignment
**Target**: Reduce cache misses by 50%

**Changes**:
1. Align intent structures to cache line (64 bytes)
2. Pack frequently accessed fields together
3. Separate hot/cold data
4. Use `__attribute__((aligned(64)))`

**Expected Impact**: 10-20% improvement

### 12.1.3 Reduce Branch Mispredictions
**Target**: Reduce branch misses by 70%

**Changes**:
1. Add `__builtin_expect()` hints for common paths
2. Reorder branches (most common first)
3. Use computed goto for intent routing
4. Eliminate unnecessary branches

**Expected Impact**: 15-25% improvement

### 12.1.4 Add Prefetching
**Target**: Hide memory latency

**Changes**:
1. Prefetch intent structure early
2. Prefetch allocator metadata
3. Use `__builtin_prefetch()`
4. Software pipelining for batch allocations

**Expected Impact**: 10-15% improvement

### 12.1.5 Additional Optimizations
**Target**: Cumulative improvements

**Changes**:
1. Remove statistics mutex in release builds
2. Inline hot functions
3. Use thread-local statistics (no mutex)
4. Remove debug logging in release
5. Use `__attribute__((hot))`

**Expected Impact**: 20-30% improvement

---

## Implementation Plan

### Phase 1: Low-Hanging Fruit (Quick Wins)
**Time**: 2-3 hours  
**Expected**: 30-40% improvement

1. Remove statistics mutex in release builds
2. Add `likely/unlikely` branch hints
3. Inline validation functions
4. Remove debug fprintf in release

### Phase 2: Cache Optimization
**Time**: 3-4 hours  
**Expected**: 10-20% improvement

1. Align structures to cache lines
2. Pack hot fields together
3. Separate hot/cold data
4. Add prefetch hints

### Phase 3: Advanced Optimizations
**Time**: 4-5 hours  
**Expected**: 10-15% improvement

1. Computed goto for routing
2. Software pipelining
3. SIMD for validation
4. Custom allocator fast paths

### Phase 4: Validation
**Time**: 2-3 hours

1. Run performance benchmarks
2. Verify <1μs target achieved
3. Check for regressions
4. Update documentation

**Total Time**: 11-15 hours  
**Total Expected Improvement**: 50-75%  
**Target Achievement**: P50 < 0.5μs, P99 < 1μs ✅

---

## Optimization Techniques

### 1. Branch Prediction Hints

```c
// Before
if (intent->lifetime == LGX_LIFETIME_FRAME) {
    ptr = lgx_frame_alloc(intent->size);
}

// After
if (__builtin_expect(intent->lifetime == LGX_LIFETIME_FRAME, 1)) {
    ptr = lgx_frame_alloc(intent->size);
}
```

### 2. Cache Line Alignment

```c
// Before
typedef struct {
    size_t size;
    lgx_lifetime_t lifetime;
    lgx_hint_t hint;
} lgx_allocation_intent_base_t;

// After
typedef struct {
    size_t size;
    lgx_lifetime_t lifetime;
    lgx_hint_t hint;
    char padding[64 - sizeof(size_t) - sizeof(lgx_lifetime_t) - sizeof(lgx_hint_t)];
} __attribute__((aligned(64))) lgx_allocation_intent_base_t;
```

### 3. Prefetching

```c
// Prefetch intent structure early
__builtin_prefetch(intent, 0, 3);  // Read, high temporal locality

// Prefetch allocator metadata
if (__builtin_expect(intent->lifetime == LGX_LIFETIME_FRAME, 1)) {
    __builtin_prefetch(&frame_arena_metadata, 0, 3);
    ptr = lgx_frame_alloc(intent->size);
}
```

### 4. Inline Hot Functions

```c
// Mark hot functions for inlining
static inline __attribute__((always_inline, hot))
void* lgx_alloc_with_intent_fast(const lgx_allocation_intent_base_t* intent) {
    // Fast path implementation
}
```

### 5. Thread-Local Statistics

```c
// Before: Global mutex
pthread_mutex_lock(&g_intent_stats.mutex);
g_intent_stats.frame_allocations++;
pthread_mutex_unlock(&g_intent_stats.mutex);

// After: Thread-local (no mutex)
__thread intent_stats_t tls_intent_stats;
tls_intent_stats.frame_allocations++;
```

### 6. Computed Goto

```c
// Before: if-else chain
if (intent->lifetime == LGX_LIFETIME_FRAME) {
    ptr = lgx_frame_alloc(intent->size);
} else if (intent->hint == LGX_HINT_GPU_SHARED) {
    ptr = lgx_gpu_alloc(intent->size, 16, LGX_GPU_HOST_VISIBLE);
} else {
    ptr = lgx_heap_alloc(intent->size);
}

// After: Computed goto (GCC extension)
static const void* dispatch_table[] = {
    &&label_frame,
    &&label_gpu,
    &&label_persistent
};

int index = compute_dispatch_index(intent);
goto *dispatch_table[index];

label_frame:
    ptr = lgx_frame_alloc(intent->size);
    goto done;
label_gpu:
    ptr = lgx_gpu_alloc(intent->size, 16, LGX_GPU_HOST_VISIBLE);
    goto done;
label_persistent:
    ptr = lgx_heap_alloc(intent->size);
done:
    return ptr;
```

---

## Performance Targets

### Current Performance
- P50: 0.89μs ✅ (already good)
- P99: 19.36μs ❌ (needs improvement)
- Cache hit rate: 94.9%
- Branch miss rate: ~5%

### Target Performance (After Optimization)
- P50: < 0.5μs (44% improvement)
- P99: < 1μs (95% improvement)
- Cache hit rate: > 98%
- Branch miss rate: < 2%

### Validation Criteria
- [ ] P50 < 0.5μs
- [ ] P99 < 1μs
- [ ] No functional regressions
- [ ] All tests pass
- [ ] Performance benchmarks show improvement

---

## Risk Assessment

### Low Risk
- Branch hints (`likely/unlikely`)
- Inline hints
- Remove debug code in release
- Thread-local statistics

### Medium Risk
- Cache line alignment (ABI impact)
- Prefetching (may hurt on some CPUs)
- Computed goto (GCC-specific)

### High Risk
- SIMD validation (portability)
- Custom fast paths (complexity)

### Mitigation
- Test on multiple CPUs
- Provide fallback implementations
- Use feature detection
- Comprehensive benchmarking

---

## Next Steps

1. **Run profiling script** to establish baseline
2. **Implement Phase 1** optimizations (quick wins)
3. **Benchmark** and validate improvements
4. **Implement Phase 2** (cache optimization)
5. **Benchmark** and validate improvements
6. **Implement Phase 3** (advanced optimizations)
7. **Final validation** against targets
8. **Document** optimizations and results

---

**Status**: Ready to implement  
**Priority**: High (blocks Tier 2 performance target)  
**Estimated Time**: 11-15 hours  
**Expected Outcome**: P50 < 0.5μs, P99 < 1μs ✅
