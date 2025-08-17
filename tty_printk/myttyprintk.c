#include <linux/serial.h>
#include <linux/tty.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/cdev.h>

#define DEVICE_NAME "myttyprintk"
#define TPK_STR_SIZE 508

struct myTtyPrintkData {
    struct cdev ttyPrintkCdev;
    spinlock_t spinlock;
    int tpkCurr;
    char tpkBuffer[TPK_STR_SIZE + 4];
};

static dev_t dev_num;
static struct myTtyPrintkData *ttyprintk_data;

static void tpk_flush(struct myTtyPrintkData *data) {
    if (data->tpkCurr > 0) {
        data->tpkBuffer[data->tpkCurr] = '\0';
        printk(KERN_INFO "[U] %s\n", data->tpkBuffer);
        data->tpkCurr = 0;
    }
}

static void tpk_printk(struct myTtyPrintkData *data, const u8 *buf, size_t count)
{
    char c;
    for (size_t i = 0; i < count; i++) {
        if (copy_from_user(&c, buf + i, 1) > 0) {
            return;
        }
        if (data->tpkCurr >= TPK_STR_SIZE) {
            data->tpkBuffer[data->tpkCurr++] = '\\';
            tpk_flush(data);
        }
        switch (c) {
            case '\r':
            case '\n':
                tpk_flush(data);
                break;
            default:
                data->tpkBuffer[data->tpkCurr++] = c;
                break;
        }
    }
}

static int tpk_open(struct inode *inode, struct file *file)
{
    file->private_data = ttyprintk_data;
    return 0;
}

static ssize_t tpk_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
    struct myTtyPrintkData *data = file->private_data;
    unsigned long flags;

    spin_lock_irqsave(&data->spinlock, flags);
    tpk_printk(data, buf, count);
    spin_unlock_irqrestore(&data->spinlock, flags);

    return count;
}

static int tpk_release(struct inode *inode, struct file *file) {
    struct myTtyPrintkData *data = file->private_data;
    unsigned long flags;

    spin_lock_irqsave(&data->spinlock, flags);
    tpk_flush(data); // 释放前强制刷新剩余数据
    spin_unlock_irqrestore(&data->spinlock, flags);
    return 0;
}

static struct file_operations fops = {
    .open = tpk_open,
    .write = tpk_write,
    .release = tpk_release,
};

static int __init ttyprintk_init(void) {
    // 分配设备号
    if (alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME) < 0) {
        pr_err("Failed to allocate device number\n");
        return -ENOMEM;
    }

    // 分配主结构体
    ttyprintk_data = kzalloc(sizeof(struct myTtyPrintkData), GFP_KERNEL);
    if (!ttyprintk_data) {
        unregister_chrdev_region(dev_num, 1);
        return -ENOMEM;
    }

    // 初始化自旋锁和缓冲区
    spin_lock_init(&ttyprintk_data->spinlock);
    ttyprintk_data->tpkCurr = 0;

    // 设置cdev
    cdev_init(&ttyprintk_data->ttyPrintkCdev, &fops);
    ttyprintk_data->ttyPrintkCdev.owner = THIS_MODULE;

    // 注册设备
    if (cdev_add(&ttyprintk_data->ttyPrintkCdev, dev_num, 1) < 0) {
        kfree(ttyprintk_data);
        unregister_chrdev_region(dev_num, 1);
        return -EFAULT;
    }

    pr_info("ttyprintk driver loaded (major=%d)\n", MAJOR(dev_num));
    return 0;
}

static void __exit ttyprintk_exit(void) {
    cdev_del(&ttyprintk_data->ttyPrintkCdev);
    kfree(ttyprintk_data);
    unregister_chrdev_region(dev_num, 1);
    pr_info("ttyprintk driver unloaded\n");
}

module_init(ttyprintk_init);
module_exit(ttyprintk_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Enhanced ttyprintk driver with spinlock protection");