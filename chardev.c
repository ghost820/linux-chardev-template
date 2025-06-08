#include <linux/init.h>
#include <linux/module.h>

MODULE_AUTHOR("Michal Miladowski <michal.miladowski@gmail.com>");
MODULE_DESCRIPTION("Character Device Driver Template");
MODULE_LICENSE("GPL");

static int __init chardev_init(void)
{
	return 0;
}

static void __exit chardev_exit(void)
{
}

module_init(chardev_init);
module_exit(chardev_exit);
