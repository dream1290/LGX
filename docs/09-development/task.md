# LGX Refactoring Task

## Phase 0 — Repo Hygiene
- [x] `git rm --cached tests/test_results.txt` — done (not in `git ls-files`)
- [x] `git rm --cached lead-engineer-new-verdict/` — not tracked (already ignored)
- [x] Update [.gitignore](file:///home/karl/Projects/LGX/.gitignore) with missing binary patterns

## Phase 1 — Correctness Bugs
- [x] Fix `use_huge_pages` flag override (`lgx_memory_manager.c:390-391`) — removed, correct logic at lines 287-294
- [x] Fix silent dealloc counter for pool-origin pointers
  - Note: previously introduced double-increment; fixed — `total_deallocations` is now incremented only once at function entry, not again in the pool branch
- [x] Fix `posix_memalign` stats counter ordering (`lgx_memory_manager_alloc_aligned`) — counter incremented before `posix_memalign`
- [x] Fix wrong field names in unit tests (`test_memory_allocation.c`) — `total_freed`→`total_deallocated`, `current_usage`→`current_allocated`, arena stats corrected
- [x] Fix `lgx_gpu_free` wrong signature in test (`test_memory_allocation.c:163`) — removed extra `0` argument

## Phase 2 — Code Cleanliness
- [x] Remove duplicate forward declarations in [lgx_memory_manager.c](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c) — one clean block remains
- [x] Remove dead `pool_alloc()` forward declaration (retained function body — needs decision)
- [x] Remove duplicate forward declarations in [lgx_frame_arena.c](file:///home/karl/Projects/LGX/src/runtime/lgx_frame_arena.c) — done
- [x] Remove duplicate declarations in [lgx_runtime.h](file:///home/karl/Projects/LGX/include/lgx_runtime.h) — duplicates removed, `lgx_alloc_with_intent_ex` kept
- [x] Move resource limits & memory safety types inside include guard in [lgx_runtime_internal.h](file:///home/karl/Projects/LGX/include/lgx/lgx_runtime_internal.h) — 82 lines of duplicated post-`#endif` content deleted

## Phase 3 — Compiler Hardening
- [x] Add `-Wredundant-decls -Wshadow -Wstrict-prototypes -Wmissing-prototypes` to [CMakeLists.txt](file:///home/karl/Projects/LGX/CMakeLists.txt)

## Verification
- [/] Build passes clean (no warnings, no errors) — running now
- [ ] All 64 tests pass
