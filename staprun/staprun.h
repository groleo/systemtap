/* -*- linux-c -*-
 *
 * staprun.h - include file for staprun and stapio
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 *
 * Copyright (C) 2005-2008 Red Hat Inc.
 */
#define _FILE_OFFSET_BITS 64

/* kernel-headers and glibc like to stomp on each other.  We include this early
 * so we can ensure glibc's own definitions will win.  rhbz 837641 & 840902 */
#include <linux/posix_types.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <linux/fd.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <pthread.h>
#include <sys/socket.h>
#include <linux/limits.h>
#include <sys/wait.h>
#include <sys/statfs.h>
#include <syslog.h>

/* Include config.h to pick up dependency for --prefix usage. */
#include "config.h"
#include "../privilege.h"

#include <sched.h>

#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif


/* define gettext options if NLS is set */
#if ENABLE_NLS
#define _(string) gettext(string)
#define _N(string, string_plural, count) \
        ngettext((string), (string_plural), (count))
#else
#define _(string) (string)
#define _N(string, string_plural, count) \
        ( (count) == 1 ? (string) : (string_plural) )
#endif
/* NB: _F and _NF not available for staprun, since autosprintf() is defined
   in c++ in ../util.cxx, in a memory-leak-free way. */
#if 0
#define _F(format, ...) autosprintf(_(format), __VA_ARGS__)
#define _NF(format, format_plural, count, ...) \
        autosprintf(_N((format), (format_plural), (count)), __VA_ARGS__)
#endif

/* For probes in staprun.c, staprun_funcs.c, mainloop.c and common.c */
#include "stap-probe.h"

extern void eprintf(const char *fmt, ...);
extern void switch_syslog(const char *name);

#define dbug(level, args...) do {if (verbose>=level) {eprintf("%s:%s:%d ",__name__,__FUNCTION__, __LINE__); eprintf(args);}} while (0)

extern char *__name__;

/* print to stderr */
#define err(args...) eprintf(args)

/* better perror() */
#define perr(args...) do {					\
		int _errno = errno;				\
		eprintf("ERROR: ");				\
		eprintf(args);					\
		eprintf(": %s\n", strerror(_errno));		\
	} while (0)

/* Error messages. Use these for serious errors, not informational messages to stderr. */
#define _err(args...) do {eprintf("%s:%s:%d: ERROR: ",__name__, __FUNCTION__, __LINE__); eprintf(args);} while(0)
#define _perr(args...) do {					\
		int _errno = errno;				\
		_err(args);					\
		eprintf(": %s\n", strerror(_errno));	\
	} while (0)
#define overflow_error() _err("Internal buffer overflow. Please file a bug report.\n")

/* Error checking version of sprintf() - returns 1 if overflow error */
#define sprintf_chk(str, args...) ({			\
	int _rc;					\
	_rc = snprintf(str, sizeof(str), args);		\
	if (_rc >= (int)sizeof(str)) {			\
		overflow_error();			\
		_rc = 1;				\
	}						\
	else						\
		_rc = 0;				\
	_rc;						\
})

/* Error checking version of snprintf() - returns 1 if overflow error */
#define snprintf_chk(str, size, args...) ({		\
	int _rc;					\
	_rc = snprintf(str, size, args);		\
	if (_rc >= (int)size) {				\
		overflow_error();			\
		_rc = 1;				\
	}						\
	else						\
		_rc = 0;				\
	_rc;						\
})

/* Grabbed from linux/module.h kernel include. */
#define MODULE_NAME_LEN (64 - sizeof(unsigned long))

/* We define this so we are compatible with old transport, but we
 * don't have to use it. */
#define STP_TRANSPORT_VERSION 1
#include "../runtime/transport/transport_msgs.h"

#define RELAYFS_MAGIC	0xF0B4A981
#define DEBUGFS_MAGIC	0x64626720
#define DEBUGFSDIR	"/sys/kernel/debug"
#define RELAYFSDIR	"/mnt/relay"

