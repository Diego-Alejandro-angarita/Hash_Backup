CC      = gcc
CFLAGS  = -Wall -Wextra -Iinclude -D_POSIX_C_SOURCE=199309L
LDFLAGS =

TARGET  = backup_app
SRC_DIR = src
OBJ_DIR = obj

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)
	rm -rf repo/ *.copy test_*.txt

benchmark: $(TARGET)
	./$(TARGET) --benchmark

.PHONY: clean benchmark