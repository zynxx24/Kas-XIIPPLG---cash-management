# XII PPLG Cash Management - C Server
# Build with: make
# Clean with: make clean
# Run with:   ./server

CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread -std=c11 -D_GNU_SOURCE
LDFLAGS = -pthread -lm
TARGET = server

SRCS = main.c server.c router.c api.c database.c auth.c security.c cJSON.c sha256.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo ""
	@echo "Build successful! Run with: ./server"
	@echo ""

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

# Dependencies
main.o: main.c server.h router.h database.h auth.h security.h
server.o: server.c server.h router.h security.h
router.o: router.c router.h api.h server.h
api.o: api.c api.h auth.h database.h security.h cJSON.h server.h
database.o: database.c database.h cJSON.h
auth.o: auth.c auth.h database.h cJSON.h sha256.h
security.o: security.c security.h cJSON.h
cJSON.o: cJSON.c cJSON.h
sha256.o: sha256.c sha256.h
