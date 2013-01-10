/* -*- linux-c -*-
 * Common functions for using inode-based uprobes
 * Copyright (C) 2011, 2012 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _UPROBES_INODE_C_
#define _UPROBES_INODE_C_

#include <linux/fs.h>
#include <linux/list.h>
#include <linux/namei.h>
#include <linux/rwlock.h>
#include <linux/uprobes.h>
#include <linux/mutex.h>

/* STAPIU: SystemTap Inode Uprobes */


// PR13489, inodes-uprobes export kludge
#if !defined(CONFIG_UPROBES)
#error "not to be built without CONFIG_UPROBES"
#endif

#if !defined(STAPCONF_UPROBE_REGISTER_EXPORTED)
// First get the right typeof(name) that's found in uprobes.h
#if defined(STAPCONF_OLD_INODE_UPROBES)
typedef typeof(&register_uprobe) uprobe_register_fn;
#else
typedef typeof(&uprobe_register) uprobe_register_fn;
#endif
// Then define the typecasted call via function pointer
#define uprobe_register (* (uprobe_register_fn)kallsyms_uprobe_register)
#elif defined(STAPCONF_OLD_INODE_UPROBES)
// In this case, just need to map the new name to the old
#define uprobe_register register_uprobe
#endif

#if !defined(STAPCONF_UPROBE_UNREGISTER_EXPORTED)
// First get the right typeof(name) that's found in uprobes.h
#if defined(STAPCONF_OLD_INODE_UPROBES)
typedef typeof(&unregister_uprobe) uprobe_unregister_fn;
#else
typedef typeof(&uprobe_unregister) uprobe_unregister_fn;
#endif
// Then define the typecasted call via function pointer
#define uprobe_unregister (* (uprobe_unregister_fn)kallsyms_uprobe_unregister)
#elif defined(STAPCONF_OLD_INODE_UPROBES)
// In this case, just need to map the new name to the old
#define uprobe_unregister unregister_uprobe
#endif

#if defined(STAPCONF_INODE_URETPROBES)
#if !defined(STAPCONF_URETPROBE_REGISTER_EXPORTED)
// First typedef from the original decl, then #define it as a typecasted call.
typedef typeof(&uretprobe_register) uretprobe_register_fn;
#define uretprobe_register (* (uretprobe_register_fn)kallsyms_uretprobe_register)
#endif

#if !defined(STAPCONF_URETPROBE_UNREGISTER_EXPORTED)
// First typedef from the original decl, then #define it as a typecasted call.
typedef typeof(&uretprobe_unregister) uretprobe_unregister_fn;
#define uretprobe_unregister (* (uretprobe_unregister_fn)kallsyms_uretprobe_unregister)
#endif
#endif

#if !defined(STAPCONF_UPROBE_GET_SWBP_ADDR_EXPORTED)
// First typedef from the original decl, then #define it as a typecasted call.
typedef typeof(&uprobe_get_swbp_addr) uprobe_get_swbp_addr_fn;
#define uprobe_get_swbp_addr (* (uprobe_get_swbp_addr_fn)kallsyms_uprobe_get_swbp_addr)
#endif

/* A target is a specific file/inode that we want to probe.  */
struct stapiu_target {
	/* All the uprobes for this target. */
	struct list_head consumers; /* stapiu_consumer */

	/* All the processes containing this target.
	 * This may not be system-wide, e.g. only the -c process.
	 * We use task_finder to manage this list.  */
	struct list_head processes; /* stapiu_process */
	struct stap_task_finder_target finder;

	const char * const filename;
	struct inode *inode;
	struct mutex lock;
};


/* A consumer is a specific uprobe that we want to place.  */
struct stapiu_consumer {
	struct uprobe_consumer consumer;

	const unsigned return_p:1;
	unsigned registered:1;

	struct list_head target_consumer;
	struct stapiu_target * const target;

