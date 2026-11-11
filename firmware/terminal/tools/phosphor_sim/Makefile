ROOT := ../..
PHOSPHOR := $(ROOT)/components/espui/phosphor
CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2 -I$(PHOSPHOR)
SDL_CFLAGS := $(shell pkg-config --cflags sdl2)
SDL_LIBS := $(shell pkg-config --libs sdl2)
BUILD_DIR ?= build

PURE_SRCS := $(filter-out \
	$(PHOSPHOR)/phosphor_lvgl_port.c \
	$(PHOSPHOR)/phosphor_input_publisher.c \
	$(PHOSPHOR)/phosphor_view_model_adapter.c, \
	$(wildcard $(PHOSPHOR)/*.c))

.PHONY: all run screenshot clean

all: $(BUILD_DIR)/phosphor_sim

$(BUILD_DIR)/phosphor_sim: main.c $(PURE_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) $^ -o $@ $(SDL_LIBS) -lm

run: $(BUILD_DIR)/phosphor_sim
	./$(BUILD_DIR)/phosphor_sim

screenshot: $(BUILD_DIR)/phosphor_sim
	SDL_VIDEODRIVER=dummy ./$(BUILD_DIR)/phosphor_sim --screenshot $(BUILD_DIR)/p1.bmp

clean:
	rm -rf $(BUILD_DIR)
