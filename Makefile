# Compiler and flags
CC     := clang
CFLAGS := -Wall -Wextra -Werror -pedantic -pthread

# All source files
SRCS   := $(wildcard *.c)
OBJS   := $(filter-out unit_test.o httpserver.o, $(SRCS:.c=.o))


# Final executable name
TARGET := httpserver
TEST_TARGET := unit_test

# Default target
all: $(TARGET) $(TEST_TARGET)


# Link step: build server from all object files
$(TARGET): httpserver.o $(OBJS)
	$(CC) $(CFLAGS) -o $@ httpserver.o $(OBJS)


$(TEST_TARGET): unit_test.o $(OBJS)
	$(CC) $(CFLAGS) -o $@ unit_test.o $(OBJS)


# Compile each .c to .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Cleanup
clean:
	rm -f $(TARGET) $(TEST_TARGET) $(OBJS) unit_test.o httpserver.o
