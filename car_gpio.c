#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>

static struct gpio_desc *led_desc;
static struct gpio_desc *button_desc;

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
    int raw = gpio_get_value(539);
    int pressed = !raw;   // invert: 0 (grounded) means pressed, so report 1
    char kbuf[2];
    kbuf[0] = pressed + '0';
    kbuf[1] = '\0';

    if (copy_to_user(buf, kbuf, 2)) {
        return -EFAULT;
    }
    printk("car_gpio_read: successful. Read value -> %d\n", pressed);
    return 2;
}

static ssize_t car_gpio_write(struct file *file, const char *buf, size_t count, loff_t *ppos) {
    char kbuf[2];
    if (copy_from_user(kbuf, buf, 1)) {
        return -EFAULT;
    }
    int value = kbuf[0] - '0';  // convert ASCII '0'/'1' to actual int 0/1
    gpiod_set_value(led_desc, value);
    return count;
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

    /* Initialize the GPIO pins for input and output */
    led_desc = gpio_to_desc(529);
    if (!led_desc) {
        printk(KERN_ERR "Failed to get descriptor for GPIO17\n");
        return -ENODEV;
    }

    button_desc = gpio_to_desc(539);
    if (!button_desc) {
        printk(KERN_ERR "Failed to get descriptor for GPIO27\n");
        return -ENODEV;
    }

    gpiod_direction_output(led_desc, 0);
    gpiod_direction_input(button_desc);
    return 0;
}


static void __exit car_gpio_exit(void) {
    cdev_del(&car_gpio_cdev);
    unregister_chrdev_region(dev_num, 1);
    gpio_set_value(17, 0);       // turn LED off cleanly on unload
    gpio_free(17);
    gpio_free(27);
    printk("Car GPIO Exit\n");
}

module_init(car_gpio_init);
module_exit(car_gpio_exit);

MODULE_AUTHOR("Garret Tullio");
MODULE_DESCRIPTION("Car GPIO Driver");
MODULE_LICENSE("GPL");

