LOCAL_PATH := $(call my-dir)
# prebuilt libncurses
include $(CLEAR_VARS)

LOCAL_MODULE := ncurses
LOCAL_SRC_FILES := ../../android-libncurses/libs/$(TARGET_ARCH_ABI)/libncurses.so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../include/

include $(PREBUILT_SHARED_LIBRARY)

# prebuilt libssl
include $(CLEAR_VARS)

LOCAL_MODULE := ssl
LOCAL_SRC_FILES := ../../android-openssl/libs/$(TARGET_ARCH_ABI)/libssl.so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../include/

include $(PREBUILT_SHARED_LIBRARY)

# prebuilt libcrypto
include $(CLEAR_VARS)

LOCAL_MODULE := crypto
LOCAL_SRC_FILES := ../../android-openssl/libs/$(TARGET_ARCH_ABI)/libcrypto.so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../include/

include $(PREBUILT_SHARED_LIBRARY)

# prebuilt libiconv
include $(CLEAR_VARS)

LOCAL_MODULE := iconv
LOCAL_SRC_FILES := ../../android-libiconv-1.14/libs/$(TARGET_ARCH_ABI)/libiconv.so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../include/

include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
prefix = /usr/local
exec_prefix = ${prefix}
sysconfdir = ${prefix}/etc
datarootdir = ${prefix}/share
LIBX3270DIR = ${sysconfdir}/x3270
MANDIR = ${datarootdir}/man
BINDIR = ${exec_prefix}/bin

L_DEFS := -DUSE_TERMIO \
	-DLIBX3270DIR=\"$(LIBX3270DIR)\"
L_CFLAGS := $(L_DEFS)

LOCAL_SRC_FILES := \
	actions.c \
	ansi.c \
	apl.c \
	c3270.c \
	charset.c \
	child.c \
	ctlr.c \
        fallbacks.c \
        ft.c \
	ft_cut.c \
	ft_dft.c \
	glue.c \
	help.c \
	host.c \
	icmd.c \
	idle.c \
	keymap.c \
        kybd.c \
	macros.c \
	print.c \
	printer.c \
	proxy.c \
	readres.c \
	resolver.c \
        resources.c \
	rpq.c \
	screen.c \
	see.c \
	sf.c \
	tables.c \
	telnet.c \
	toggles.c \
        trace_ds.c \
	unicode.c \
	unicode_dbcs.c \
	utf8.c \
	util.c \
	version.c \
	xio.c \
	XtGlue.c \
        menubar.c \
	keypad.c

LOCAL_MODULE := c3270
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := $(L_CFLAGS)
LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../../android-openssl/jni/include/ \
	$(LOCAL_PATH)/../../android-libiconv-1.14/jni/include \
	$(LOCAL_PATH)/../../android-libncurses/jni/include

LOCAL_SHARED_LIBRARIES += libc libssl libcrypto libncurses libiconv

include $(BUILD_EXECUTABLE)