/*
 * function prototypes
 */
int init_staprun(void);
int init_stapio(void);
int stp_main_loop(void);
int send_request(int type, void *data, int len);
void cleanup_and_exit (int, int);
int init_ctl_channel(const char *name, int verb);
void close_ctl_channel(void);
int init_relayfs(void);
void close_relayfs(void);
int init_oldrelayfs(void);
void close_oldrelayfs(int);
int write_realtime_data(void *data, ssize_t nb);
void setup_signals(void);
int make_outfile_name(char *buf, int max, int fnum, int cpu,
		      time_t t, int bulk);
int init_backlog(int cpu);
void write_backlog(int cpu, int fnum, time_t t);
time_t read_backlog(int cpu, int fnum);
/* staprun_funcs.c */
void setup_staprun_signals(void);
const char *moderror(int err);

/* insert_module helper functions.  */
typedef void (*assert_permissions_func) (
  const char *module_path __attribute__ ((unused)),
  int module_fd __attribute__ ((unused)),
  const void *module_data __attribute__ ((unused)),
  off_t module_size __attribute__ ((unused)),
  privilege_t *user_credentials __attribute__ ((unused))
);

void assert_stap_module_permissions (
  const char *module_path __attribute__ ((unused)),
  int module_fd __attribute__ ((unused)),
  const void *module_data __attribute__ ((unused)),
  off_t module_size __attribute__ ((unused)),
  privilege_t *user_credentials __attribute__ ((unused))
);

void assert_uprobes_module_permissions (
  const char *module_path __attribute__ ((unused)),
  int module_fd __attribute__ ((unused)),
  const void *module_data __attribute__ ((unused)),
  off_t module_size __attribute__ ((unused)),
  privilege_t *user_credentials __attribute__ ((unused))
);

int insert_module(const char *path, const char *special_options,
		  char **options, assert_permissions_func apf,
		  privilege_t *user_credentials
);

int rename_module(void* module_file, const __off_t st_size);

int mountfs(void);
void start_symbol_thread(void);
void stop_symbol_thread(void);

/* common.c functions */
int stap_strfloctime(char *buf, size_t max, const char *fmt, time_t t);
void parse_args(int argc, char **argv);
void usage(char *prog);
void parse_modpath(const char *);
void setup_signals(void);
int set_clexec(int fd);

/*
 * variables
 */
extern int control_channel;
extern int ncpus;
extern int initialized;
extern int kernel_ptr_size;

/* flags */
extern int verbose;
extern int suppress_warnings;
extern unsigned int buffer_size;
extern unsigned int reader_timeout_ms;
extern char *modname;
extern char *modpath;
#define MAXMODOPTIONS 64
extern char *modoptions[MAXMODOPTIONS];
extern int target_pid;
extern char *target_cmd;
extern char *outfile_name;
extern int rename_mod;
extern int attach_mod;
extern int delete_mod;
extern int load_only;
extern int need_uprobes;
extern const char *uprobes_path;
extern int daemon_mode;
extern off_t fsize_max;
extern int fnum_max;
extern int remote_id;
extern const char *remote_uri;
extern int relay_basedir_fd;

/* getopt variables */
extern char *optarg;
extern int optopt;
extern int optind;

/* maximum number of CPUs we can handle */
#define NR_CPUS 256

/* relay*.c uses these */
extern int out_fd[NR_CPUS];

/* relay_old uses these. Set in ctl.c */
extern unsigned subbuf_size;
extern unsigned n_subbufs;

/*
 * ppoll exists in glibc >= 2.4
 */
#if (__GLIBC__ < 2) || ((__GLIBC__ == 2) && (__GLIBC_MINOR__ < 4))
#define NEED_PPOLL

extern int ppoll(struct pollfd *fds, nfds_t nfds,
		 const struct timespec *timeout, const sigset_t *sigmask);
#endif
