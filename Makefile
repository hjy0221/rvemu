CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow
CPPFLAGS ?= -Iinclude
BUILD := build
LIBSRC := src/rvemu.c

.PHONY: all test clean
all: $(BUILD)/rvemu

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/rvemu: $(LIBSRC) src/main.c include/rvemu.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LIBSRC) src/main.c -o $@

$(BUILD)/test_rvemu: $(LIBSRC) tests/test_rvemu.c include/rvemu.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LIBSRC) tests/test_rvemu.c -o $@

test: $(BUILD)/test_rvemu
	./$(BUILD)/test_rvemu

clean:
	rm -rf $(BUILD)
