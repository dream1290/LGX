# LGX Lifetime Profiler — Build
#
# Targets:
#   make all          Build probe shim and test workload
#   make test         Build + run synthetic test + analyze
#   make clean        Remove build artifacts
#
# Usage with real games:
#   make all
#   LD_PRELOAD=./lgx_lifetime_probe.so supertuxkart
#   python3 analyze_lifetimes.py /tmp/lgx_probe_*.bin

CC       = gcc
CFLAGS   = -O2 -Wall -Wextra -Wpedantic -fPIC
LDFLAGS_PROBE = -shared -ldl -lpthread
LDFLAGS_WORK  = -ldl

.PHONY: all clean test test-stk

all: lgx_lifetime_probe.so test_workload

lgx_lifetime_probe.so: lgx_lifetime_probe.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS_PROBE)

test_workload: test_workload.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS_WORK)

# Run synthetic test: probe → analyze
test: all
	@echo "═══════════════════════════════════════════════════════════"
	@echo "  Running synthetic workload with probe..."
	@echo "═══════════════════════════════════════════════════════════"
	LGX_PROBE_LOG_ENTRIES=4000000 \
	LD_PRELOAD=./lgx_lifetime_probe.so \
	./test_workload
	@echo ""
	@echo "═══════════════════════════════════════════════════════════"
	@echo "  Analyzing results..."
	@echo "═══════════════════════════════════════════════════════════"
	@LOG=$$(ls -t /tmp/lgx_probe_*.bin 2>/dev/null | head -1) && \
	if [ -n "$$LOG" ]; then \
		python3 analyze_lifetimes.py "$$LOG"; \
	else \
		echo "ERROR: No probe log found in /tmp/"; \
	fi

# Run against SuperTuxKart (if installed)
# Play for 30-60 seconds, then quit normally
test-stk: all
	@echo "═══════════════════════════════════════════════════════════"
	@echo "  Running SuperTuxKart with probe..."
	@echo "  Play for 30-60 seconds, then quit normally."
	@echo "═══════════════════════════════════════════════════════════"
	LGX_PROBE_LOG_ENTRIES=8000000 \
	LD_PRELOAD=./lgx_lifetime_probe.so \
	supertuxkart || true
	@echo ""
	@echo "═══════════════════════════════════════════════════════════"
	@echo "  Analyzing results..."
	@echo "═══════════════════════════════════════════════════════════"
	@LOG=$$(ls -t /tmp/lgx_probe_*.bin 2>/dev/null | head -1) && \
	if [ -n "$$LOG" ]; then \
		python3 analyze_lifetimes.py "$$LOG"; \
	else \
		echo "ERROR: No probe log found in /tmp/"; \
	fi

clean:
	rm -f lgx_lifetime_probe.so test_workload
	rm -f /tmp/lgx_probe_*.bin
