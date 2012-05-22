# Configuration
-include ../.config

CC = gcc

SDK = gdmwimax
SDKLIB = lib$(SDK)
obj-sdk = device.o io.o hci.o wimax.o nds.o log.o\
	handle.o gctapi.o sdk.o timer.o msg.o error.o eap.o \
	 fload.o netlink_u.o pthread_sem.o

.EXPORT_ALL_VARIABLES:

CFLAGS += ${GFLAGS}
CFLAGS += -Wall
CFLAGS += -g -O0 -fPIC

ifeq ("$(CONFIG_TRANSPORT_TEST)", "y")
CFLAGS += -DTRANSPORT_TEST
endif

ifdef CONFIG_LOG_FILE_BUF_SIZE
CFLAGS += -DCONFIG_LOG_FILE_BUF_SIZE=$(CONFIG_LOG_FILE_BUF_SIZE)
endif

ifeq ("$(CONFIG_ENABLE_BW_SWITCHING_FOR_KT)", "y")
CFLAGS += -DCONFIG_ENABLE_BW_SWITCHING_FOR_KT=$(CONFIG_ENABLE_BW_SWITCHING_FOR_KT)
endif

ifeq ("$(CONFIG_ENABLE_SERVICE_FLOW)", "y")
obj-sdk += sf.o
CFLAGS += -DCONFIG_ENABLE_SERVICE_FLOW
endif

all: $(SDK)

$(SDK): $(obj-sdk)
	ar rscv $(SDKLIB).a $(obj-sdk)
#	$(CC) -fPIC -shared -W1,-soname,$(SDKLIB).so -o $(SDKLIB).so  $(obj-sdk)

clean:
	-rm -f $(SDK) *.a *.so *.elf *.gdb *.o disa nm