	loff_t offset; /* the probe offset within the inode */
	loff_t sdt_sem_offset; /* the semaphore offset from process->base */

  	// List of perf counters used by each probe
  	// This list is an index into struct stap_perf_probe,
        long perf_counters_dim;
        long (*perf_counters) [];
        const struct stap_probe * const probe;
};


/* A process that we want to probe.  These are dynamically discovered and
 * associated using task_finder, allocated from this static array.  */
static struct stapiu_process {
	struct list_head target_process;
	unsigned long relocation; /* the mmap'ed .text address */
	unsigned long base; /* the address to apply sdt offsets against */
	pid_t tgid;
} stapiu_process_slots[MAXUPROBES];


/* This lock guards modification to stapiu_process_slots and target->processes.
 * XXX: consider fine-grained locking for target-processes.  */
static DEFINE_RWLOCK(stapiu_process_lock);


/* The stap-generated probe handler for all inode-uprobes. */
static int
stapiu_probe_handler (struct stapiu_consumer *sup, struct pt_regs *regs);

static int
stapiu_probe_prehandler (struct uprobe_consumer *inst, unsigned long ip, struct pt_regs *regs)
{
	unsigned long saved_ip;
	struct stapiu_consumer *sup =
		container_of(inst, struct stapiu_consumer, consumer);
	int ret;

	/* NB: The current test kernels of new uretprobes are only passing a
	 * valid address for uretprobes, and passing 0 for regular uprobes.  In
	 * the near future, the latter should get a fixed-up address too, and
	 * this call to uprobe_get_swbp_addr() can go away.  */
	if (ip == 0)
		ip = uprobe_get_swbp_addr(regs);

	/* Make it look like the IP is set as it would in the actual user task
	 * when calling real probe handler. Reset IP regs on return, so we
	 * don't confuse uprobes.  */
	saved_ip = REG_IP(regs);
	SET_REG_IP(regs, ip);
	ret = stapiu_probe_handler(sup, regs);
	SET_REG_IP(regs, saved_ip);
	return ret;
}

#ifdef STAPCONF_INODE_UPROBES_NOADDR
/* This is the old form of uprobes handler, without the separate ip address.
 * We'll always have to kludge it in with uprobe_get_swbp_addr().
 */
static int
stapiu_probe_prehandler_noaddr (struct uprobe_consumer *inst, struct pt_regs *regs)
{
	unsigned long ip = uprobe_get_swbp_addr(regs);
	return stapiu_probe_prehandler(inst, ip, regs);
}
#define STAPIU_HANDLER stapiu_probe_prehandler_noaddr
#else
#define STAPIU_HANDLER stapiu_probe_prehandler
#endif

static int
stapiu_register (struct inode* inode, struct stapiu_consumer* c)
{
	c->consumer.handler = STAPIU_HANDLER;
	if (!c->return_p) {
		return uprobe_register (inode, c->offset, &c->consumer);
        } else {
#if defined(STAPCONF_INODE_URETPROBES)
		return uretprobe_register (inode, c->offset, &c->consumer);
#else
		return EINVAL;
#endif
        }
}

static void
stapiu_unregister (struct inode* inode, struct stapiu_consumer* c)
{
	if (!c->return_p)
		uprobe_unregister (inode, c->offset, &c->consumer);
#if defined(STAPCONF_INODE_URETPROBES)
	else
		uretprobe_unregister (inode, c->offset, &c->consumer);
#endif
}


static inline void
stapiu_target_lock(struct stapiu_target *target)
{
	mutex_lock(&target->lock);
}

static inline void
stapiu_target_unlock(struct stapiu_target *target)
{
	mutex_unlock(&target->lock);
}

/* Read-modify-write a semaphore, usually +/- 1.  */
static int
stapiu_write_semaphore(unsigned long addr, unsigned short delta)
{
	int rc = 0;
	unsigned short __user* sdt_addr = (unsigned short __user*) addr;
	unsigned short sdt_semaphore = 0; /* NB: fixed size */
	/* XXX: need to analyze possibility of race condition */
	rc = get_user(sdt_semaphore, sdt_addr);
	if (!rc) {
		sdt_semaphore += delta;
		rc = put_user(sdt_semaphore, sdt_addr);
	}
	return rc;
}


