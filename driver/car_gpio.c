#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>

#define GPIO5 517
#define GPIO6 518
#define GPIO13 525
#define GPIO19 531
#define GPIO20 532
#define GPIO21 533
#define GPIO26 538
#define GPIO16 528

#define NUM_INPUT_PINS 4
#define NUM_OUTPUT_PINS 4

static struct gpio_desc *input_pins[NUM_INPUT_PINS];
static struct gpio_desc *output_pins[NUM_OUTPUT_PINS];

static int input_gpio_numbers[NUM_INPUT_PINS] = {GPIO5, GPIO6, GPIO13, GPIO19};
static int output_gpio_numbers[NUM_OUTPUT_PINS] = {GPIO20, GPIO21, GPIO26, GPIO16};

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
    char kbuf[NUM_INPUT_PINS+1];
    int i;
    for (i=0; i<NUM_INPUT_PINS; i++) {
        kbuf[i] = gpiod_get_value(input_pins[i]) + '0'; 

    }
    kbuf[NUM_INPUT_PINS] = '\0';

    if (copy_to_user(buf, kbuf, NUM_INPUT_PINS+1)) {
        return -EFAULT;
    }
    return NUM_INPUT_PINS+1;
}

static ssize_t car_gpio_write(struct file *file, const char *buf, size_t count, loff_t *ppos) {

    char kbuf[NUM_OUTPUT_PINS];
    if (count < NUM_OUTPUT_PINS) {
        return -EINVAL;
    }
    if (copy_from_user(kbuf, buf, NUM_OUTPUT_PINS)) {
        return -EFAULT;
    }

    int i;
    for (i=0; i < NUM_OUTPUT_PINS; i++) {
        gpiod_set_value(output_pins[i], kbuf[i] - '0');
    }

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

    int i;
    for (i=0; i<NUM_INPUT_PINS; i++) {
        input_pins[i] = gpio_to_desc(input_gpio_numbers[i]);
        if (!input_pins[i]) {
            printk(KERN_ERR "Failed to get descriptor for %d\n",input_gpio_numbers[i]);
            return -ENODEV;
        }
        printk("Successfully initialized %d\n", input_gpio_numbers[i]);
        gpiod_direction_input(input_pins[i]);
    }

    for (i=0; i<NUM_OUTPUT_PINS; i++) {
        output_pins[i] = gpio_to_desc(output_gpio_numbers[i]);
        if (!output_pins[i]) {
            printk(KERN_ERR "Failed to get descriptor for %d\n",output_gpio_numbers[i]);
            return -ENODEV;
        }
        printk("Successfully initialized %d\n", output_gpio_numbers[i]);
        gpiod_direction_output(output_pins[i], 0);
    }

    return 0;
}


static void __exit car_gpio_exit(void) {
    cdev_del(&car_gpio_cdev);
    unregister_chrdev_region(dev_num, 1);

    int i;
    for (i=0; i<NUM_INPUT_PINS; i++) {
        gpio_free(input_gpio_numbers[i]);
    }

    for (i=0; i<NUM_OUTPUT_PINS; i++) {
        gpiod_set_value(output_pins[i], 0);
        gpio_free(output_gpio_numbers[i]);
    }

    printk("Car GPIO Exit\n");
}

module_init(car_gpio_init);
module_exit(car_gpio_exit);

MODULE_AUTHOR("Garret Tullio");
MODULE_DESCRIPTION("Car GPIO Driver");
MODULE_LICENSE("GPL");

