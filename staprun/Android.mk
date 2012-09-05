LOCAL_PATH := $(call my-dir)
STAP_ROOT_PATH := $(LOCAL_PATH)/..

## Target ##
include $(CLEAR_VARS)
LOCAL_MODULE := stapio
LOCAL_C_INCLUDES += \
	$(STAP_ROOT_PATH)/android/target \
	$(STAP_ROOT_PATH)/includes \
	$(STAP_ROOT_PATH)

LOCAL_CFLAGS := \
	-fno-strict-aliasing	\
	-fno-builtin-strftime \
	-DBINDIR='"/system/bin"' \
	-D_GNU_SOURCE=1

# Avoid stripping elf note sections by the toolchain.
LOCAL_LDFLAGS := -Wl,--no-gc-sections

LOCAL_SRC_FILES := \
	stapio.c	\
	mainloop.c	\
	common.c	\
	ctl.c		\
	relay.c 	\
	relay_old.c

LOCAL_LDLIBS += -lpthread

LOCAL_MODULE_CLASS := EXECUTABLES
intermediates := $(call local-intermediates-dir)
GEN:=$(addprefix $(intermediates)/,git_version.h)
$(GEN) : PRIVATE_CUSTOM_TOOL = external/systemtap/git_version.sh -k -s external/systemtap -o git_version.h ; mv git_version.h $@
$(GEN) : PRIVATE_PATH := $(STAP_ROOT_PATH)
$(GEN) :
	$(transform-generated-source)
LOCAL_C_INCLUDES += $(intermediates)
LOCAL_GENERATED_SOURCES = $(GEN)

LOCAL_SHARED_LIBRARIES := liblog
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := staprun
LOCAL_C_INCLUDES += \
	bionic/libstdc++/include \
	bionic \
	external/stlport/stlport \
	$(STAP_ROOT_PATH)/android/target \
	$(STAP_ROOT_PATH)/includes \
	$(STAP_ROOT_PATH)

LOCAL_CFLAGS := \
	-fno-strict-aliasing	\
	-fno-builtin-strftime -D_FILE_OFFSET_BITS=64 \
	-DSINGLE_THREADED \
	-DPKGDATADIR='"/system/bin"'	\
	-DPKGLIBDIR='"/system/bin"' \
	-DUSTL_ANDROID_X86

LOCAL_CPPFLAGS := -fexceptions

LOCAL_LDFLAGS := -Wl,--no-gc-sections
LOCAL_CPP_EXTENSION:=.cxx

LOCAL_SRC_FILES := \
	staprun.c	\
	ctl.c	\
	staprun_funcs.c	\
	common.c	\
	../android/glob.c \
	../privilege.cxx \
	../util.cxx

LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_C_INCLUDES += $(intermediates)
LOCAL_GENERATED_SOURCES = $(GEN)

LOCAL_SHARED_LIBRARIES := liblog libstlport
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
