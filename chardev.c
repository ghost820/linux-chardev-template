#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/module.h>

#define DRV_NAME "chardev"
#define DRV_CLASS_NAME "chardev"

MODULE_AUTHOR("Michal Miladowski <michal.miladowski@gmail.com>");
MODULE_DESCRIPTION("Character Device Driver Template");
MODULE_LICENSE("GPL");

static const struct file_operations chardev_fileops = {
	.owner = THIS_MODULE,
};

static dev_t chardev_id;
static struct class *chardev_class;
static struct cdev chardev_cdev;

static int __init chardev_init(void)
{
	int rc;

	rc = alloc_chrdev_region(&chardev_id, 0, 1, DRV_NAME);
	if (rc < 0) {
		pr_err("%s: failed to allocate char dev region\n", DRV_NAME);
		return rc;
	}

	chardev_class = class_create(DRV_CLASS_NAME);
	if (IS_ERR(chardev_class)) {
		pr_err("%s: failed to allocate class\n", DRV_NAME);
		unregister_chrdev_region(chardev_id, 1);
		return PTR_ERR(chardev_class);
	}

	cdev_init(&chardev_cdev, &chardev_fileops);
	chardev_cdev.owner = THIS_MODULE;
	cdev_add(&chardev_cdev, chardev_id, 1);

	device_create(chardev_class, NULL, chardev_id, NULL, DRV_NAME);

	return 0;
}

static void __exit chardev_exit(void)
{
	device_destroy(chardev_class, chardev_id);
	cdev_del(&chardev_cdev);
	class_unregister(chardev_class);
	class_destroy(chardev_class);
	unregister_chrdev_region(chardev_id, 1);
}

module_init(chardev_init);
module_exit(chardev_exit);
