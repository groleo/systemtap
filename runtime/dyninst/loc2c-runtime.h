/* target operations in the Dyninst mode
 * Copyright (C) 2012 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _STAPDYN_LOC2C_RUNTIME_H_
#define _STAPDYN_LOC2C_RUNTIME_H_

#include "../loc2c-runtime.h"

#define __get_user_asm(x, addr, err, itype, rtype, ltype, errret)	\
	(err) = __get_user((x), (typeof(x)*)(addr))

#define __put_user_asm(x, addr, err, itype, rtype, ltype, errret)	\
	(err) = __put_user((x), (typeof(x)*)(addr))


#define u_fetch_register(regno) \
  pt_regs_fetch_register(c->uregs, regno)
#define u_store_register(regno, value) \
  pt_regs_store_register(c->uregs, regno, value)


#define uread(ptr) ({ \
	typeof(*(ptr)) _v = 0; \
	if (__copy_from_user((void *)&_v, (void *)(ptr), sizeof(*(ptr)))) \
	    DEREF_FAULT(ptr); \
	_v; \
    })

#define uwrite(ptr, value) ({ \
	typeof(*(ptr)) _v; \
	_v = (typeof(*(ptr)))(value); \
	if (__copy_to_user((void *)(ptr), (void *)&_v, sizeof(*(ptr)))) \
	    STORE_DEREF_FAULT(ptr); \
    })

#define uderef(size, addr) ({ \
    intptr_t _i = 0; \
    switch (size) { \
	case 1: _i = uread((u8 *)(addr)); break; \
	case 2: _i = uread((u16 *)(addr)); break; \
	case 4: _i = uread((u32 *)(addr)); break; \
	case 8: _i = uread((u64 *)(addr)); break; \
	default: __get_user_bad(); \
    } \
    _i; \
  })

#define store_uderef(size, addr, value) ({ \
    switch (size) { \
	case 1: uwrite((u8 *)(addr), (value)); break; \
	case 2: uwrite((u16 *)(addr), (value)); break; \
	case 4: uwrite((u32 *)(addr), (value)); break; \
	case 8: uwrite((u64 *)(addr), (value)); break; \
	default: __put_user_bad(); \
    } \
  })


/* We still need to clean the runtime more before these can go away... */
#define kread uread
#define kwrite uwrite
#define kderef uderef
#define store_kderef store_uderef


#endif /* _STAPDYN_LOC2C_RUNTIME_H_ */