/* Read-modify-write a semaphore in an arbitrary task, usually +/- 1.  */
static int
stapiu_write_task_semaphore(struct task_struct* task,
			    unsigned long addr, unsigned short delta)
{
	int count, rc = 0;
	unsigned short sdt_semaphore = 0; /* NB: fixed size */
	/* XXX: need to analyze possibility of race condition */
	count = __access_process_vm_noflush(task, addr,
			&sdt_semaphore, sizeof(sdt_semaphore), 0);
	if (count != sizeof(sdt_semaphore))
		rc = 1;
	else {
		sdt_semaphore += delta;
		count = __access_process_vm_noflush(task, addr,
				&sdt_semaphore, sizeof(sdt_semaphore), 1);
		rc = (count == sizeof(sdt_semaphore)) ? 0 : 1;
	}
	return rc;
}


static void
stapiu_decrement_process_semaphores(struct stapiu_process *p,
				    struct list_head *consumers)
{
	struct task_struct *task;
	rcu_read_lock();

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
	/* We'd like to call find_task_by_pid_ns() here, but it isn't
	 * exported.  So, we call what it calls...  */
	task = pid_task(find_pid_ns(p->tgid, &init_pid_ns), PIDTYPE_PID);
#else
	task = find_task_by_pid(p->tgid);
#endif

	/* The task may have exited while we weren't watching.  */
	if (task) {
		struct stapiu_consumer *c;

		/* Holding the rcu read lock makes us atomic, and we
		 * can't write userspace memory while atomic (which
		 * could pagefault).  So, instead we lock the task
		 * structure, then release the rcu read lock. */
		get_task_struct(task);
		rcu_read_unlock();

		list_for_each_entry(c, consumers, target_consumer) {
			if (c->sdt_sem_offset) {
				unsigned long addr = p->base + c->sdt_sem_offset;
				stapiu_write_task_semaphore(task, addr,
						(unsigned short) -1);
			}
		}
		put_task_struct(task);
	}
	else {
		rcu_read_unlock();
	}
}


/* As part of shutdown, we need to decrement the semaphores in every task we've
 * been attached to.  */
static void
stapiu_decrement_semaphores(struct stapiu_target *targets, size_t ntargets)
{
	size_t i;
	/* NB: no stapiu_process_lock needed, as the task_finder engine is
	 * already stopped by now, so no one else will mess with us.  We need
	 * to be sleepable for access_process_vm.  */
	might_sleep();
	for (i = 0; i < ntargets; ++i) {
		struct stapiu_target *ut = &targets[i];
		struct stapiu_consumer *c;
		struct stapiu_process *p;
		int has_semaphores = 0;

		list_for_each_entry(c, &ut->consumers, target_consumer) {
			if (c->sdt_sem_offset) {
				has_semaphores = 1;
				break;
			}
		}
		if (!has_semaphores)
			continue;

		list_for_each_entry(p, &ut->processes, target_process)
			stapiu_decrement_process_semaphores(p, &ut->consumers);
	}
}


/* Unregister all uprobe consumers of a target.  */
static void
stapiu_target_unreg(struct stapiu_target *target)
{
	struct stapiu_consumer *c;

	if (! target->inode)
		return;
	list_for_each_entry(c, &target->consumers, target_consumer) {
		if (c->registered) {
			c->registered = 0;
			stapiu_unregister(target->inode, c);
		}
	}
}


