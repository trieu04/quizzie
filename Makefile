CC = gcc
CFLAGS = -Wall -Wextra -g -D_XOPEN_SOURCE=500
GTK_CFLAGS = `pkg-config --cflags gtk+-3.0`
GTK_LDFLAGS = `pkg-config --libs gtk+-3.0`
LDFLAGS = -pthread

# Directories
BIN_DIR = bin
OBJ_DIR = obj

CLIENT_DIR = client
CLIENT_OBJ_DIR = $(OBJ_DIR)/client
CLIENT_INC_DIR = $(CLIENT_DIR)/include

SERVER_DIR = server
SERVER_OBJ_DIR = $(OBJ_DIR)/server
SERVER_INC_DIR = $(SERVER_DIR)/include

# Find all .c files in client and server directories (excluding include directories)
CLIENT_SRCS = $(shell find $(CLIENT_DIR) -name '*.c' -not -path '*/include/*')
SERVER_SRCS = $(shell find $(SERVER_DIR) -name '*.c' -not -path '*/include/*')

# Generate object file names, preserving directory structure
CLIENT_OBJS = $(CLIENT_SRCS:$(CLIENT_DIR)/%.c=$(CLIENT_OBJ_DIR)/%.o)
SERVER_OBJS = $(SERVER_SRCS:$(SERVER_DIR)/%.c=$(SERVER_OBJ_DIR)/%.o)

# Output binaries
CLIENT_TARGET = $(BIN_DIR)/client
SERVER_TARGET = $(BIN_DIR)/server

.PHONY: all client server clean directories

all: directories client server

client: directories $(CLIENT_TARGET)

server: directories $(SERVER_TARGET)

$(CLIENT_TARGET): $(CLIENT_OBJS)
	$(CC) $(CLIENT_OBJS) -o $@ $(GTK_LDFLAGS) $(LDFLAGS)

$(SERVER_TARGET): $(SERVER_OBJS)
	$(CC) $(SERVER_OBJS) -o $@ $(LDFLAGS)

$(CLIENT_OBJ_DIR)/%.o: $(CLIENT_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(GTK_CFLAGS) -I$(CLIENT_INC_DIR) -c $< -o $@

$(SERVER_OBJ_DIR)/%.o: $(SERVER_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(SERVER_INC_DIR) -c $< -o $@

directories:
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(CLIENT_OBJ_DIR)
	@mkdir -p $(SERVER_OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
