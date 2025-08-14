CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pedantic -g -O
TARGET = trans
TEST_TARGET = test_encode
SOURCES = main.c encode.c network.c
TEST_SOURCES = test_encode.c encode.c
OBJECTS = $(SOURCES:.c=.o)
TEST_OBJECTS = $(TEST_SOURCES:.c=.o)
TEST_READ_PORT = 8080
TEST_WRITE_PORT = 8081

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

test_connect:
	./trans -m from -p $(TEST_READ_PORT) --ll -s "./trans -q -m to --lr -p $(TEST_WRITE_PORT)"

test_pty_connect:
	./trans -m from -p $(TEST_READ_PORT) --ll -s "ssh -tt -e none localhost 'stty raw -icanon -echo; $(PWD)/trans -q -m to --lr -p $(TEST_WRITE_PORT)'"

test_send:
	(ruby bin.rb | socat -u - TCP:localhost:$(TEST_READ_PORT) > /dev/null) & (socat -u "TCP-LISTEN:$(TEST_WRITE_PORT),reuseaddr" - < /dev/null | tee hoge.txt)

test_check:
	cat hoge.txt | od -t x1 -A n| ruby -l -0777 -ne '$$_.split.each_slice(7).each{|x|puts x.join(" ")}' | less
