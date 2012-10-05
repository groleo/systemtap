#define _GNU_SOURCE

#define STP_NO_OVERLOAD 1

/* We don't need to worry about pagefaults in loc2c-runtime.h */
#define STAPCONF_PAGEFAULT_DISABLE  1
#define pagefault_disable()
#define pagefault_enable()
