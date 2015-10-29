
include config.mk

ifeq ($(ANDROID), 1)
ARCH = arm
STATIC = 1
endif

ifeq ($(ARCH), arm)

ifndef TOOLCHAIN
endif
ifndef PREFIX
endif

TOOL_PATH = $(TOOLCHAIN)/bin/

CC = $(TOOL_PATH)$(PREFIX)gcc
CXX = $(TOOL_PATH)$(PREFIX)g++
LD = $(TOOL_PATH)$(PREFIX)ld
AR = $(TOOL_PATH)$(PREFIX)ar
RANLIB = $(TOOL_PATH)$(PREFIX)ranlib
AS = $(TOOL_PATH)$(PREFIX)as
NM = $(TOOL_PATH)$(PREFIX)nm
STRIP = $(TOOL_PATH)$(PREFIX)strip
OBJCOPY = $(TOOL_PATH)$(PREFIX)objcopy
OBJDUMP = $(TOOL_PATH)$(PREFIX)objdump

CFLAGS = -mthumb -march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16

else
ARCH = host

CC = gcc
CXX = g++
LD = ld
AR = ar
RANLIB = ranlib
AS = as
NM = nm
STRIP = strip
OBJCOPY = objcopy
OBJDUMP = objdump

CFLAGS = -L/usr/local/lib -lssl -levent -lcrypto -shared
endif

DTP_TOP_DIR=$(PWD)
SRC_DIR=$(DTP_TOP_DIR)/src
ZLIB_DIR=$(DTP_TOP_DIR)/third-party/zlib-1.2.8
OPENSSL_DIR=$(DTP_TOP_DIR)/third-party/openssl-1.0.1e
LIBEVENT_DIR=$(DTP_TOP_DIR)/third-party/libevent-2.0.21-stable

USR_PATH=$(DTP_TOP_DIR)/arch/$(ARCH)
OBJ_PATH=$(USR_PATH)/obj

ifeq ($(STATIC), 1)
INCLUDE_PATH=$(USR_PATH)/include
LDLIB_PATH=$(USR_PATH)/lib
else
INCLUDE_PATH=$(DTP_TOP_DIR)/usr/include
endif

LIBZ_A=$(LDLIB_PATH)/libz.a
LIBSSL_A=$(LDLIB_PATH)/libssl.a
LIBCRYPTO_A=$(LDLIB_PATH)/libcrypto.a
LIBEVENT_A=$(LDLIB_PATH)/libevent.a
LIBEVENT_CORE_A=$(LDLIB_PATH)/libevent_core.a
LIBEVENT_EXTRA_A=$(LDLIB_PATH)/libevent_extra.a
LIBEVENT_OPENSSL_A=$(LDLIB_PATH)/libevent_openssl.a
LIBEVENT_PTHREADS_A=$(LDLIB_PATH)/libevent_pthreads.a

LIB_OBJS = \
		dtp_transport.o \
		dtp.o \
		dtp_log.o \
		dtp_mobile.o \
		dtp_retrans_queue.o \
		dtp_socket.o \
		dtp_select.o \
		context.o \
		crypt.o \
		scheduler.o \
		debug.o

DTP_CFLAGS = -Wall -g -ggdb -pthread -DREENTRANT -O2 # -fno-strict-aliasing #-DCONFIG_EMBDED -DUSE_IND_THREAD 
ifeq ($(ANDROID), 1)
	DTP_CFLAGS += -DANDROID \
				  -DIN_MOBILE \
				  -DNO_FDSET_EXTENSION
	LIB_OBJS += ifaddrs.o
endif
CPPFLAGS = -I$(INCLUDE_PATH)

# all
all: dtp_library

$(OBJ_PATH)/%.o: $(SRC_DIR)/%.c $(OBJ_PATH)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $(DTP_CFLAGS) -fPIC $< -o $@

$(OBJ_PATH):
	mkdir -p $(OBJ_PATH)

