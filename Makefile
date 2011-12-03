export LC_ALL=C
export BAR_PATH=/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin:/usr/local/sbin:/no/such/dir:/repeated_dir:/repeated_dir:/bin:::F

CC            = gcc
COMMON_CFLAGS = -static -L./src -Wall -Wextra
DEBUG_CFLAGS  = $(COMMON_CFLAGS) -m32 -DDEBUG_ON -ggdb -save-temps -Werror
REL_CFLAGS    = $(COMMON_CFLAGS) -m32 -O3 -static -static-libgcc #-static-libstdc++
BUILD_DIR     = ./build
TEST_DIR      = ./test
TEST_OUT_DIR  = $(TEST_DIR)/output
TEST_IN_DIR   = $(TEST_DIR)/input
TEST_DIRR_DIR = $(TEST_DIR)/diff

all: clean cpath
	$(CC) $(REL_CFLAGS) ./src/cpath.c -o ./bin/cpath

cpath:
	$(CC) $(REL_FLAGS) ./src/cpath.c -o ./bin/cpath

debug-cpath: clean
	mkdir -p $(TEST_OUT_DIR)
	$(CC) $(DEBUG_CFLAGS) ./src/cpath.c -o ./bin/cpath
	./bin/cpath NO_SUCH_1 NO_SUCH_2 PATH -D -v -v -v -v -v 2> $(TEST_OUT_DIR)/test1_out_err.txt
	./bin/cpath BAR_PATH -D -v -v -v -v -v -v 2> $(TEST_OUT_DIR)/test2_out_err.txt
	./bin/cpath -A -D -v -v -v -v -v 2> $(TEST_OUT_DIR)/test3_out_err.txt

clean:
	rm -vf ./core ./bin/* ./bin/.??* ./cpath.o ./cpath.i ./cpath.s
	rm -rvf $(TEST_OUT_DIR)
	find . -name '*~' -delete -print
	mkdir -p ./bin