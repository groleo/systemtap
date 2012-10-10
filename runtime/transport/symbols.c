/* -*- linux-c -*- 
 * symbols.c - stp symbol and module functions
 *
 * Copyright (C) Red Hat Inc, 2006-2011
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _STP_SYMBOLS_C_
#define _STP_SYMBOLS_C_
#include "../sym.h"


static void systemtap_module_refresh (void);
static int _stp_kmodule_check (const char*);

/* PR12612: pre-commit-3abb860f values */

#define STP13_MODULE_NAME_LEN 64
#define STP13_SYMBOL_NAME_LEN 64
struct _stp13_msg_relocation {
        char module[STP13_MODULE_NAME_LEN];
        char reloc[STP13_SYMBOL_NAME_LEN];
        uint64_t address;
};

static void _stp_do_relocation(const char __user *buf, size_t count)
{
  static struct _stp_msg_relocation msg; /* by protocol, never concurrently used */
  static struct _stp13_msg_relocation msg13; /* ditto */

  /* PR12612: Let's try to be compatible with systemtap modules being
     compiled by new systemtap, but loaded (staprun'd) by an older
     systemtap runtime.  The only known incompatilibility is that we
     get an older, smaller, relocation message.  So here we accept both
     sizes. */
  if (sizeof(msg) == count) { /* systemtap 1.4+ runtime */
    if (unlikely(copy_from_user (& msg, buf, count)))
            return;
  } else if (sizeof(msg13) == count) { /* systemtap 1.3- runtime */
    if (unlikely(copy_from_user (& msg13, buf, count)))
            return;
#if STP_MODULE_NAME_LEN <= STP13_MODULE_NAME_LEN
#error "STP_MODULE_NAME_LEN should not be smaller than STP13_MODULE_NAME_LEN"
#endif
    strlcpy (msg.module, msg13.module, STP13_MODULE_NAME_LEN);
    strlcpy (msg.reloc, msg13.reloc, STP13_MODULE_NAME_LEN);
    msg.address = msg13.address;
  } else {
      errk ("STP_RELOCATE message size mismatch (%lu or %lu vs %lu)\n",
            (long unsigned) sizeof(msg), (long unsigned) sizeof (msg13), (long unsigned) count);
      return;
  }

  dbug_sym(2, "relocate (%s %s 0x%lx)\n", msg.module, msg.reloc, (unsigned long) msg.address);

  /* Detect actual kernel load address. */
  if (!strcmp ("kernel", msg.module)
      && !strcmp ("_stext", msg.reloc)) {
      if (msg.address == 0) {
      #ifdef CONFIG_KALLSYMS
	    dbug_sym(1,"asking kernel for _stext load address");
	    msg.address = kallsyms_lookup_name("_stext");
	    if (msg.address == 0) {
	    _stp_warn("kallsyms_lookup_name() does not returned a suitable load address for _stext");
	} else {
	    dbug_sym(1, "found kernel _stext load address: 0x%lx\n",
             (unsigned long) msg.address);
	}
     #else
	_stp_warn("No load address for symbol _stext given. Stap will not be able to relocate the probe points!");
     #endif
    }
    if (_stp_kretprobe_trampoline != (unsigned long) -1)
      _stp_kretprobe_trampoline += (unsigned long) msg.address;
  }

  _stp_kmodule_update_address(msg.module, msg.reloc, msg.address);
}



#if !defined(STAPCONF_MODULE_SECT_ATTRS) && LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
/* It would be nice if it were (still) in a header we could get to,
   like include/linux/module.h, but commit a58730c42 moved them into
   kernel/module.c. */
struct module_sect_attr
{
        struct module_attribute mattr;
        char *name;
        unsigned long address;
};

struct module_sect_attrs
{
        struct attribute_group grp;
        unsigned int nsections;
        struct module_sect_attr attrs[0];
};
#endif


