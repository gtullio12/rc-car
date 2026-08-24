#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>

#define GPIO5 517
#define GPIO6 518
#define GPIO13 525
#define GPIO19 531

static struct gpio_desc *rc_car_forward;
static struct gpio_desc *rc_car_reverse;
static struct gpio_desc *rc_car_left;
static struct gpio_desc *rc_car_right;

static dev_t dev_num;        // new: holds allocated major+minor
static struct cdev car_gpio_cdev;  // new: our device's cdev structure
                                   //
static int debug_enable = 0;
module_param(debug_enable, int, 0);
MODULE_PARM_DESC(debug_enable, "Enable module debug mode.");


static int car_gpio_open(struct inode *inode, struct file *file) {
    printk("car_gpio_open: successful\n");
    return 0;
}

static int car_gpio_release(struct inode *inode, struct file *file) {
    printk("car_gpio_release: successful\n");
    return 0;
}

static ssize_t car_gpio_read(struct file *file, char *buf, size_t count, loff_t *ptr) {
    char kbuf[5];
    kbuf[0] = !gpiod_get_value(rc_car_forward) + '0';  // byte 0: forward
    kbuf[1] = !gpiod_get_value(rc_car_reverse) + '0';      // byte 1: back
    kbuf[2] = gpiod_get_value(rc_car_left) + '0';      // byte 2: left
    kbuf[3] = gpiod_get_value(rc_car_right) + '0';     // byte 3: right
    kbuf[4] = '\0';

    if (copy_to_user(buf, kbuf, 5)) {
        return -EFAULT;
    }
    return 5;
}

static ssize_t car_gpio_write(struct file *file, const char *buf, size_t count, loff_t *ppos) {
    /*
    char kbuf[2];
    if (copy_from_user(kbuf, buf, 1)) {
        return -EFAULT;
    }
    int value = kbuf[0] - '0';  // convert ASCII '0'/'1' to actual int 0/1
    gpiod_set_value(led_desc, value);
    return count;
    */
    return 0;
}

struct file_operations car_gpio_fops = {
    .owner = THIS_MODULE,
    .read = car_gpio_read,
    .write = car_gpio_write,
    .open = car_gpio_open,
    .release = car_gpio_release
};


static int __init car_gpio_init(void) {
    int ret;
    printk("Car GPIO Init - debug mode is %s\n", debug_enable ? "enabled" : "disabled");

    // ask kernel for a dynamically allocated major number, 1 minor, starting at minor 0
    ret = alloc_chrdev_region(&dev_num, 0, 1, "car_gpio");
    if (ret < 0) {
        printk(KERN_ERR "Failed to allocate char device region\n");
        return ret;
    }

    cdev_init(&car_gpio_cdev, &car_gpio_fops);
    car_gpio_cdev.owner = THIS_MODULE;

    ret = cdev_add(&car_gpio_cdev, dev_num, 1);
    if (ret < 0) {
        printk(KERN_ERR "Failed to add cdev\n");
        unregister_chrdev_region(dev_num, 1);
        return ret;
    }

    printk("Car GPIO: registered successfully, major=%d minor=%d\n",
            MAJOR(dev_num), MINOR(dev_num));

    /* Initialize the GPIO pins 5,6,13,19 */
    rc_car_forward = gpio_to_desc(GPIO6);
    if (!rc_car_forward) {
        printk(KERN_ERR "Failed to get descriptor for GPIO5\n");
        return -ENODEV;
    }
    printk("Successfully initialized GPIO6");

    rc_car_reverse = gpio_to_desc(GPIO5);
    if (!rc_car_reverse) {
        printk(KERN_ERR "Failed to get descriptor for GPIO6\n");
        return -ENODEV;
    }
    printk("Successfully initialized GPIO5");

    rc_car_left = gpio_to_desc(GPIO19);
    if (!rc_car_left) {
        printk(KERN_ERR "Failed to get descriptor for GPIO13\n");
        return -ENODEV;
    }
    printk("Successfully initialized GPIO19");

    rc_car_right = gpio_to_desc(GPIO13);
    if (!rc_car_right) {
        printk(KERN_ERR "Failed to get descriptor for GPIO19\n");
        return -ENODEV;
    }
    printk("Successfully initialized GPIO13");

    gpiod_direction_input(rc_car_forward);
    gpiod_direction_input(rc_car_reverse);
    gpiod_direction_input(rc_car_left);
    gpiod_direction_input(rc_car_right);
    return 0;
}


static void __exit car_gpio_exit(void) {
    cdev_del(&car_gpio_cdev);
    unregister_chrdev_region(dev_num, 1);
    gpio_free(GPIO5);
    gpio_free(GPIO6);
    gpio_free(GPIO13);
    gpio_free(GPIO19);
    printk("Car GPIO Exit\n");
}

module_init(car_gpio_init);
module_exit(car_gpio_exit);

MODULE_AUTHOR("Garret Tullio");
MODULE_DESCRIPTION("Car GPIO Driver");
MODULE_LICENSE("GPL");

