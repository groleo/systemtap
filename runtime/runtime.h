/* main header file
 * Copyright (C) 2005-2012 Red Hat Inc.
 * Copyright (C) 2005-2006 Intel Corporation.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _RUNTIME_H_
#define _RUNTIME_H_


#if defined(__KERNEL__)

#include "linux/runtime.h"

#elif defined(__DYNINST__)

#include "dyninst/runtime.h"

#endif


#endif /* _RUNTIME_H_ */
