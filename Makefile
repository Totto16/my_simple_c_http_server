CC = gcc
CFLAGS =  -D_POSIX_C_SOURCE -D_BSD_SOURCE  -std=c11 -Wall -Wextra -Wpedantic -pthread
OFLAG = -O3

.PHONY: all
all: task3

.PHONY: clean
clean:
	$(RM) task3

%: %.c
	$(CC) $(CFLAGS) $(OFLAG) -o $@ $^
