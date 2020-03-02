CC = gcc

TARGET = yayash
SRCS = $(wildcard *.c)
TEST_SRCS = $(wildcard test/*.c)

BIN_DIR = ./bin
RELEASE_BIN_DIR = ./bin/release
RELEASE_TARGET = $(RELEASE_BIN_DIR)/$(TARGET)
DEBUG_BIN_DIR = ./bin/debug
DEBUG_TARGET = $(DEBUG_BIN_DIR)/$(TARGET)

TEST_RELEASE_TARGET = $(RELEASE_BIN_DIR)/test

OBJS_DIR = ./objs
RELEASE_OBJS_DIR = ./objs/release
TEST_RELEASE_OBJS_DIR = ./objs/release/test
RELEASE_OBJS = $(SRCS:%.c=$(RELEASE_OBJS_DIR)/%.o)
TEST_RELEASE_OBJS = $(filter-out $(RELEASE_OBJS_DIR)/main.o, $(RELEASE_OBJS))
TEST_RELEASE_OBJS += $(TEST_SRCS:%.c=$(RELEASE_OBJS_DIR)/%.o)

DEBUG_OBJS_DIR = ./objs/debug
DEBUG_OBJS = $(SRCS:%.c=$(DEBUG_OBJS_DIR)/%.o)

DEPS_DIR = ./objs
RELEASE_DEPS_DIR = $(DEPS_DIR)/release
TEST_RELEASE_DEPS_DIR = $(DEPS_DIR)/release/test
RELEASE_DEPS = $(SRCS:%.c=$(RELEASE_DEPS_DIR)/%.d)
DEBUG_DEPS_DIR = $(DEPS_DIR)/debug
DEBUG_DEPS = $(SRCS:%.c=$(DEBUG_DEPS_DIR)/%.d)


DEBUG_FLAGS = -g3 -O0
RELEASE_FLAGS = -O3

INCLUDE += -I./

CFLAGS = -Wall -Wextra -std=c11
CFLAGS += -Wno-unused-parameter
CFLAGS += -MMD -MP



.PHONY: all test debug release clean

all: $(RELEASE_TARGET)

run: $(RELEASE_TARGET)
	$(RELEASE_TARGET) $(ARGS)

runtest: $(TEST_RELEASE_TARGET)
	$(TEST_RELEASE_TARGET) $(ARGS)

test: $(TEST_RELEASE_TARGET)

release: $(RELEASE_TARGET)

debug: $(DEBUG_TARGET)
	
$(RELEASE_TARGET) : $(RELEASE_OBJS) Makefile | $(RELEASE_BIN_DIR)
	$(CC) $(LDFLAGS) $(LDLIBS) -o $@ $(RELEASE_OBJS)

$(TEST_RELEASE_TARGET) : $(TEST_RELEASE_OBJS) Makefile | $(RELEASE_BIN_DIR)
	$(CC) $(LDFLAGS) $(LDLIBS)  -o $@ $(TEST_RELEASE_OBJS)

$(DEBUG_TARGET) : $(DEBUG_OBJS) Makefile | $(DEBUG_BIN_DIR)
	$(CC) $(LDFLAGS) $(LDLIBS) -o $@ $(DEBUG_OBJS)

$(RELEASE_BIN_DIR):
	mkdir -p $@
$(RELEASE_OBJS_DIR):
	mkdir -p $@
$(TEST_RELEASE_OBJS_DIR):
	mkdir -p $@
$(DEBUG_BIN_DIR):
	mkdir -p $@
$(DEBUG_OBJS_DIR):
	mkdir -p $@
$(DEPS_DIR):
	mkdir -p $@

$(TEST_RELEASE_OBJS_DIR)/%.o: test/%.c Makefile | $(TEST_RELEASE_OBJS_DIR) $(TEST_RELEASE_DEPS_DIR)
	$(CC) $(CFLAGS) $(INCLUDE) $(RELEASE_FLAGS) -c -o $@ $<
$(RELEASE_OBJS_DIR)/%.o: %.c Makefile | $(RELEASE_OBJS_DIR) $(RELEASE_DEPS_DIR)
	$(CC) $(CFLAGS) $(INCLUDE) $(RELEASE_FLAGS) -c -o $@ $<
$(DEBUG_OBJS_DIR)/%.o: %.c Makefile | $(DEBUG_OBJS_DIR) $(DEBUG_DEPS_DIR)
	$(CC) $(CFLAGS) $(INCLUDE) $(DEBUG_FLAGS) -c -o $@ $<

-include $(RELEASE_DEPS)
-include $(DEBUG_DEPS)


clean:
	rm -rf $(BIN_DIR) $(OBJS_DIR) $(DEPS_DIR)
