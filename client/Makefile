CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude -D_XOPEN_SOURCE=500 `pkg-config --cflags gtk+-3.0`
LDFLAGS = `pkg-config --libs gtk+-3.0` -pthread

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# Find all source files
SRCS = $(shell find $(SRC_DIR) -name '*.c')
# Generate object file names
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Output binary
TARGET = $(BIN_DIR)/client

.PHONY: all clean directories

all: directories $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

directories:
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)