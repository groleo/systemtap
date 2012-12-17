#if defined(CONFIG_ARCH_SUPPORTS_UPROBES) && defined(CONFIG_UPROBES)
#include <linux/wait.h>
#include <linux/uprobes.h>
/* Check the signature of uprobes handlers.
 * This is the old form; new versions also include a separate long addr.
 */
static int handler (struct uprobe_consumer *inst, struct pt_regs *regs)
{
    (void)inst;
    (void)regs;
    return 0;
}
struct uprobe_consumer consumer = { .handler=handler };


#else
#error "not an inode-uprobes kernel"
#endif

