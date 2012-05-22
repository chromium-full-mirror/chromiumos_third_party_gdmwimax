# Configuration
-include ../.config

CC = gcc
OBJDUMP = objdump

SDK_LIB = ../sdk/libgdmwimax.a
SDK_LNK = -lpthread -lm -lrt -lz
CM = cm
LIBS = $(SDK_LNK) $(SDK_LIB)
obj-cm = main.o cm.o cmd.o ind.o profilestring.o exitcb.o sem.o \
	cm_timer.o cm_msg.o dhclient.o

.EXPORT_ALL_VARIABLES:

CFLAGS += ${GFLAGS}
CFLAGS += -Wall
CFLAGS += -g -O0

ifeq ("$(CONFIG_DM_INTERFACE)", "y")
CFLAGS += -DCONFIG_DM_INTERFACE
CFLAGS += -DCONFIG_DM_NET_DEVICE="\""$(CONFIG_DM_NET_DEVICE)""\"
obj-cm += dm_if.o
endif

ifdef CONFIG_LOG_FILE_BUF_SIZE
CFLAGS += -DCONFIG_LOG_FILE_BUF_SIZE=$(CONFIG_LOG_FILE_BUF_SIZE)
endif

ifeq ("$(CONFIG_ENABLE_SERVICE_FLOW)", "y")
CFLAGS += -DCONFIG_ENABLE_SERVICE_FLOW
endif

all: $(CM)

$(CM): $(obj-cm) $(SDK_LIB)
	$(CC) -o $@ $(obj-cm) $(LIBS)
	$(OBJDUMP) -d $@ > disa

clean:
	-rm -f $(CM) *.a *.so *.elf *.gdb *.o disa
