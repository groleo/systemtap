#if defined(CONFIG_ARCH_SUPPORTS_UPROBES) && defined(CONFIG_UPROBES)
#include <linux/wait.h>
#include <linux/uprobes.h>
/* Check whether we have uretprobes. */
void *reg = uretprobe_register;
void *ureg = uretprobe_unregister;

#else
#error "not an inode-uprobes kernel"
#endif

