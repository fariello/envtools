export LC_ALL=C
export BAR_PATH=/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin:/usr/local/sbin:/no/such/dir:/repeated_dir:/repeated_dir:/bin:::F

CC            = gcc
COMMON_CFLAGS = -L./src -Wall -Wextra
DEBUG_CFLAGS  = $(COMMON_CFLAGS) -DDEBUG_ON -ggdb
REL_CFLAGS    = $(COMMON_CFLAGS) -O3 #-static -static-libgcc
BUILD_DIR     = ./build
TEST_DIR      = ./test
TEST_OUT_DIR  = $(TEST_DIR)/output
TEST_IN_DIR   = $(TEST_DIR)/input
TEST_DIRR_DIR = $(TEST_DIR)/diff

all: clean cleanpath unsetenvs

debug: clean debug-unsetenvs debug-cleanpath

cleanpath:
	$(CC) $(REL_CFLAGS) ./src/cleanpath.c -o ./bin/cleanpath

unsetenvs:
	$(CC) $(REL_CFLAGS) ./src/unsetenvs.c -o ./bin/unsetenvs

debug-cleanpath: clean
	mkdir -p $(TEST_OUT_DIR)
	$(CC) $(DEBUG_CFLAGS) ./src/cleanpath.c -o ./bin/cleanpath
	./bin/cleanpath NO_SUCH_1 NO_SUCH_2 PATH -D -v -v -v -v -v 2> $(TEST_OUT_DIR)/test1_out_err.txt
	./bin/cleanpath BAR_PATH -D -v -v -v -v -v -v 2> $(TEST_OUT_DIR)/test2_out_err.txt
	./bin/cleanpath -A -D -v -v -v -v -v 2> $(TEST_OUT_DIR)/test3_out_err.txt

debug-unsetenvs: clean
	mkdir -p $(TEST_OUT_DIR)
	$(CC) $(DEBUG_CFLAGS) ./src/unsetenvs.c -o ./bin/unsetenvs
	./bin/unsetenvs NO_SUCH_1 NO_SUCH_2 PATH -D -v -v -v -v -v 2> $(TEST_OUT_DIR)/test1_out_err.txt
	./bin/unsetenvs BAR_PATH -D -v -v -v -v -v -v 2> $(TEST_OUT_DIR)/test2_out_err.txt
	./bin/unsetenvs -A -D -v -v -v -v -v 2> $(TEST_OUT_DIR)/test3_out_err.txt

clean:
	rm -vf ./core ./bin/* ./bin/.??* ./cleanpath.o ./cleanpath.i ./cleanpath.s  ./unsetenvs.o ./unsetenvs.i ./unsetenvs.s
	rm -rvf $(TEST_OUT_DIR)
	find . -name '*~' -delete -print
	mkdir -p ./bin