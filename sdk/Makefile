# Configuration
-include ../.config

CC ?= gcc
AR ?= ar

SDK = gdmwimax
SDKLIB = lib$(SDK)
obj-sdk = device.o io.o hci.o wimax.o nds.o log.o \
	handle.o gctapi.o sdk.o timer.o msg.o error.o eap.o \
	fload.o netlink_u.o pthread_sem.o

.EXPORT_ALL_VARIABLES:

CFLAGS ?= -g -O2
CFLAGS += -Wall -fPIC

ifeq ("$(CONFIG_TRANSPORT_TEST)", "y")
CPPFLAGS += -DTRANSPORT_TEST
endif

ifdef CONFIG_LOG_FILE_BUF_SIZE
CPPFLAGS += -DCONFIG_LOG_FILE_BUF_SIZE=$(CONFIG_LOG_FILE_BUF_SIZE)
endif

ifeq ("$(CONFIG_ENABLE_BW_SWITCHING_FOR_KT)", "y")
CPPFLAGS += -DCONFIG_ENABLE_BW_SWITCHING_FOR_KT=$(CONFIG_ENABLE_BW_SWITCHING_FOR_KT)
endif

ifeq ("$(CONFIG_ENABLE_SERVICE_FLOW)", "y")
obj-sdk += sf.o
CPPFLAGS += -DCONFIG_ENABLE_SERVICE_FLOW
endif

all: $(SDKLIB).a

$(SDKLIB).a: $(obj-sdk)
	$(AR) rscv $@ $^

clean:
	-rm -f *.a *.so *.elf *.gdb *.o
