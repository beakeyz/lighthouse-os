DIRECTORY_GUARD=mkdir -p $(@D)

CC := /bin/gcc
LD := /bin/ld

OUT := ./out
OUT_BINARY := $(OUT)/lighthouse-os-tests

SOURCE_FILES := ./tests.c
TEST_SOURCE_FILES := $(shell find ./ -type f -name '*_test.c')
OBJ_FILES := $(patsubst %.c,$(OUT)/%.o,$(SOURCE_FILES) $(TEST_SOURCE_FILES))

HEADER_FILES := ./tests.h
TEST_HEADER_FILES := $(shell find ./ -type f -name '*_test.h')

CFLAGS := -O2   \
-I./ 			\
-I./../ 		\
-I./../src/aniva/ \


$(OUT)/%.o: %.c
	@$(DIRECTORY_GUARD)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: build
build: $(OBJ_FILES)
	$(CC) -o $(OUT_BINARY) $(OBJ_FILES)

.PHONY: clean
clean:
	rm -rf $(OUT)
