#include <linux/serial.h>
#include <linux/tty.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/types.h>  // 添加u8类型定义

#define TPK_STR_SIZE 508
#define TPK_MAX_ROOM 4096
#define TPK_PREFIX KERN_SOH __stringify(CONFIG_TTY_PRINTK_LEVEL)

struct ttyprintk_port {
    struct tty_port port;
    spinlock_t spinlock;
};

static struct ttyprintk_port tpk_port;
static int tpk_curr;
static char tpk_buffer[TPK_STR_SIZE + 4];
static struct tty_driver *ttyprintk_driver;

static void tpk_flush(void)
{
    if (tpk_curr > 0) {
        tpk_buffer[tpk_curr] = '\0';
        printk(TPK_PREFIX "[U] %s\n", tpk_buffer);
        tpk_curr = 0;
    }
}

static ssize_t tpk_printk(const u8 *buf, size_t count)
{
    size_t i;
    
    for (i = 0; i < count; i++) {
        if (tpk_curr >= TPK_STR_SIZE) {
            tpk_buffer[tpk_curr++] = '\\';
            tpk_flush();
        }
        switch (buf[i]) {
            case '\r':
            case '\n':
                tpk_flush();
                break;
            default:
                tpk_buffer[tpk_curr++] = buf[i];
                break;
        }
    }
    return count;
}

static int tpk_open(struct tty_struct *tty, struct file *filp)
{
    tty->driver_data = &tpk_port;
    return tty_port_open(&tpk_port.port, tty, filp);
}

static void tpk_close(struct tty_struct *tty, struct file *filp)
{
    struct ttyprintk_port *tpkp = tty->driver_data;
    tty_port_close(&tpkp->port, tty, filp);
}

static ssize_t tpk_write(struct tty_struct *tty, const u8 *buf, size_t count)
{
    struct ttyprintk_port *tpkp = tty->driver_data;
    unsigned long flags;
    ssize_t ret;

    spin_lock_irqsave(&tpkp->spinlock, flags);
    ret = tpk_printk(buf, count);
    spin_unlock_irqrestore(&tpkp->spinlock, flags);

    return ret;
}

static unsigned int tpk_write_room(struct tty_struct *tty)
{
    return TPK_MAX_ROOM;
}

static void tpk_hangup(struct tty_struct *tty)
{
    struct ttyprintk_port *tpkp = tty->driver_data;
    tty_port_hangup(&tpkp->port);
}

static void tpk_port_shutdown(struct tty_port *tport)
{
    struct ttyprintk_port *tpkp = container_of(tport, struct ttyprintk_port, port);
    unsigned long flags;

    spin_lock_irqsave(&tpkp->spinlock, flags);
    tpk_flush();
    spin_unlock_irqrestore(&tpkp->spinlock, flags);
}

static const struct tty_operations ttyprintk_ops = {
    .open = tpk_open,
    .close = tpk_close,
    .write = tpk_write,
    .write_room = tpk_write_room,
    .hangup = tpk_hangup,
};

static const struct tty_port_operations tpk_port_ops = {
    .shutdown = tpk_port_shutdown,
};

static int __init ttyprintk_init(void)
{
    int ret;
    printk(KERN_ALERT "My ttyprintk Module loading\n");
    spin_lock_init(&tpk_port.spinlock);

    ttyprintk_driver = tty_alloc_driver(1,
            TTY_DRIVER_RESET_TERMIOS |
            TTY_DRIVER_REAL_RAW |
            TTY_DRIVER_UNNUMBERED_NODE);
    if (IS_ERR(ttyprintk_driver))
        return PTR_ERR(ttyprintk_driver);

    tty_port_init(&tpk_port.port);
    tpk_port.port.ops = &tpk_port_ops;

    ttyprintk_driver->driver_name = "myttyprintkdr";
    ttyprintk_driver->name = "myttyprintk_xu";
    ttyprintk_driver->major = 262;
    ttyprintk_driver->minor_start = 3;
    ttyprintk_driver->type = TTY_DRIVER_TYPE_CONSOLE;
    ttyprintk_driver->init_termios = tty_std_termios;
    ttyprintk_driver->init_termios.c_oflag = OPOST | OCRNL | ONOCR | ONLRET;
    tty_set_operations(ttyprintk_driver, &ttyprintk_ops);
    tty_port_link_device(&tpk_port.port, ttyprintk_driver, 0);

    ret = tty_register_driver(ttyprintk_driver);
    if (ret < 0) {
        printk(KERN_ERR "Couldn't register ttyprintk driver\n");
        goto error;
    }

    printk(KERN_ALERT "My ttyprintk Module loaded\n");
    return 0;

error:
    tty_driver_kref_put(ttyprintk_driver);
    tty_port_destroy(&tpk_port.port);
    return ret;
}

static void __exit ttyprintk_exit(void)
{
    tty_unregister_driver(ttyprintk_driver);
    tty_driver_kref_put(ttyprintk_driver);
    tty_port_destroy(&tpk_port.port);
    printk(KERN_ALERT "My ttyprintk Module exit\n");
}

device_initcall(ttyprintk_init);
module_exit(ttyprintk_exit);

MODULE_LICENSE("GPL");