/* Register all uprobe consumers of a target.  */
static int
stapiu_target_reg(struct stapiu_target *target, struct task_struct* task)
{
	int ret = 0;
	struct stapiu_consumer *c;

	list_for_each_entry(c, &target->consumers, target_consumer) {
		if (! c->registered) {
			int i;
			for (i=0; i < c->perf_counters_dim; i++) {
			  if ((*(c->perf_counters))[i] > -1)
			    _stp_perf_read_init ((*(c->perf_counters))[i], task);
		        }
			ret = stapiu_register(target->inode, c);
			if (ret) {
				c->registered = 0;
				_stp_error("probe %s registration error (rc %d)",
					   c->probe->pp, ret);
				break;
			}
			c->registered = 1;
		}
	}
	if (ret)
		stapiu_target_unreg(target);
	return ret;
}


/* Cleanup every target.  */
static void
stapiu_exit_targets(struct stapiu_target *targets, size_t ntargets)
{
	size_t i;
	for (i = 0; i < ntargets; ++i) {
		struct stapiu_target *ut = &targets[i];

		stapiu_target_unreg(ut);

		/* NB: task_finder needs no unregister. */
		if (ut->inode) {
			iput(ut->inode);
			ut->inode = NULL;
		}
	}
}


/* Initialize every target.  */
static int
stapiu_init_targets(struct stapiu_target *targets, size_t ntargets)
{
	int ret = 0;
	size_t i;
	for (i = 0; i < ntargets; ++i) {
		struct stapiu_target *ut = &targets[i];
		INIT_LIST_HEAD(&ut->consumers);
		INIT_LIST_HEAD(&ut->processes);
		mutex_init(&ut->lock);
		ret = stap_register_task_finder_target(&ut->finder);
		if (ret != 0) {
			_stp_error("Couldn't register task finder target for file '%s': %d\n",
				   ut->filename, ret);
			break;
		}
	}
	return ret;
}


/* Initialize the entire inode-uprobes subsystem.  */
static int
stapiu_init(struct stapiu_target *targets, size_t ntargets,
	    struct stapiu_consumer *consumers, size_t nconsumers)
{
	int ret = stapiu_init_targets(targets, ntargets);
	if (!ret) {
		size_t i;

		/* Connect each consumer to its target. */
		for (i = 0; i < nconsumers; ++i) {
			struct stapiu_consumer *uc = &consumers[i];
			list_add(&uc->target_consumer,
				 &uc->target->consumers);
		}
	}
	return ret;
}


/* Shutdown the entire inode-uprobes subsystem.  */
static void
stapiu_exit(struct stapiu_target *targets, size_t ntargets,
	    struct stapiu_consumer *consumers, size_t nconsumers)
{
	stapiu_decrement_semaphores(targets, ntargets);
	stapiu_exit_targets(targets, ntargets);
}


/* Task-finder found a process with the target that we're interested in.
 * Grab a process slot and associate with this target, so the semaphores
 * and filtering can work properly.  */
static int
stapiu_change_plus(struct stapiu_target* target, struct task_struct *task,
		   unsigned long relocation, unsigned long length,
		   unsigned long offset, unsigned long vm_flags,
		   struct inode *inode)
{
	size_t i;
	struct stapiu_process *p;
	int rc;

	/* Check the buildid of the target (if we haven't already). We
	 * lock the target so we don't have concurrency issues. */
	stapiu_target_lock(target);
	if (! target->inode) {
		if (! inode) {
			rc = -EINVAL;
			stapiu_target_unlock(target);
			return rc;
		}

		/* Grab the inode first (to prevent TOCTTOU problems). */
		target->inode = igrab(inode);
		if (!target->inode) {
			_stp_error("Couldn't get inode for file '%s'\n",
				   target->filename);
			rc = -EINVAL;
			stapiu_target_unlock(target);
			return rc;
		}

		/* Actually do the check. */
		if ((rc = _stp_usermodule_check(task, target->filename,
						relocation))) {
			/* Be sure to release the inode on failure. */
			iput(target->inode);
			target->inode = NULL;
			stapiu_target_unlock(target);
			return rc;
		}

		/* OK, we've checked the target's buildid. Now
		 * register all its consumers. */
		rc = stapiu_target_reg(target, task);
		if (rc) {
			/* Be sure to release the inode on failure. */
			iput(target->inode);
			target->inode = NULL;
			stapiu_target_unlock(target);
			return rc;
		}
	}
	stapiu_target_unlock(target);