ifeq ($(STATIC), 1)
dtp_library: libz_obj libssl_obj libcrypto_obj libevent_obj libevent_core_obj libevent_extra_obj libevent_openssl_obj libevent_pthreads_obj $(addprefix $(OBJ_PATH)/, $(LIB_OBJS))
	cd $(OBJ_PATH); \
	$(AR) rvs libdtp.a \
		$(LIB_OBJS) \
		$(OBJ_PATH)/libz_obj/*.o \
		$(OBJ_PATH)/libssl_obj/*.o \
		$(OBJ_PATH)/libcrypto_obj/*.o \
		$(OBJ_PATH)/libevent_obj/*.o \
		$(OBJ_PATH)/libevent_core_obj/*.o \
		$(OBJ_PATH)/libevent_extra_obj/*.o \
		$(OBJ_PATH)/libevent_openssl_obj/*.o \
		$(OBJ_PATH)/libevent_pthreads_obj/*.o
	cp $(OBJ_PATH)/libdtp.a .
else
dtp_library: $(addprefix $(OBJ_PATH)/, $(LIB_OBJS))
	cd $(OBJ_PATH); \
	$(AR) rvs libdtp.a $(LIB_OBJS)
	cp $(OBJ_PATH)/libdtp.a .
endif

$(LIBZ_A):
	cd $(ZLIB_DIR) && \
	CC=$(CC) AR=$(AR) RANLIB=$(RANLIB) AS=$(AS) NM=$(NM) \
	STRIP=$(STRIP) OBJCOPY=$(OBJCOPY) OBJDUMP=$(OBJDUMP) CFLAGS="$(CFLAGS)" \
	./configure --prefix=$(USR_PATH) && make && make install

$(LIBSSL_A): $(LIBZ_A)
	cd $(OPENSSL_DIR) && \
	CC=$(CC) AR=$(AR) RANLIB=$(RANLIB) CFLAGS="$(CFLAGS)" \
	AS=$(AS) NM=$(NM) STRIP=$(STRIP) OBJCOPY=$(OBJCOPY) OBJDUMP=$(OBJDUMP) \
	./Configure $(if ifeq "$(ARCH)" "arm", linux-elf-arm) --prefix=$(USR_PATH) --openssldir=$(USR_PATH) -L$(USR_PATH)/liblib -ldl no-threads no-asm && \
	make && make install

$(LIBEVENT_A):
	cd $(LIBEVENT_DIR) && \
	AS=$(AS) NM=$(NM) STRIP=$(STRIP) OBJCOPY=$(OBJCOPY) OBJDUMP=$(OBJDUMP) \
	./configure $(if ifeq "$(ARCH)" "arm", --host=arm) CC=$(CC) CXX=$(CXX) --prefix=$(USR_PATH) LDFLAGS=-L$(LDLIB_PATH) CPPFLAGS=-I$(INCLUDE_PATH) CFLAGS="$(CFLAGS)" && \
	make && make install

$(LIBCRYPTO_A): $(LIBSSL_A)

$(LIBEVENT_CORE_A): $(LIBEVENT_A)

$(LIBEVENT_EXTRA_A): $(LIBEVENT_A)

$(LIBEVENT_OPENSSL_A): $(LIBEVENT_A)

$(LIBEVENT_PTHREADS_A): $(LIBEVENT_A)

define extract_obj =
	mkdir -p $(OBJ_PATH)/$@
	cd $(OBJ_PATH)/$@; \
	$(AR) x $<
endef

libz_obj: $(LIBZ_A) $(OBJ_PATH)
	mkdir -p $(OBJ_PATH)/$@
	cd $(OBJ_PATH)/$@; \
	$(AR) x $<

libssl_obj: $(LIBSSL_A) $(OBJ_PATH)
	mkdir -p $(OBJ_PATH)/$@
	cd $(OBJ_PATH)/$@; \
	$(AR) x $<

libcrypto_obj: $(LIBCRYPTO_A) $(OBJ_PATH)
	mkdir -p $(OBJ_PATH)/$@
	cd $(OBJ_PATH)/$@; \
	$(AR) x $<

libevent_obj: $(LIBEVENT_A) $(OBJ_PATH)
	mkdir -p $(OBJ_PATH)/$@
	cd $(OBJ_PATH)/$@; \
	$(AR) x $<

libevent_core_obj: $(LIBEVENT_CORE_A) $(OBJ_PATH)
	mkdir -p $(OBJ_PATH)/$@
	cd $(OBJ_PATH)/$@; \
	$(AR) x $<

libevent_extra_obj: $(LIBEVENT_EXTRA_A) $(OBJ_PATH)
	mkdir -p $(OBJ_PATH)/$@
	cd $(OBJ_PATH)/$@; \
	$(AR) x $<

libevent_openssl_obj: $(LIBEVENT_OPENSSL_A) $(OBJ_PATH)
	mkdir -p $(OBJ_PATH)/$@
	cd $(OBJ_PATH)/$@; \
	$(AR) x $<

libevent_pthreads_obj: $(LIBEVENT_PTHREADS_A) $(OBJ_PATH)
	mkdir -p $(OBJ_PATH)/$@
	cd $(OBJ_PATH)/$@; \
	$(AR) x $<

# clean
clean:
	rm -rf libdtp.a $(addprefix $(OBJ_PATH)/, $(LIB_OBJS) libdtp.a)

libclean:
	cd $(ZLIB_DIR) && make clean
	cd $(OPENSSL_DIR) && make clean
	cd $(LIBEVENT_DIR) && make clean
	rm -f $(LIBZ_A) $(LIBSSL_A) $(LIBEVENT_A)

distclean: clean libclean
	rm -rf $(DTP_TOP_DIR)/arch

