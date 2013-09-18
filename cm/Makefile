# Configuration
-include ../.config

CC ?= gcc

SDK_LIB = ../sdk/libgdmwimax.a
SDK_LNK = -lpthread -lm -lrt
CM = cm
LIBS = $(SDK_LNK) $(SDK_LIB)
obj-cm = main.o cm.o cmd.o ind.o profilestring.o exitcb.o sem.o \
	cm_timer.o cm_msg.o dhclient.o

.EXPORT_ALL_VARIABLES:

CFLAGS ?= -g -O2
CFLAGS += -Wall

ifeq ("$(CONFIG_DM_INTERFACE)", "y")
CPPFLAGS += -DCONFIG_DM_INTERFACE
CPPFLAGS += -DCONFIG_DM_NET_DEVICE='"$(CONFIG_DM_NET_DEVICE)"'
obj-cm += dm_if.o
endif

ifdef CONFIG_LOG_FILE_BUF_SIZE
CPPFLAGS += -DCONFIG_LOG_FILE_BUF_SIZE=$(CONFIG_LOG_FILE_BUF_SIZE)
endif

ifeq ("$(CONFIG_ENABLE_SERVICE_FLOW)", "y")
CPPFLAGS += -DCONFIG_ENABLE_SERVICE_FLOW
endif

all: $(CM)

$(CM): $(obj-cm) $(SDK_LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(obj-cm) $(LIBS)

clean:
	-rm -f $(CM) *.a *.so *.elf *.gdb *.o
