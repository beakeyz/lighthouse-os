DIRECTORY_GUARD=mkdir -p $(@D)

CC := /bin/gcc
LD := /bin/ld

OUT := ./out
OUT_BINARY := base_env

SOURCE_FILES := ./main.c

OBJ_FILES := $(patsubst %.c,$(OUT)/%.o,$(SOURCE_FILES))

HEADER_FILES := ./def.h \
				./defaults.h

CFLAGS := -O2 \
		  -I./ \
		  -I./../../src/libs/libc/lightos

HEADER_DEPS := $(patsubst %.o,%.d,$(OBJ_FILES))

-include $(HEADER_DEPS)
$(OUT)/%.o: %.c
	@$(DIRECTORY_GUARD)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: build
build: $(OBJ_FILES)
	$(CC) -o $(OUT_BINARY) $(OBJ_FILES)

.PHONY: clean
clean:
	rm -rf $(OUT)

