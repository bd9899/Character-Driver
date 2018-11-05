#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>

MODULE_LICENSE("GPL");

#define MAX_COUNT 20
#define MAX_BUFFER_LENGTH 256
#define MAX_KEY_LENGTH 32

#define CRYPT_IOC_MAGIC (0xAA)

typedef struct configure_input {
	int index;
	char key[33];
} configure_input;

#define CRYPT_CREATE _IO(CRYPT_IOC_MAGIC, 0)
#define CRYPT_DESTROY _IOWR(CRYPT_IOC_MAGIC, 1, int)
#define CRYPT_CONFIGURE _IOWR(CRYPT_IOC_MAGIC, 2, configure_input)

//holds 10 encrpyt/decrypt pairs, as well as cryptctl
int dev_major_number = 0;
struct cryptctl_dev *cryptctl = NULL;
struct class *cryptctl_class = NULL;

struct encrypt_decrypt_dev{
	char key[MAX_KEY_LENGTH+1]; //+1 is for null terminating character
	char message[MAX_BUFFER_LENGTH+1];
	int minor_number; //minor number for this device
	struct cdev ed_cdev;
};

struct cryptctl_dev{
	//might have to add new fields, not sure for now
	int count; //total number of devices created thus far (excluding cryptctl)
	int minor_number;
	struct encrypt_decrypt_dev *devs[MAX_COUNT];
	struct cdev cryptctl_cdev;
};


static int cryptctl_open(struct inode *pinode, struct file *pfile){
	printk(KERN_ALERT "Inside %s\n", __FUNCTION__);
	return 0;
}

static ssize_t cryptctl_read(struct file *pfile, char __user *buffer, size_t length , loff_t *offset){
	printk(KERN_ALERT "Inside %s\n", __FUNCTION__);
	return 0;
}

static ssize_t cryptctl_write(struct file *pfile, const char __user *buffer, size_t length, loff_t *offset){
	printk(KERN_ALERT "Inside %s\n", __FUNCTION__);
	return length;
}

static int cryptctl_close(struct inode *pinode, struct file *pfile){
	printk(KERN_ALERT "Inside %s\n", __FUNCTION__);
	return 0;
}

static long cryptctl_ioctl(struct file *pfile, unsigned int cmd, unsigned long arg){
	int err = 0;
	int retval = 0;

	if (_IOC_TYPE(cmd) != CRYPT_IOC_MAGIC) {
		return -ENOTTY;
	}
	printk(KERN_ALERT "Inside %s\n", __FUNCTION__);
	switch (cmd) {
		case CRYPT_CREATE:
			//create key
			break;
		case CRYPT_DESTROY:
			//delete key
			break;
		case CRYPT_CONFIGURE:
			//change key
			break;
		default:
			return -ENOTTY;
	}
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

struct file_operations ed_dev_file_operations = {
	.owner   = THIS_MODULE,
	.open    = cryptctl_open,
	.read    = cryptctl_read,
	.write   = cryptctl_write,
	.release = cryptctl_close
};

static int cryptctl_init(void){
	dev_t device_region;
	struct device *my_device = NULL;
	int i;
	int err;
	
	printk(KERN_ALERT "Inside %s\n", __FUNCTION__);
	
	cryptctl_class = class_create(THIS_MODULE, "cryptctl");
	if(IS_ERR(cryptctl_class)){
		err = PTR_ERR(cryptctl_class);
		printk(KERN_ALERT "class_create() failed\n");
		return err;
	}
	
	err = alloc_chrdev_region(&device_region, 0, 1, "cryptctl");
	
	if(err < 0){
		printk(KERN_ALERT "alloc_chrdev_region() failed\n");
		return err;
	}
	
	dev_major_number = MAJOR(device_region);

	cryptctl = (struct cryptctl_dev *)kmalloc(sizeof(struct cryptctl_dev), GFP_KERNEL);

	
	for(i=0; i<MAX_COUNT; i++){
		cryptctl->devs[i] = NULL;
	}
	cryptctl->count = 0;
	cryptctl->minor_number = MINOR(device_region);
	cdev_init(&cryptctl->cryptctl_cdev, &crpytctl_file_operations);
	cryptctl->cryptctl_cdev.owner = THIS_MODULE;
	
	err = cdev_add(&cryptctl->cryptctl_cdev, device_region, 1);
	
	if(err){
		printk(KERN_ALERT "Failed to create cryptctl\n");
		return err;
	}
	
	my_device = device_create(cryptctl_class, NULL, device_region, NULL, "cryptctl");
	
	if(IS_ERR(my_device)){
		err = PTR_ERR(my_device);
		printk(KERN_ALERT "device_create() for cryptctl creation\n");
		cdev_del(&cryptctl->cryptctl_cdev);
		return err;
	}
	
	return 0; //successfully created cryptctl

}

static void cryptctl_exit(void){
	printk(KERN_ALERT "Inside %s\n", __FUNCTION__);
	//must destroy class with class_destroy
	
	if(cryptctl){
		int i;
		for(i=0; i < MAX_COUNT; i++){
			if(cryptctl->devs[i] != NULL){
				device_destroy(cryptctl_class, MKDEV(dev_major_number, cryptctl->devs[i]->minor_number));
				cdev_del(&cryptctl->devs[i]->ed_cdev);
				unregister_chrdev_region(MKDEV(dev_major_number, cryptctl->devs[i]->minor_number),1);
				cryptctl->devs[i] = NULL;
			}
		}
		//deleted all of the existing encrpyt/decrypt devices
		device_destroy(cryptctl_class, MKDEV(dev_major_number, cryptctl->minor_number));
		cdev_del(&cryptctl->cryptctl_cdev);
		class_destroy(cryptctl_class);
		unregister_chrdev_region(MKDEV(dev_major_number,cryptctl->minor_number),1);
		cryptctl_class = NULL;
		cryptctl = NULL;
	}
	
	return;
}

module_init(cryptctl_init);
module_exit(cryptctl_exit);
