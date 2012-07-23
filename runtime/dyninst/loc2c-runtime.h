#include "../loc2c-runtime.h"

#define __get_user_asm(x, addr, err, itype, rtype, ltype, errret)	\
	__get_user((x), (typeof(x)*)(addr))

