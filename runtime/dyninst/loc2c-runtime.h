#include "../loc2c-runtime.h"

#define __get_user_asm(x, addr, err, itype, rtype, ltype, errret)	\
	(err) = __get_user((x), (typeof(x)*)(addr))

#define __put_user_asm(x, addr, err, itype, rtype, ltype, errret)	\
	(err) = __put_user((x), (typeof(x)*)(addr))
