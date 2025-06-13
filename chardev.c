#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define DRV_NAME "chardev"
#define DRV_CLASS_NAME "chardev"

#define CHARDEV_BUFSIZE 16

MODULE_AUTHOR("Michal Miladowski <michal.miladowski@gmail.com>");
MODULE_DESCRIPTION("Character Device Driver Template");
MODULE_LICENSE("GPL");

struct chardev_data {
	struct cdev cdev;
	u8 *buffer;
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

static loff_t chardev_lseek(struct file *filp, loff_t off, int whence)
{
	loff_t tmp;

	mutex_lock(&chardev_mutex);

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
			mutex_unlock(&chardev_mutex);
			return -EINVAL;
	}

	if (tmp > CHARDEV_BUFSIZE || tmp < 0) {
		mutex_unlock(&chardev_mutex);
		return -EINVAL;
	}

	filp->f_pos = tmp;

	mutex_unlock(&chardev_mutex);

	return tmp;
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
	int rc;
	struct device *dev;

	rc = alloc_chrdev_region(&chardev_id, 0, 1, DRV_NAME);
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

	cdev_init(&chardev.cdev, &chardev_fileops);
	chardev.cdev.owner = THIS_MODULE;
	rc = cdev_add(&chardev.cdev, chardev_id, 1);
	if (rc < 0) {
  		pr_err("%s: failed to add cdev\n", DRV_NAME);
  		goto err_cdev_add;
 	}

	dev = device_create(chardev_class, NULL, chardev_id, NULL, DRV_NAME);
	if (IS_ERR(dev)) {
		pr_err("%s: failed to create device\n", DRV_NAME);
		rc = PTR_ERR(dev);
		goto err_device_create;
	}

	chardev.buffer = kzalloc(CHARDEV_BUFSIZE, GFP_KERNEL);
	if (!chardev.buffer) {
		pr_err("%s: failed to allocate device buffer\n", DRV_NAME);
		rc = -ENOMEM;
		goto err_alloc;
  	}

	return 0;

err_alloc:
	device_destroy(chardev_class, chardev_id);
err_device_create:
	cdev_del(&chardev.cdev);
err_cdev_add:
	class_destroy(chardev_class);
err_class_create:
	unregister_chrdev_region(chardev_id, 1);
err_chrdev:
	return rc;
}

static void __exit chardev_exit(void)
{
	kfree(chardev.buffer);
	device_destroy(chardev_class, chardev_id);
	cdev_del(&chardev.cdev);
	class_destroy(chardev_class);
	unregister_chrdev_region(chardev_id, 1);
}

module_init(chardev_init);
module_exit(chardev_exit);
