# Host tests for ESPUI pure logic.
#
# ESP-IDF is not installed yet, so these tests exercise the parts of ESPUI
# that do not need FreeRTOS, LVGL or a panel: manifest validation, tokens,
# ViewModel, scene manager, theme manager/registry, input routing, action
# dispatch, motion scheduling, asset/memory bookkeeping and state mapping.
#
# Usage:
#   make test     # build + run
#   make syntax   # compile every firmware C file against host stubs
#   make clean

ROOT := ../..
ESPUI := $(ROOT)/components/espui
COMPONENTS := $(ROOT)/components

CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O1 -g \
          -fsanitize=address,undefined -fno-omit-frame-pointer
INC := -Istubs \
       -I$(ESPUI)/theme_api \
       -I$(ESPUI)/core \
       -I$(ESPUI)/runtime \
       -I$(ESPUI)/hal \
       -I$(ESPUI)/themes \
       -I$(ESPUI)/themes/terminal_aesthetic/renderers \
       -I$(COMPONENTS)/protocol/include \
       -I$(COMPONENTS)/state/include \
       -I$(COMPONENTS)/networking/include

ALL_ESPUI_SRCS := \
	$(wildcard $(ESPUI)/theme_api/*.c) \
	$(wildcard $(ESPUI)/core/*.c) \
	$(wildcard $(ESPUI)/runtime/*.c) \
	$(wildcard $(ESPUI)/themes/*.c) \
	$(wildcard $(ESPUI)/themes/safe/*.c) \
	$(wildcard $(ESPUI)/themes/terminal_aesthetic/*.c) \
	$(wildcard $(ESPUI)/themes/terminal_aesthetic/renderers/*.c)

# The UI task and LVGL renderer need FreeRTOS/LVGL; they are covered by the
# syntax-check target instead of the pure-logic test binary.
NON_PURE_SRCS := $(ESPUI)/core/espui.c $(ESPUI)/runtime/lvgl_renderer.c

PURE_SRCS := \
	$(filter-out $(NON_PURE_SRCS), $(ALL_ESPUI_SRCS)) \
	$(wildcard $(COMPONENTS)/protocol/*.c) \
	$(wildcard $(COMPONENTS)/state/state_adapter.c)

TEST_SRCS := $(wildcard test_*.c) stubs/espui_runtime_stub.c
BUILD_DIR := build
TEST_BIN := $(BUILD_DIR)/espui_tests

ASCII_PREVIEW_SRCS := \
	ascii_preview.c \
	$(ESPUI)/themes/terminal_aesthetic/renderers/ascii_canvas.c \
	$(ESPUI)/themes/terminal_aesthetic/renderers/ascii_renderer.c \
	$(ESPUI)/theme_api/espui_motion.c \
	$(ESPUI)/theme_api/espui_types.c \
	$(ESPUI)/theme_api/espui_util.c \
	$(ESPUI)/core/espui_view_model.c \
	$(ESPUI)/runtime/animation_scheduler.c
PREVIEW_BIN := $(BUILD_DIR)/ascii_preview
STATE ?= all
FRAMES ?= 2
DELAY ?= 400

.PHONY: all test syntax preview clean

all: test

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(PURE_SRCS) $(TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INC) $^ -o $@

preview: $(PREVIEW_BIN)
	./$(PREVIEW_BIN) $(STATE) $(FRAMES) $(DELAY)

$(PREVIEW_BIN): $(ASCII_PREVIEW_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INC) $^ -o $@

syntax:
	./syntax_check.sh

clean:
	rm -rf $(BUILD_DIR)
