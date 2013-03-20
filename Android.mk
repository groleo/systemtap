LOCAL_PATH := $(call my-dir)
LOCAL_PATH_BKP := $(LOCAL_PATH)
# go build target bins
include $(call all-subdir-makefiles)
LOCAL_PATH := $(LOCAL_PATH_BKP)

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
	tapset-dynprobe.cxx \
	re2c-migrate/re2c-dfa.cxx \
	re2c-migrate/re2c-emit.cxx \
	re2c-migrate/re2c-globals.cxx \
	re2c-migrate/re2c-regex.cxx \
	re2c-migrate/stapregex.cxx \
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
$(GEN) : PRIVATE_CUSTOM_TOOL = external/systemtap/git_version.sh -k -s external/systemtap -o $@
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
LOCAL_SHARED_LIBRARIES := libdwflrh libdwrh libeblrh libelfrh

include $(BUILD_HOST_EXECUTABLE)
