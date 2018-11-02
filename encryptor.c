#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/device.h>

MODULE_LICENSE("GPL");

#define MAX_COUNT = 19
#define MAX_BUFFER_LENGTH = 256
#define MAX_KEY_LENGTH = 32

//holds 10 encrpyt/decrypt pairs, as well as cryptctl

int cryptctl_major_number = 0;
struct device *devices[MAX_COUNT];
struct class *cryptctl_class = NULL;

int count = 0;
int available_minor_number = 0;

static int cryptctl_open(struct inode *pinode, struct file *pfile){
	return 0;
}

static ssize_t cryptctl_read(struct file *pfile, char __user *buffer, size_t length , loff_t *offset){
	return 0;
}

static ssize_t cryptctl_write(struct file *pfile, const char __user *buffer, size_t length, loff_t *offset){
	return length;
}

static int cryptctl_close(struct inode *pinode, struct file *pfile){
	return 0;
}

static long cryptctl_ioctl(struct file *pfile, unsigned int cmd, unsigned long arg){
	return 0;
}



struct file_operations crpytctl_file_operations = {
	.owner   = THIS_MODULE,
	.open    = cryptctl_open,
	.read    = cryptctl_read,
	.write   = cryptctl_write,
	.release = cryptctl_close,
	.unlocked_ioctl = cryptctl_ioctl
};

int cryptctl_init(void){
	printk(KERN_ALERT "Inside %s\n", __FUNCTION__);
	
	int i;
	for(i=0; i<MAX_COUNT; i++){
		devices[i] = NULL;
	}
	dev_t device_region;
	int err = alloc_chrdev_region(&device_region, 0, MAX_COUNT, "cryptctl");
	if(err < 0){
		printk(KERN_ALERT "alloc_chrdev_region() failed\n");
		return err;
	}
	
	cryptctl_major_number = MAJOR(device_region);
	cryptctl_class = class_create(THIS_MODULE, "cryptctl");
	if(IS_ERR(cryptctl_class)){
		err = PTR_ERR(cryptctl_class);
		printk(KERN_ALERT "class_create() failed\n");
		return err;
	}
	
	devices[0] == device_create(cryptctl_class, NULL, dev, NULL, "cryptctl");
	if(devices[0] == NULL){
			class_destroy(cryptctl_class);
			unregister_chrdev_region(device_region, MAX_COUNT);
			printk(KERN_ALERT "device_create() failed\n");
			return -1;
	}
	
	//device created
	
	
	
	
	
	return 0;
}

void cryptctl_exit(void){
	printk(KERN_ALERT "Inside %s\n", __FUNCTION__);
	//must destroy class with class_destroy
	
	
}

module_init(cryptctl_init);
module_exit(cryptctl_exit);