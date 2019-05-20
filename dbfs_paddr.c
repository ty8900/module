#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <asm/pgtable.h>

#define LENGTH_MAX 100

//better to define a struct and use it while getting input and giving output

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("paddr : a simple page table walk");
MODULE_AUTHOR("Seonkyo Ok");

static struct dentry *dir, *output;
static struct task_struct *task;

//calculates the power
long power(long base, unsigned int exp){
    long result = 1;
    while(exp)
    {
        if(exp%2==1)
             result*=base;
        base*=base;
        exp/=2;
    }
    return result;
}

static ssize_t read_output(struct file *fp,
                        char __user *user_buffer,
                        size_t length,
                        loff_t *position)
{
    //memory related variables
    struct mm_struct *mm;
    pgd_t *pgdp;
    p4d_t *p4dp;
    pud_t *pudp;
    pmd_t *pmdp;
    pte_t *ptep;
    phys_addr_t pfn;

    //inputs
    pid_t input_pid;
    long int vaddr = 0;
    
    int i;
    ssize_t length_copied;
    unsigned char kernel_buffer[LENGTH_MAX];
    unsigned char out_buf[LENGTH_MAX];
    int length_output = (length < LENGTH_MAX) ? length : LENGTH_MAX;

    memset(kernel_buffer, 0, LENGTH_MAX);
    memset(out_buf, 0, LENGTH_MAX);

    if(copy_from_user(kernel_buffer, user_buffer, length_output))
        return -EFAULT;
/*
 * GET INPUTS
 */
    //get pid
    input_pid = (int)(kernel_buffer[1])*16*16+(int)(kernel_buffer[0]);
    //get the vaddr input(48bit)
    for(i=0;i<6;i++)
        vaddr += (long)(kernel_buffer[8+i])*power(16*16,i);

/*
 * PAGE TABLE WALK
 *  - after finding values, validating follows
 */
    //get task_struct
    task = pid_task(find_get_pid(input_pid), PIDTYPE_PID);
    if(!task)
        return -EINVAL;
    //get mm_struct
    mm = task->mm;
    //get pointer to pgd
    pgdp = pgd_offset(mm, vaddr);
    if(pgd_none(*pgdp) || pgd_bad(*pgdp))
        return -EINVAL;
    //get pointer to p4d
    p4dp = p4d_offset(pgdp, vaddr);
    if(p4d_none(*p4dp) || p4d_bad(*p4dp))
        return -EINVAL;
    //get pointer to pud
    pudp = pud_offset(p4dp, vaddr);
    if(pud_none(*pudp) || pud_bad(*pudp))
        return -EINVAL;
    //get pointer to pmd
    pmdp = pmd_offset(pudp, vaddr);
    if(pmd_none(*pmdp) || pmd_bad(*pmdp))
        return -EINVAL;
    //get pointer to pte
    ptep = pte_offset_kernel(pmdp, vaddr);
    if(pte_none(*ptep) || !pte_present(*ptep))
        return -EINVAL;
    //get page frame number
    pfn = pte_pfn(*ptep);
/*
 * SET THE RESULT
 */
    for(i=0;i<length_output;i++){
        //keep the other values(~15)
        if(i<16)
            out_buf[i] = kernel_buffer[i];
        //keep the lower 8 bits of vaddr(~16)
        else if(i<17)
            out_buf[i] = kernel_buffer[i-8];
        //keep the lower 12~9 bits of vaddr and then set the lower 4 bits of paddr
        else if(i<18) {
            out_buf[i] = (kernel_buffer[i-8])%16;
            out_buf[i] += (pfn%16)*16;
            pfn /= 16;
        }
        //set the other bits
        else {
            out_buf[i] = (pfn%16);
            pfn /= 16;
            out_buf[i] += (pfn%16)*16;
            pfn /= 16;
        }
    }
    out_buf[length_output] = '\0';
    //copy the result to user buffer
    length_copied = simple_read_from_buffer(user_buffer, length, position, out_buf, length_output);
    return length_copied;
}

static const struct file_operations dbfs_fops = {
    .read = read_output,
};

static int __init dbfs_module_init(void)
{
    // Implement init module
    dir = debugfs_create_dir("paddr", NULL);
    if (!dir) {
        printk(KERN_ERR "Cannot create paddr dir\n");
        return -1;
    }
    output = debugfs_create_file("output", 00700, dir, NULL, &dbfs_fops);
    if(!output) {
        printk(KERN_ERR "Cannot create output file\n");
        return -1;
    }
	printk("dbfs_paddr module initialize done\n");
    return 0;
}

static void __exit dbfs_module_exit(void)
{
    debugfs_remove_recursive(dir);    
	printk("dbfs_paddr module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
