LOCAL_PATH := $(call my-dir)

## Host ##
include $(CLEAR_VARS)
LOCAL_MODULE := stap
LOCAL_MODULE_TAGS := optional
LOCAL_CPP_EXTENSION:=.cxx
LOCAL_SRC_FILES := \
	main.cxx \
	session.cxx \
	parse.cxx \
	staptree.cxx \
	elaborate.cxx \
	translate.cxx \
	tapsets.cxx \
	buildrun.cxx \
	loc2c.c \
	hash.cxx \
	mdfour.c \
	cache.cxx \
	util.cxx \
	coveragedb.cxx \
	dwarf_wrappers.cxx \
	tapset-been.cxx \
	tapset-procfs.cxx \
	tapset-timers.cxx \
	tapset-netfilter.cxx \
	tapset-perfmon.cxx \
	tapset-mark.cxx \
	tapset-itrace.cxx \
	tapset-utrace.cxx \
	task_finder.cxx \
	dwflpp.cxx \
	rpm_finder.cxx \
	setupdwfl.cxx \
	remote.cxx \
	privilege.cxx \
	cmdline.cxx

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/includes \
	$(LOCAL_PATH)/android/host \
	$(LOCAL_PATH)/android/ \
	external/elfutils-redhat/ \
	external/elfutils-redhat/libdwfl \
	external/elfutils-redhat/libdw \
	external/elfutils-redhat/libelf

LOCAL_MODULE_CLASS := EXECUTABLES
intermediates := $(call local-intermediates-dir)
GEN:=$(addprefix $(intermediates)/,git_version.h)
$(GEN) : PRIVATE_CUSTOM_TOOL = external/systemtap/git_version.sh -k -s external/systemtap -o git_version.h ; mv git_version.h $@
$(GEN) : PRIVATE_PATH := $(LOCAL_PATH)
$(GEN) :
	$(transform-generated-source)
LOCAL_C_INCLUDES += $(intermediates)
LOCAL_GENERATED_SOURCES = $(GEN)

LOCAL_CFLAGS += \
	-DLOCALEDIR='""' \
	-DBINDIR='""' \
	-DPKGDATADIR='""' \
	-fexceptions \
	-Wall \
	-Wno-unused-parameter \
	-Wformat=2

LOCAL_LDLIBS += -lpthread -ldl
LOCAL_STATIC_LIBRARIES := libdwflrh libdwrh libeblrh libelfrh

include $(BUILD_HOST_EXECUTABLE)

## Target ##
include $(CLEAR_VARS)
LOCAL_MODULE := stapio
LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/android/target \
	$(LOCAL_PATH)/includes

LOCAL_CFLAGS := \
	-fno-strict-aliasing	\
	-fno-builtin-strftime \
	-DBINDIR='"/system/bin"' \
	-D_GNU_SOURCE=1

# Avoid stripping elf note sections by the toolchain.
LOCAL_LDFLAGS := -Wl,--no-gc-sections

LOCAL_SRC_FILES := \
	staprun/stapio.c	\
	staprun/mainloop.c	\
	staprun/common.c	\
	staprun/ctl.c	\
	staprun/relay.c \
	staprun/relay_old.c


#android/target/relay_old.c

LOCAL_LDLIBS += -lpthread

LOCAL_MODULE_CLASS := EXECUTABLES
intermediates := $(call local-intermediates-dir)
GEN:=$(addprefix $(intermediates)/,git_version.h)
$(GEN) : PRIVATE_CUSTOM_TOOL = external/systemtap/git_version.sh -k -s external/systemtap -o git_version.h ; mv git_version.h $@
$(GEN) : PRIVATE_PATH := $(LOCAL_PATH)
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
	$(LOCAL_PATH)/android/target \
	$(LOCAL_PATH)/includes

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
	staprun/staprun.c	\
	staprun/ctl.c	\
	staprun/staprun_funcs.c	\
	staprun/common.c	\
	android/glob.c \
	privilege.cxx \
	util.cxx

LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_C_INCLUDES += $(intermediates)
LOCAL_GENERATED_SOURCES = $(GEN)
# Small hack: Avoid stripping binary because:
# code that is in staprun dir will be built
# in the same dir. This confuses the build system
# at stripping time because that output dir has the
# same name as the binary.
# Another option would be to build staprun via Android.mk
# in staprun dir.
LOCAL_STRIP_MODULE := false

LOCAL_SHARED_LIBRARIES := liblog libstlport
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
