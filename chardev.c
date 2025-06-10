#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#define DRV_NAME "chardev"
#define DRV_CLASS_NAME "chardev"

#define CHARDEV_BUFSIZE 128

MODULE_AUTHOR("Michal Miladowski <michal.miladowski@gmail.com>");
MODULE_DESCRIPTION("Character Device Driver Template");
MODULE_LICENSE("GPL");

struct chardev_data {
	struct cdev cdev;
	u8 *buffer;
	bool is_open;
};

static DEFINE_MUTEX(chardev_mutex);

static dev_t chardev_id;
static struct class *chardev_class;
static struct chardev_data chardev;

static ssize_t chardev_write(struct file *filp, const char __user *buf,
	size_t count, loff_t *f_pos)
{
	struct chardev_data *dev = filp->private_data;

	mutex_lock(&chardev_mutex);

	if (*f_pos + count > CHARDEV_BUFSIZE) {
		count = CHARDEV_BUFSIZE - *f_pos;
	}

	if (!count) {
		mutex_unlock(&chardev_mutex);
		return -EFBIG;
	}

	if (copy_from_user(dev->buffer + *f_pos, buf, count) != 0) {
		mutex_unlock(&chardev_mutex);
		return -EFAULT;
	}

	*f_pos += count;

	mutex_unlock(&chardev_mutex);

	return count;
}

static ssize_t chardev_read(struct file *filp, char __user *buf,
	size_t count, loff_t *f_pos)
{
	struct chardev_data *dev = filp->private_data;

	mutex_lock(&chardev_mutex);

	if (*f_pos + count > CHARDEV_BUFSIZE) {
		count = CHARDEV_BUFSIZE - *f_pos;
	}

	if (copy_to_user(buf, dev->buffer + *f_pos, count) != 0) {
		mutex_unlock(&chardev_mutex);
		return -EFAULT;
	}

	*f_pos += count;

	mutex_unlock(&chardev_mutex);

	return count;
}

static int chardev_open(struct inode *inode, struct file *filp)
{
	struct chardev_data *dev;

	if (MAJOR(chardev_id) != imajor(inode) ||
	    MINOR(chardev_id) != iminor(inode)) {
		pr_err("%s: device not found\n", DRV_NAME);
		return -ENODEV;
	}

	dev = container_of(inode->i_cdev, struct chardev_data, cdev);

	mutex_lock(&chardev_mutex);
	if (dev->is_open) {
		mutex_unlock(&chardev_mutex);
		return -EBUSY;
	}
	dev->buffer = kzalloc(CHARDEV_BUFSIZE, GFP_KERNEL);
	if (!dev->buffer) {
		mutex_unlock(&chardev_mutex);
		return -ENOMEM;
	}
	dev->is_open = true;
	filp->private_data = dev;
	mutex_unlock(&chardev_mutex);

	return 0;
}

static int chardev_release(struct inode *inode, struct file *filp)
{
	struct chardev_data *dev = filp->private_data;

	mutex_lock(&chardev_mutex);
	kfree(dev->buffer);
	dev->buffer = NULL;
	dev->is_open = false;
	filp->private_data = NULL;
	mutex_unlock(&chardev_mutex);

	return 0;
}

static const struct file_operations chardev_fileops = {
	.owner = THIS_MODULE,
	.write = chardev_write,
	.read = chardev_read,
	.open = chardev_open,
	.release = chardev_release,
};

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

	cdev_init(&chardev.cdev, &chardev_fileops);
	chardev.cdev.owner = THIS_MODULE;
	cdev_add(&chardev.cdev, chardev_id, 1);

	device_create(chardev_class, NULL, chardev_id, NULL, DRV_NAME);

	return 0;
}

static void __exit chardev_exit(void)
{
	device_destroy(chardev_class, chardev_id);
	cdev_del(&chardev.cdev);
	class_destroy(chardev_class);
	unregister_chrdev_region(chardev_id, 1);
}

module_init(chardev_init);
module_exit(chardev_exit);
