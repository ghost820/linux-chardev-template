#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define DRV_NAME "chardev"
#define DRV_CLASS_NAME "chardev"

#define NUM_OF_DEVS 4

#define CHARDEV_BUFSIZE 16

MODULE_AUTHOR("Michal Miladowski <michal.miladowski@gmail.com>");
MODULE_DESCRIPTION("Character Device Driver Template");
MODULE_LICENSE("GPL");

struct chardev_data {
	struct cdev cdev;
	u8 *buffer;
	struct mutex mutex;
};

static dev_t chardev_id;
static struct class *chardev_class;
static struct chardev_data chardev[NUM_OF_DEVS];

static ssize_t chardev_write(struct file *filp, const char __user *buf,
	size_t count, loff_t *f_pos)
{
	struct chardev_data *dev = filp->private_data;

	mutex_lock(&dev->mutex);

	if (*f_pos + count > CHARDEV_BUFSIZE) {
		count = CHARDEV_BUFSIZE - *f_pos;
	}

	if (!count) {
		mutex_unlock(&dev->mutex);
		return -EFBIG;
	}

	if (copy_from_user(dev->buffer + *f_pos, buf, count) != 0) {
		mutex_unlock(&dev->mutex);
		return -EFAULT;
	}

	*f_pos += count;

	mutex_unlock(&dev->mutex);

	return count;
}

static ssize_t chardev_read(struct file *filp, char __user *buf,
	size_t count, loff_t *f_pos)
{
	struct chardev_data *dev = filp->private_data;

	mutex_lock(&dev->mutex);

	if (*f_pos + count > CHARDEV_BUFSIZE) {
		count = CHARDEV_BUFSIZE - *f_pos;
	}

	if (copy_to_user(buf, dev->buffer + *f_pos, count) != 0) {
		mutex_unlock(&dev->mutex);
		return -EFAULT;
	}

	*f_pos += count;

	mutex_unlock(&dev->mutex);

	return count;
}

static loff_t chardev_lseek(struct file *filp, loff_t off, int whence)
{
	struct chardev_data *dev = filp->private_data;
	loff_t tmp;

	mutex_lock(&dev->mutex);

	switch (whence) {
		case SEEK_SET:
			tmp = off;
			break;
		case SEEK_CUR:
			tmp = filp->f_pos + off;
			break;
		case SEEK_END:
			tmp = CHARDEV_BUFSIZE + off;
			break;
		default:
			mutex_unlock(&dev->mutex);
			return -EINVAL;
	}

	if (tmp > CHARDEV_BUFSIZE || tmp < 0) {
		mutex_unlock(&dev->mutex);
		return -EINVAL;
	}

	filp->f_pos = tmp;

	mutex_unlock(&dev->mutex);

	return tmp;
}

static int chardev_open(struct inode *inode, struct file *filp)
{
	struct chardev_data *dev;

	dev = container_of(inode->i_cdev, struct chardev_data, cdev);

	filp->private_data = dev;

	return 0;
}

static int chardev_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static const struct file_operations chardev_fileops = {
	.owner = THIS_MODULE,
	.write = chardev_write,
	.read = chardev_read,
	.llseek = chardev_lseek,
	.open = chardev_open,
	.release = chardev_release,
};

static int __init chardev_init(void)
{
	int i;
	int rc;
	dev_t dev_id;
	struct device *dev;

	rc = alloc_chrdev_region(&chardev_id, 0, NUM_OF_DEVS, DRV_NAME);
	if (rc < 0) {
		pr_err("%s: failed to allocate char dev region\n", DRV_NAME);
		goto err_chrdev;
	}

	chardev_class = class_create(DRV_CLASS_NAME);
	if (IS_ERR(chardev_class)) {
		pr_err("%s: failed to allocate class\n", DRV_NAME);
		rc = PTR_ERR(chardev_class);
		goto err_class_create;
	}

	for (i = 0; i < NUM_OF_DEVS; i++) {
		dev_id = MKDEV(MAJOR(chardev_id), MINOR(chardev_id) + i);
		cdev_init(&chardev[i].cdev, &chardev_fileops);
		chardev[i].cdev.owner = THIS_MODULE;
		rc = cdev_add(&chardev[i].cdev, dev_id, 1);
		if (rc < 0) {
			pr_err("%s: failed to add cdev\n", DRV_NAME);
			goto err_cdev_add;
		}

		dev = device_create(chardev_class, NULL, dev_id, NULL,
							"%s%d", DRV_NAME, i);
		if (IS_ERR(dev)) {
			pr_err("%s: failed to create device\n", DRV_NAME);
			cdev_del(&chardev[i].cdev);
			rc = PTR_ERR(dev);
			goto err_device_create;
		}

		chardev[i].buffer = kzalloc(CHARDEV_BUFSIZE, GFP_KERNEL);
		if (!chardev[i].buffer) {
			pr_err("%s: failed to allocate device buffer\n", DRV_NAME);
			device_destroy(chardev_class, dev_id);
			cdev_del(&chardev[i].cdev);
			rc = -ENOMEM;
			goto err_alloc;
		}

		mutex_init(&chardev[i].mutex);
	}

	return 0;

err_alloc:
err_device_create:
err_cdev_add:
	for (i--; i >= 0; i--) {
		dev_id = MKDEV(MAJOR(chardev_id), MINOR(chardev_id) + i);
		kfree(chardev[i].buffer);
		device_destroy(chardev_class, dev_id);
		cdev_del(&chardev[i].cdev);
	}
	class_destroy(chardev_class);
err_class_create:
	unregister_chrdev_region(chardev_id, NUM_OF_DEVS);
err_chrdev:
	return rc;
}

static void __exit chardev_exit(void)
{
	int i;
	dev_t dev_id;

	for (i = 0; i < NUM_OF_DEVS; i++) {
		dev_id = MKDEV(MAJOR(chardev_id), MINOR(chardev_id) + i);
		kfree(chardev[i].buffer);
		device_destroy(chardev_class, dev_id);
		cdev_del(&chardev[i].cdev);
 	}
	class_destroy(chardev_class);
	unregister_chrdev_region(chardev_id, NUM_OF_DEVS);
}

module_init(chardev_init);
module_exit(chardev_exit);
