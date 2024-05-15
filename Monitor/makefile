CC = gcc -g

# Needed for GLib 2.0 (use of dictionary components): -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include
# Needed for thread management -pthread
# -Wno-unknown-pragmas not show warning for unknown pragmas
CFLAGS = -Wall -Wextra -Wno-unknown-pragmas -std=c99 -pthread -Wformat-truncation=0 -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include 

# Needed for GLib 2.0 (use of dictionary components): -lglib-2.0 
# lm is needed for shared memory
LDFLAGS = -lm -lglib-2.0

SRC_DIR = .
OBJ_DIR = ../obj_mon
BIN_DIR = ../bin

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
EXEC = $(BIN_DIR)/Monitor

LIBS = -pthread -lrt

.PHONY: all clean

all: $(EXEC)

$(EXEC): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(OBJS) $(LIBS) $(LDFLAGS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) -r $(OBJ_DIR)
# Removed $(BIN_DIR)