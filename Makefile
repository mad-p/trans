CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pedantic -g
TARGET = trans
TEST_TARGET = test_encode
SOURCES = main.c encode.c network.c
TEST_SOURCES = test_encode.c encode.c
OBJECTS = $(SOURCES:.c=.o)
TEST_OBJECTS = $(TEST_SOURCES:.c=.o)

.PHONY: all clean test install

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_TARGET): $(TEST_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c trans.h
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -f $(OBJECTS) $(TEST_OBJECTS) $(TARGET) $(TEST_TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

# デバッグ用ターゲット
debug: CFLAGS += -DDEBUG
debug: $(TARGET)

# リリース用ターゲット
release: CFLAGS += -O2 -DNDEBUG
release: clean $(TARGET)

# ヘルプ
help:
	@echo "Available targets:"
	@echo "  all      - Build the main program (default)"
	@echo "  test     - Build and run unit tests"
	@echo "  clean    - Remove all built files"
	@echo "  install  - Install the program to /usr/local/bin"
	@echo "  debug    - Build with debug flags"
	@echo "  release  - Build optimized release version"
	@echo "  help     - Show this help message"

test_recv:
	./trans -m from -p 8080 -s cat

test_send:
	ruby bin.rb | ./trans -m to -p 8080 | tee hoge.txt

test_check:
	cat hoge.txt | od -t x1 -A n| ruby -l -0777 -ne '$$_.split.each_slice(7).each{|x|puts x.join(" ")}' | less
