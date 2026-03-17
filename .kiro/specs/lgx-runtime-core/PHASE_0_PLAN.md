# Phase 0: Validation Prototype (Week 1-4)

## Goal
Prove the basic architecture works.

## Deliverables
1. Minimal runtime with init/shutdown
2. Simple allocator (just malloc wrapper)
3. Version check
4. Test game that links against runtime

## Success Criteria
- Runtime initializes in <1 second
- Allocations work
- Test game runs without crashing

## What We're NOT Building
- Lock-free allocator (Phase 1)
- Pinned libraries (Phase 1)
- Telemetry (Phase 1)
- Intent API (Layer 2)
- Mathematical proofs (Layer 4)

## Timeline
- Week 1: Build skeleton
- Week 2: Add basic allocator
- Week 3: Measure performance
- Week 4: Write validation report

## Decision Point
If Phase 0 works → Proceed to Phase 1  
If Phase 0 fails → Revise architecture

## This Weekend (48 Hours)

**Saturday:**
1. Install WSL2 (30 min)
2. Create project structure (30 min)
3. Write minimal runtime code (2 hours)
4. Build and run test_game (30 min)

**Sunday:**
1. Add basic allocator (2 hours)
2. Measure initialization time (30 min)
3. Write 1-page validation report (1 hour)

**Total: ~7 hours**

## Code to Write (Minimal)

### src/lgx_runtime.c (~20 lines)
```c
#include "lgx_runtime.h"
#include <stdio.h>
#include <stdlib.h>

lgx_result_t lgx_runtime_init(void) {
    printf("LGX Runtime initialized\n");
    return LGX_SUCCESS;
}

void* lgx_alloc(size_t size) {
    return malloc(size);
}

void lgx_free(void* ptr) {
    free(ptr);
}
```

### tests/test_game.c (~10 lines)
```c
#include "lgx_runtime.h"
#include <stdio.h>

int main(void) {
    lgx_runtime_init();
    
    void* ptr = lgx_alloc(1024);
    printf("Allocated 1024 bytes: %p\n", ptr);
    lgx_free(ptr);
    
    printf("Test passed!\n");
    return 0;
}
```

**That's it. 30 lines of code. Build it this weekend.**