static int _stp_module_notifier (struct notifier_block * nb,
                                 unsigned long val, void *data)
{
        /* Prior to 2.6.11, struct module contained a module_sections
           attribute vector rather than module_sect_attrs.  Prior to
           2.6.19, module_sect_attrs lacked a number-of-sections
           field.  Without CONFIG_KALLSYMS, we don't get any of the
           related fields at all in struct module.  XXX: autoconf for
           that directly? */

#if defined(CONFIG_KALLSYMS) && LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
        struct module *mod = data;
        struct module_sect_attrs *attrs = mod->sect_attrs;
        unsigned i;

        if (val == MODULE_STATE_COMING) {
                /* A module is arriving.  Register all of its section
                   addresses, as though staprun sent us a bunch of
                   STP_RELOCATE messages.  Now ... where did the
                   fishie go? */
                for (i=0; i<attrs->nsections; i++) 
                        _stp_kmodule_update_address(mod->name, 
                                                    attrs->attrs[i].name,
                                                    attrs->attrs[i].address);

                /* Verify build-id. */
                if (_stp_kmodule_check (mod->name))
                   _stp_kmodule_update_address(mod->name, NULL, 0); /* Pretend it was never here. */
        }
        else if (val == MODULE_STATE_LIVE) {
                /* The init section(s) may have been unloaded. */
                for (i=0; i<attrs->nsections; i++) 
                        if (strstr(attrs->attrs[i].name, "init.") != NULL)
                        _stp_kmodule_update_address(mod->name, 
                                                    attrs->attrs[i].name,
                                                    0);

                /* No need to verify build-id here; if it failed when COMING,
                   all other section names will already have reloc=0. */
        }
        else if (val == MODULE_STATE_GOING) {
                /* Unregister all sections. */
                _stp_kmodule_update_address(mod->name, NULL, 0);
        }

        /* Give the probes a chance to update themselves. */
        /* Proper kprobes support for this appears to be relatively
           recent.  Example prerequisite commits: 0deddf436a f24659d9 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
        systemtap_module_refresh();
#endif

#endif /* skipped for ancient or kallsyms-free kernels */

        return NOTIFY_DONE;
}

#if STP_TRANSPORT_VERSION == 2
/* Notification function to call on a kernel panic */
static int _stp_module_panic_notifier (struct notifier_block *nb, unsigned long val, void *data)
{
        int i;

        /* Loop over each cpu buffer */
        for_each_possible_cpu(i)
        {
                int j=0;
                struct rchan_buf * sub_buf;
                char *subbuf_start;
                char *previous;
                char *next;
                size_t bytes_passed;
                int printed;
                int first_iteration;

                sub_buf = _stp_relay_data.rchan->buf[i];

                /* Set our pointer to the beginning of the channel buffer */
                subbuf_start = (char *)sub_buf->start;

                /* Loop over each sub buffer */
                for (j=0; j< sub_buf->chan->n_subbufs; j++)
                {
                        /* Ensure our start is not NULL */
                        if(subbuf_start == NULL)
                        {
                                printk(KERN_EMERG "Current buffer is NULL\n");
                                return NOTIFY_DONE;
                        }

                        bytes_passed = 0; /* Keep track of the number of bytes already passed */
                        first_iteration = 1; /* Flag for keeping track of the 1st itteration*/
                        printed = 0; /* Flag for keeping track of when we've already printed the
                                      * message about what info might be new */

                        previous = subbuf_start;
                        next = strchr(previous, '\n');
                        bytes_passed+= (next - previous);

                        /* Loop over the whole buffer, printing line by line */
                        while (next != NULL && bytes_passed < sub_buf->chan->subbuf_size)
                        {

                                if(first_iteration)
                                {
                                        printk(KERN_CONT "Stap trace buffer for processor %d sub-buffer %d:\n", i, j);
                                }

                                /* Once we reach the number of bytes consumed on the last
                                 * sub-buffer filled, print a message saying that everything
                                 * from then on might not have made it to the display before
                                 * the kernel panic */
                                if(subbuf_start == sub_buf->data
                                   && bytes_passed >= sub_buf->bytes_consumed
                                   && !printed)
                                {
                                        printk(KERN_CONT
                                               "The following may not have been sent to the display:\n");
                                        printed = 1;
                                }

                                /* Print the line. Other than the first itteration, we need to print everything
                                 * except the first '\n' character.*/
                                if(first_iteration)
                                {
                                        printk(KERN_CONT "%.*s\n", (int)(next - previous), previous);
                                        first_iteration = 0;
                                }
                                else
                                {
                                        printk(KERN_CONT "%.*s\n", (int)(next - previous)-1, previous+1);
                                }

                                /* Get the next token */
                                previous = next;
                                next = strchr(next + 1, '\n');
                                if(next != NULL)
                                {
                                        bytes_passed+= (next - previous);
                                }
                        }

                        /* Move on to the next sub-buffer */
                        subbuf_start = subbuf_start + sub_buf->chan->subbuf_size;
                }
        }
        return NOTIFY_DONE;
}
#endif
#endif /* _STP_SYMBOLS_C_ */
