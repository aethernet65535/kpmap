/* File Name: kpmap.c */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/pagewalk.h>
#include <linux/pgtable.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define KERNEL_END (0xffffffffffff)
#define START_ADDR (0)
#define END_ADDR (KERNEL_END)

#define USER_MASK (1 << PAGE_SHIFT)

#define IWANTKERNEL (0)

struct pte_info {
	bool read, write, exec, user, global;
};

static int pti_status(void)
{
	return static_cpu_has(X86_FEATURE_PTI) ? 1 : 0;
}

static int pte_user(pte_t pte)
{
	return pte_val(pte) & _PAGE_USER;
}

static struct pte_info parse_pte(pte_t pte)
{
	struct pte_info flags = { 0 };

	if (!pte_present(pte))
		return flags;

	flags.read = 1;
	flags.write = pte_write(pte);
	flags.exec = pte_exec(pte);
	flags.user = pte_user(pte);
	flags.global = pte_global(pte);

	return flags;
}

static int my_pte_entry(pte_t *pte, unsigned long addr, unsigned long next,
			struct mm_walk *walk)
{
	struct seq_file *m = walk->private;
	pte_t entry = *pte;
	struct pte_info flags = parse_pte(entry);

	if (!pte_present(entry))
		goto end;

	seq_printf(m, "%s pte: %lx \t\t Flags: %c%c%c%c%c\n",
		   (addr >= TASK_SIZE_MAX) ? "KERNEL" : "USER", addr,
		   (flags.read) ? 'r' : '-', (flags.write) ? 'w' : '-',
		   (flags.exec) ? 'x' : '-', (flags.user) ? 'u' : '-',
		   (flags.global) ? 'g' : '-');

end:
	return 0;
}

static const struct mm_walk_ops my_walk_ops = {
	.pte_entry = my_pte_entry,
};

static int kpmap_walk(struct seq_file *m, void *v)
{
	struct mm_struct *mm;
	struct mm_walk walk;
	pgd_t *pgd;

	mm = current->mm;
	pgd = mm->pgd;

	walk = (struct mm_walk){
		.mm = mm,
		.ops = &my_walk_ops,
		.private = m,
	};

	if (!mm) {
		seq_printf(m, "kpmap: mm is NULL\n");
		return -1;
	}

	if (!pgd) {
		seq_printf(m, "kpmap: pgd is NULL\n");
		return -1;
	}

	if (pti_status()) {
		if (!IWANTKERNEL)
			pgd = (pgd_t *)((unsigned long)pgd | USER_MASK);
	} else {
		seq_printf(m, "kpmap: PTI is off\n");
	}

	walk.pgd = pgd;

	if ((unsigned long)pgd & USER_MASK)
		seq_printf(m, "kpmap: pgd is user\n");
	else
		seq_printf(m, "kpmap: pgd is kernel\n");

	/* For debug, delete it if your speed faster than light. */
	ssleep(5);

	seq_printf(m, "--- START WALK ---\n");

	mmap_read_lock(mm);
	walk_pgd_range(START_ADDR, END_ADDR, &walk);
	mmap_read_unlock(mm);

	seq_printf(m, "--- END WALK ---\n");

	return 0;
}

static int kpmap_open(struct inode *inode, struct file *file)
{
	return single_open(file, kpmap_walk, NULL);
}

static const struct proc_ops kpmap_fops = {
	.proc_open = kpmap_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static struct proc_dir_entry *kp_proc_file;

static int __init kpmap_init(void)
{
	kp_proc_file = proc_create("kpmap_dump", 0444, NULL, &kpmap_fops);
	if (!kp_proc_file) {
		pr_err("kpmap: failed to create proc entry\n");
		return -ENOMEM;
	}
	pr_info("kpmap: module loaded. Read /proc/kpmap_dump to get data.\n");
	return 0;
}

static void __exit kpmap_exit(void)
{
	if (kp_proc_file)
		remove_proc_entry("kpmap_dump", NULL);
	pr_info("kpmap: module unloaded\n");
}

module_init(kpmap_init);
module_exit(kpmap_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("aethernet");
MODULE_DESCRIPTION("walk and print process's pagetable");
MODULE_VERSION("2.0");