	/* Associate this target with this process. */
	write_lock(&stapiu_process_lock);
	for (i = 0; i < MAXUPROBES; ++i) {
		p = &stapiu_process_slots[i];
		if (!p->tgid) {
			p->tgid = task->tgid;
			p->relocation = relocation;

                        /* The base is used for relocating semaphores.  If the
                         * probe is in an ET_EXEC binary, then that offset
                         * already is a real address.  But stapiu_process_found
                         * calls us in this case with relocation=offset=0, so
                         * we don't have to worry about it.  */
			p->base = relocation - offset;

			list_add(&p->target_process, &target->processes);
			break;
		}
	}
	write_unlock(&stapiu_process_lock);

	return 0; /* XXX: or an error? maxskipped? */
}


/* Task-finder found a writable mapping in our interested target.
 * If any of the consumers need a semaphore, increment now.  */
static int
stapiu_change_semaphore_plus(struct stapiu_target* target, struct task_struct *task,
			     unsigned long relocation, unsigned long length)
{
	int rc = 0;
	struct stapiu_process *p, *process = NULL;
	struct stapiu_consumer *c;

	/* First find the related process, set by stapiu_change_plus.  */
	read_lock(&stapiu_process_lock);
	list_for_each_entry(p, &target->processes, target_process) {
		if (p->tgid == task->tgid) {
			process = p;
			break;
		}
	}
	read_unlock(&stapiu_process_lock);
	if (!process)
		return 0;

	/* NB: no lock after this point, as we need to be sleepable for
	 * get/put_user semaphore action.  The given process should be frozen
	 * while we're busy, so it's not an issue.
	 */

	/* Look through all the consumers and increment semaphores.  */
	list_for_each_entry(c, &target->consumers, target_consumer) {
		if (c->sdt_sem_offset) {
			unsigned long addr = process->base + c->sdt_sem_offset;
			if (addr >= relocation && addr < relocation + length) {
				int rc2 = stapiu_write_task_semaphore(task,
								      addr, +1);
				if (!rc)
					rc = rc2;
			}
		}
	}
	return rc;
}


/* Task-finder found a mapping that's now going away.  We don't need to worry
 * about the semaphores, so we can just release the process slot.  */
static int
stapiu_change_minus(struct stapiu_target* target, struct task_struct *task,
		    unsigned long relocation, unsigned long length)
{
	struct stapiu_process *p, *tmp;

	/* NB: we aren't unregistering uprobes and releasing the
	 * inode here.  The registration is system-wide, based on
	 * inode, not process based.  */

	write_lock(&stapiu_process_lock);
	list_for_each_entry_safe(p, tmp, &target->processes, target_process) {
		if (p->tgid == task->tgid && (relocation <= p->relocation &&
					      p->relocation < relocation+length)) {
			list_del(&p->target_process);
			memset(p, 0, sizeof(*p));
		}
	}
	write_unlock(&stapiu_process_lock);
	return 0;
}


static struct inode *
stapiu_get_task_inode(struct task_struct *task)
{
	struct mm_struct *mm;
	struct file* vm_file;
	struct inode *inode = NULL;

	// Grab the inode associated with the task.
	//
	// Note we're not calling get_task_mm()/mmput() here.  Since
	// we're in the the context of task, the mm should stick
	// around without locking it (and mmput() can sleep).
	mm = task->mm;
	if (! mm) {
		/* If the thread doesn't have a mm_struct, it is
		 * a kernel thread which we need to skip. */
		return NULL;
	}

	down_read(&mm->mmap_sem);
	vm_file = stap_find_exe_file(mm);
	if (vm_file && vm_file->f_path.dentry)
		inode = vm_file->f_path.dentry->d_inode;

	up_read(&mm->mmap_sem);
	return inode;
}


