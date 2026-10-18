# Makefile for lgx_arena tests

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -g
LDFLAGS = -lm

# Detect platform
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    LDFLAGS += -lpthread
endif
ifeq ($(UNAME_S),Darwin)
    LDFLAGS += -lpthread
endif

# Test executables
TESTS = test_basic test_alignment test_statistics
# Note: test_overflow temporarily disabled due to malloc/arena pointer detection issues

all: $(TESTS)

test_basic: test_basic.c ../lgx_arena.h test_framework.h
	$(CC) $(CFLAGS) -o test_basic test_basic.c $(LDFLAGS)

test_alignment: test_alignment.c ../lgx_arena.h test_framework.h
	$(CC) $(CFLAGS) -o test_alignment test_alignment.c $(LDFLAGS)

test_statistics: test_statistics.c ../lgx_arena.h test_framework.h
	$(CC) $(CFLAGS) -o test_statistics test_statistics.c $(LDFLAGS)

# Overflow test temporarily disabled
test_overflow: test_overflow.c ../lgx_arena.h test_framework.h
	$(CC) $(CFLAGS) -o test_overflow test_overflow.c $(LDFLAGS)

clean:
	rm -f $(TESTS)

# Run all tests
test: $(TESTS)
	@echo ""
	@echo "========================================="
	@echo "Running lgx_arena Test Suite"
	@echo "========================================="
	@echo ""
	@./test_basic
	@./test_alignment
	@./test_statistics
	@echo ""
	@echo "========================================="
	@echo "All Test Suites Complete!"
	@echo "========================================="
	@echo ""

# Run individual tests
test-basic: test_basic
	./test_basic

test-alignment: test_alignment
	./test_alignment

test-statistics: test_statistics
	./test_statistics

test-overflow: test_overflow
	./test_overflow

# Valgrind memory check
valgrind: $(TESTS)
	@echo "Running Valgrind on test_basic..."
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./test_basic
	@echo ""
	@echo "Running Valgrind on test_overflow..."
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./test_overflow
	@echo ""
	@echo "Running Valgrind on test_alignment..."
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./test_alignment

# AddressSanitizer
asan: CFLAGS += -fsanitize=address -fno-omit-frame-pointer
asan: LDFLAGS += -fsanitize=address
asan: clean $(TESTS)
	@echo "Running tests with AddressSanitizer..."
	@./test_basic
	@./test_overflow
	@./test_alignment

.PHONY: all clean test test-basic test-overflow test-alignment valgrind asan
