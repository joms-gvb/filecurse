CC := gcc
CFLAGS := -Wall -Wextra -std=c99 -O2 $(shell pkg-config --cflags ncursesw)
LDLIBS := $(shell pkg-config --libs ncursesw)
SRC_DIR := src
BUILD_DIR := bin
TARGET := $(BUILD_DIR)/filecurses

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

.PHONY: all clean dirs

all: dirs $(TARGET)

dirs:
	@mkdir -p $(BUILD_DIR)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	-rm -rf $(BUILD_DIR)/*

