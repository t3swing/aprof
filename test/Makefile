
ifeq ($(platform), arm)
CC = arm-hisiv600-linux-gcc
CFLAGS := -DPLATFORM_ARM
else
CC = gcc
CFLAGS := -DPLATFORM_X86
endif

CFLAGS := -Wall -Wno-unused 
CFLAGS += -I ./include

LDFLAGS += -lpthread

TARGET := test

all:$(TARGET)

SRCS := test.c
OBJS := $(SRCS:%.c=%.o)

$(TARGET):$(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(CFLAGS)

clean:
	@rm -f $(OBJS)
	@rm -f $(TARGET)