/* The task_finder_callback we use for ET_EXEC targets. */
static int
stapiu_process_found(struct stap_task_finder_target *tf_target,
		     struct task_struct *task, int register_p, int process_p)
{
	struct stapiu_target *target =
		container_of(tf_target, struct stapiu_target, finder);

	if (!process_p)
		return 0; /* ignore threads */

	/* ET_EXEC events are like shlib events, but with 0 relocation bases */
	if (register_p) {
		int rc = -EINVAL;
		struct inode *inode = stapiu_get_task_inode(task);

		if (inode) {
			rc = stapiu_change_plus(target, task, 0, TASK_SIZE,
						0, 0, inode);
			stapiu_change_semaphore_plus(target, task, 0,
						     TASK_SIZE);
		}
		return rc;
	} else
		return stapiu_change_minus(target, task, 0, TASK_SIZE);
}


/* The task_finder_mmap_callback */
static int
stapiu_mmap_found(struct stap_task_finder_target *tf_target,
		  struct task_struct *task,
		  char *path, struct dentry *dentry,
		  unsigned long addr, unsigned long length,
		  unsigned long offset, unsigned long vm_flags)
{
	int rc = 0;
	struct stapiu_target *target =
		container_of(tf_target, struct stapiu_target, finder);

	/* Sanity check that the inodes match (if the target's inode
	 * is set). Doesn't guarantee safety, but it's a start.  If
	 * the target's inode isn't set, this is the first time we've
	 * seen this target.
	 */
	if (target->inode && dentry->d_inode != target->inode)
		return 0;

	/* The file path must match too. */
	if (!path || strcmp (path, target->filename))
		return 0;

	/* 1 - shared libraries' executable segments load from offset 0
	 *   - ld.so convention offset != 0 is now allowed
	 *     so stap_uprobe_change_plus can set a semaphore,
	 *     i.e. a static extern, in a shared object
	 * 2 - the shared library we're interested in
	 * 3 - mapping should be executable or writeable (for
	 *     semaphore in .so)
	 *     NB: or both, on kernels that lack noexec mapping
	 */

	/* Check non-writable, executable sections for probes. */
	if ((vm_flags & VM_EXEC) && !(vm_flags & VM_WRITE))
		rc = stapiu_change_plus(target, task, addr, length,
					offset, vm_flags, dentry->d_inode);

	/* Check writeable sections for semaphores.
	 * NB: They may have also been executable for the check above,
	 *     if we're running a kernel that lacks noexec mappings.
	 *     So long as there's no error (rc == 0), we need to look
	 *     for semaphores too. 
	 */
	if ((rc == 0) && (vm_flags & VM_WRITE))
		rc = stapiu_change_semaphore_plus(target, task, addr, length);

	return rc;
}


/* The task_finder_munmap_callback */
static int
stapiu_munmap_found(struct stap_task_finder_target *tf_target,
		    struct task_struct *task,
		    unsigned long addr, unsigned long length)
{
	struct stapiu_target *target =
		container_of(tf_target, struct stapiu_target, finder);

	return stapiu_change_minus(target, task, addr, length);
}


/* The task_finder_callback we use for ET_DYN targets.
 * This just forces an unmap of everything as the process exits. (PR11151)
 */
static int
stapiu_process_munmap(struct stap_task_finder_target *tf_target,
		      struct task_struct *task,
		      int register_p, int process_p)
{
	struct stapiu_target *target =
		container_of(tf_target, struct stapiu_target, finder);

	if (!process_p)
		return 0; /* ignore threads */

	/* Covering 0->TASK_SIZE means "unmap everything" */
	if (!register_p)
		return stapiu_change_minus(target, task, 0, TASK_SIZE);
	return 0;
}


#endif /* _UPROBES_INODE_C_ */
