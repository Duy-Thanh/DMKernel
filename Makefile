CC = gcc
CFLAGS = -Wall -Wextra -g -I./include
LDFLAGS = -lm -rdynamic

# Directories
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin
TEST_DIR = tests
TEST_BUILD_DIR = build/tests

# Source files
CORE_SRC = $(wildcard $(SRC_DIR)/core/*.c)
SHELL_SRC = $(wildcard $(SRC_DIR)/shell/*.c)
LANG_SRC = $(wildcard $(SRC_DIR)/lang/*.c)
PRIM_SRC = $(wildcard $(SRC_DIR)/primitives/*.c)
MAIN_SRC = $(SRC_DIR)/main.c
ALL_SRC = $(CORE_SRC) $(SHELL_SRC) $(LANG_SRC) $(PRIM_SRC)

# Test files
TEST_SRC = $(wildcard $(TEST_DIR)/*.c)
TEST_NAMES = $(basename $(notdir $(TEST_SRC)))
TEST_BINS = $(patsubst %,$(BIN_DIR)/%, $(TEST_NAMES))

# Object files
CORE_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(CORE_SRC))
SHELL_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SHELL_SRC))
LANG_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(LANG_SRC))
PRIM_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(PRIM_SRC))
MAIN_OBJ = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(MAIN_SRC))
LIB_OBJS = $(CORE_OBJS) $(SHELL_OBJS) $(LANG_OBJS) $(PRIM_OBJS)
ALL_OBJS = $(LIB_OBJS) $(MAIN_OBJ)
TEST_OBJS = $(patsubst $(TEST_DIR)/%.c,$(TEST_BUILD_DIR)/%.o,$(TEST_SRC))

# Main targets
.PHONY: all clean tests

all: dirs dmkernel tests

dirs:
	mkdir -p $(BUILD_DIR)/core $(BUILD_DIR)/shell $(BUILD_DIR)/lang $(BUILD_DIR)/primitives $(BIN_DIR) $(TEST_BUILD_DIR)

# Special object file for main.c as a library (depends on dirs)
$(BUILD_DIR)/main_lib.o: $(SRC_DIR)/main.c | dirs
	$(CC) $(CFLAGS) -DDMKERNEL_AS_LIBRARY -c $< -o $@

dmkernel: $(ALL_OBJS)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/$@ $^ $(LDFLAGS)

# Create libdmkernel.a archive for linking with tests
$(BUILD_DIR)/libdmkernel.a: $(LIB_OBJS) $(BUILD_DIR)/main_lib.o
	ar rcs $@ $^

tests: $(BUILD_DIR)/libdmkernel.a $(TEST_BINS)

# General rule for test binaries
$(BIN_DIR)/test_%: $(TEST_BUILD_DIR)/test_%.o $(BUILD_DIR)/libdmkernel.a
	$(CC) $(CFLAGS) -o $@ $< $(BUILD_DIR)/libdmkernel.a $(LDFLAGS)

# Compile rules
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_BUILD_DIR)/%.o: $(TEST_DIR)/%.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

run_tests: tests
	@echo "Running kernel panic test..."
	@echo "Note: This will trigger a kernel panic and halt execution."
	@echo "Press Enter to continue..."
	@read dummy
	$(BIN_DIR)/test_panic

run_watchdog: tests
	@echo "Running kernel watchdog test..."
	@echo "Note: This will demonstrate various panic scenarios."
	@echo "Press Enter to continue..."
	@read dummy
	$(BIN_DIR)/test_watchdog 