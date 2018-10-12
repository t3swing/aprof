
#CC = gcc
ifeq ($(platform), arm)
CC = arm-hisiv400-linux-gcc
CFLAGS := -DPLATFORM_ARM
else
CC = gcc
CFLAGS := -DPLATFORM_X86
endif

CFLAGS += -g -Wall -Wextra -Wno-unused-parameter -fstack-protector-all
CFLAGS += -I ./include

LDFLAGS += -lm -ldl -lpthread

TARGET := aprof

all:$(TARGET)

SRCS := aprof.c readSym.c misc.c
OBJS := $(SRCS:%.c=%.o)

$(TARGET):$(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(CFLAGS)

clean:
	@rm -f $(OBJS)
	@rm -f $(TARGET)
