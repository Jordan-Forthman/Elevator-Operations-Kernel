#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/time64.h>

static struct proc_dir_entry *proc_entry;
static struct timespec64 last_read_time;	//stores time stamp of last read operation
static struct timespec64 start_time;	//stores inital time when module is loaded
static bool is_first_read = true; 	//track first read

static int my_timer_show(struct seq_file *m, void *v) {
    struct timespec64 current_time;
    ktime_get_real_ts64(&current_time);
    s64 elapsed_ns = 0;

    seq_printf(m, "Current time: %lld.%09ld\n", (long long)current_time.tv_sec, current_time.tv_nsec);

    if (!is_first_read) {
        elapsed_ns = timespec64_sub(current_time, last_read_time).tv_nsec +
	    (s64)(current_time.tv_sec - last_read_time.tv_sec) * 1000000000;
	seq_printf(m, "Elapsed time: %lld.%09ld\n", elapsed_ns / 1000000000, (long) (elapsed_ns % 1000000000));
    } else {
	is_first_read = false;	//set to false after first read
    }

    last_read_time = current_time;	//update after processing

    return 0;
}

static int my_timer_open(struct inode *inode, struct file *file) {
    return single_open(file, my_timer_show, NULL);
}

//file operations for /proc/timer
static const struct proc_ops my_timer_proc_ops = {
    .proc_open = my_timer_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init my_timer_init(void) {
    ktime_get_real_ts64(&start_time);	//init current time
    last_read_time = start_time;
    is_first_read = true;

    proc_entry = proc_create("timer", 0, NULL, &my_timer_proc_ops);
    //error handling
    if (!proc_entry) {
        printk(KERN_ERR "Failed to create /proc/timer\n");
        return -ENOMEM;
    }
    return 0;
}

//exit func
static void __exit my_timer_exit(void) {
    proc_remove(proc_entry);
}

module_init(my_timer_init);
module_exit(my_timer_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jordan Forthman");
MODULE_DESCRIPTION("A simple timer kernel module");
