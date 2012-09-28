/* -*- linux-c -*-
 * Copyright (C) 2005-2012 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */
#ifndef _STP_STRING_H_
#define _STP_STRING_H_

#define to_oct_digit(c) ((c) + '0')
static void _stp_text_str(char *out, char *in, int len, int quoted, int user);

/* XXX: duplication with loc2c-runtime.h */
/* XXX: probably needs the same set_fs / pagefault_* / bad_addr checks */

#if defined(__KERNEL__)

/*
 * Powerpc uses a paranoid user address check in __get_user() which
 * spews warnings "BUG: Sleeping function...." when DEBUG_SPINLOCK_SLEEP
 * is enabled. With 2.6.21 and above, a newer variant __get_user_inatomic
 * is provided without the paranoid check. Use it if available, fall back
 * to __get_user() if not. Other archs can use __get_user() as is
 */
#if defined(__powerpc__)
#ifdef __get_user_inatomic
#define __stp_get_user(x, ptr) __get_user_inatomic(x, ptr)
#else /* __get_user_inatomic */
#define __stp_get_user(x, ptr) __get_user(x, ptr)
#endif /* __get_user_inatomic */
#elif defined(__ia64__)
#define __stp_get_user(x, ptr)						\
  ({									\
     int __res;								\
     pagefault_disable();						\
     __res = __get_user(x, ptr);					\
     pagefault_enable();						\
     __res;								\
  })
#else /* !defined(__powerpc__) && !defined(__ia64) */
#define __stp_get_user(x, ptr) __get_user(x, ptr)
#endif /* !defined(__powerpc__) && !defined(__ia64) */

#elif defined(__DYNINST__)

#define __stp_get_user(x, ptr) __get_user(x, ptr)

#endif

#endif /* _STP_STRING_H_ */
