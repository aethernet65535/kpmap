/* File Name: kpmap.c */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/pagewalk.h>
#include <linux/pgtable.h>
#include <linux/mm.h>
#include <linux/delay.h>

#define KERNEL_END	(0xffffffffffff)
#define START_ADDR	(0)
#define END_ADDR	(KERNEL_END)

#define USER_MASK	(1 << PAGE_SHIFT)

#define IWANTKERNEL	(1)

struct pte_info {
	bool read, write, exec, user, global;
};

static int pti_status(void)
{
	if(static_cpu_has(X86_FEATURE_PTI))
		return 1;

	return 0;
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
	pte_t entry = *pte;
	struct pte_info flags = parse_pte(entry);

	if (!pte_present(entry))
		goto end;

	pr_info("%s pte: %lx \t\t Flags: %c%c%c%c%c\n",
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

static int kpmap_walk(void)
{
	struct mm_struct *mm;
	struct mm_walk walk;
	pgd_t *pgd;

	mm = current->mm;
	pgd = mm->pgd;

	if (!mm) {
		pr_info("kpmap: mm is NULL\n");
		return -1;
	}

	if (!pgd) {
		pr_info("kpmap: pgd is NULL\n");
		return -1;
	}
	
	if (pti_status()) {
		if (!IWANTKERNEL)
			pgd = (pgd_t *)((unsigned long)pgd | USER_MASK);
	}
	else
		pr_info("kpmap: PTI is off\n");
		
	if ((unsigned long)pgd & USER_MASK)
		pr_info("kpmap: pgd is user\n");
	else
		pr_info("kpmap: pgd is kernel\n");

	walk = (struct mm_walk){
		.mm = mm,
		.ops = &my_walk_ops,
		.pgd = pgd,
	};
	
	/* For debug, delete it if your speed faster than light. */
	ssleep(5);
	
	mmap_read_lock(mm);
	walk_pgd_range(START_ADDR, END_ADDR, &walk);
	mmap_read_unlock(mm);

	return 0;
}

static int __init kpmap_init(void)
{
	pr_info("kpmap: module loaded\n");

	kpmap_walk();

	return 0;
}

static void __exit kpmap_exit(void)
{
	pr_info("kpmap: module unloaded\n");
}

module_init(kpmap_init);
module_exit(kpmap_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("aethernet");
MODULE_DESCRIPTION("walk and print process's pagetable");
MODULE_VERSION("2.0");
