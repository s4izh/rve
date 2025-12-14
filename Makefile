CC = gcc

SRC_DIR = src
TEST_SRC_DIR = $(SRC_DIR)/tests

INCLUDES = -Iinclude
BUILD_DIR = build
BUILD_DIR_RELEASE = $(BUILD_DIR)/release
BUILD_DIR_TEST = $(BUILD_DIR)/test
BIN_DIR = bin

CFLAGS = -Wall -Wextra $(INCLUDES) -std=c99 -ggdb -MMD -MP
TEST_CFLAGS = $(CFLAGS) -DTESTS

SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(SRC:$(SRC_DIR)/%.c=$(BUILD_DIR_RELEASE)/%.o)
TARGET = $(BIN_DIR)/rve
TARGET_TEST = $(BIN_DIR)/rve_test

TEST_SRCS = $(wildcard $(TEST_SRC_DIR)/*.c)
TEST_MAIN_OBJS = $(SRC:$(SRC_DIR)/%.c=$(BUILD_DIR_TEST)/%.o)
TEST_ONLY_OBJS = $(TEST_SRCS:$(TEST_SRC_DIR)/%.c=$(BUILD_DIR_TEST)/tests/%.o)
TEST_OBJS = $(TEST_MAIN_OBJS) $(TEST_ONLY_OBJS)

DEPS = $(OBJ:.o=.d)
TEST_DEPS = $(TEST_OBJS:.o=.d)

all: $(TARGET)

test: $(TARGET_TEST)

$(TARGET): $(OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $^ -o $@

$(TARGET_TEST): $(TEST_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(TEST_OBJS) -o $@

$(BUILD_DIR_TEST)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CFLAGS) -c $< -o $@

$(BUILD_DIR_TEST)/tests/%.o: $(TEST_SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CFLAGS) -c $< -o $@

$(BUILD_DIR_RELEASE)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

-include $(DEPS)
-include $(TEST_DEPS)

.PHONY: all test clean
.DEFAULT_GOAL := all
