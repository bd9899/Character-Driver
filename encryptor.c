#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <string.h>

MODULE_LICENSE("GPL");

#define MOD(a,b) ((((a)%(b))+(b))%(b))
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
	char *key; //+1 is for null terminating character
	char *message;
	int minor_number; //minor number for this device
	struct cdev ed_cdev;
};

struct cryptctl_dev{
	//might have to add new fields, not sure for now
	int count; //total number of devices created thus far (excluding cryptctl)
	int minor_number;
	int encrypt; //0 if encryptor, 1 if decryptor
	char *buffer;
	struct encrypt_decrypt_dev *devs[MAX_COUNT];
	struct cdev cryptctl_cdev;
};

static void encrypt(char *key, char *message){
	int i;
	int message_len = strlen(message);
	int key_len = strlen(key);
	int num_into_message = message_len % key_len;
	int left_over = message_len - num_into_message*key_len;
	
	char *full_key = (char *)kzalloc(strlen(message_len)+1);
	if(num_into_message > 0){
		strcpy(full_key, key);
		num_into_message - 1;
		while(num_into_message > 0){
			strcat(full_key, key);
			num_into_message = num_into_message - 1;
		}
	}
	
	if(left_over > 0){
		strncat(full_key, key, left_over);
	}
	
	for(i=0; i < message_len; i++){
		message[i] = MOD((message[i] + full_key[i]) % 128);
	}
}

static void decrypt(char *key, char *message){
	int i;
	int message_len = strlen(message);
	int key_len = strlen(key);
	int num_into_message = message_len % key_len;
	int left_over = message_len - num_into_message*key_len;
	
	char *full_key = (char *)kzalloc(strlen(message_len)+1);
	if(num_into_message > 0){
		strcpy(full_key, key);
		num_into_message - 1;
		while(num_into_message > 0){
			strcat(full_key, key);
			num_into_message = num_into_message - 1;
		}
	}
	
	if(left_over > 0){
		strncat(full_key, key, left_over);
	}
	for(i=0; i < message_len; i++){
		MOD(message[i] = (message[i] - full_key[i]) % 128);
	}
	
}

static int cryptctl_open(struct inode *pinode, struct file *pfile){
	
	unsigned int major = imajor(pinode);
	unsigned int minor = iminor(pinode);
	
	printk(KERN_ALERT "Inside %s\n", __FUNCTION__);
	
	if(major != dev_major_number || minor < 0){
		printk(KERN_ALERT "No device found with major %d and minor %d\n", major, minor);
		return -ENODEV;
	}
	
	if(minor == cryptctl->minor_number){ //cryptctl being opened
		if(pinode->i_cdev != &(cryptctl->cryptctl_cdev)){
			printk(KERN_ALERT "No device found with major %d and minor %d\n", major, minor);
			return -ENODEV;
		}
		pfile->private_data = cryptctl;
		
		if(cryptctl->buffer == NULL){
			cryptctl->buffer = (char *)kzalloc(MAX_BUFFER_LENGTH, GFP_KERNEL);
			if(cryptctl->buffer == NULL){
				printk(KERN_ALERT "Out of memory\n");
				return -ENOMEM;
			}
		}
	}else{
		int i;
		struct encrypt_decrypt_dev *dev = NULL;
		
		for(i=0; i<MAX_COUNT; i++){
			if(cryptctl->devs[i] == NULL){
				continue;
			}else{
				if(cryptctl->devs[i]->minor_number == minor){
					dev = cryptctl->devs[i];
					pfile->private_data = dev;
					break;
				}
			}
		}
		
		if(dev == NULL){
			printk(KERN_ALERT "No device found with major %d and minor %d\n", major, minor);
			return -ENODEV;
		}
		
		if(pinode->i_cdev != &(dev->ed_cdev)){
			printk(KERN_ALERT "No device found with major %d and minor %d\n", major, minor);
			return -ENODEV;
		}
	}	
	return 0;
}

static ssize_t cryptctl_read(struct file *pfile, char __user *buffer, size_t count, loff_t *offset){
	ssize_t retval = 0;	
	struct cryptctl_dev *dev = (struct cryptctl_dev *)pfile->private_data;
	
	printk(KERN_ALERT "Inside %s\n", __FUNCTION__);
	if(dev != cryptctl){
		printk(KERN_ALERT "pfile private data is not equal to cryptctl\n");
		return -ENODEV;
	}
	
	if(*offset >= MAX_BUFFER_LENGTH){
		return retval;
	}
	if(*offset+count > MAX_BUFFER_LENGTH){
		count = MAX_BUFFER_LENGTH - *offset;
	}
	if(copy_to_user(buffer, &(dev->buffer[*offset]), count) != 0){
		retval = -EFAULT;
		printk(KERN_ALERT "Error with copy_to_user()\n");
		return retval;
	}	
	*offset += count;
	retval =  count;
	return retval;
}
static ssize_t ed_dev_read(struct file *pfile, char __user *buffer, size_t count, loff_t *offset){
	ssize_t retval = 0;	
	struct encrypt_decrypt_dev *dev = (struct encrypt_decrypt_dev *)pfile->private_data;
	printk(KERN_ALERT "Inside %s\n", __FUNCTION__);

	
	if(*offset >= MAX_BUFFER_LENGTH){
		return retval;
	}
	if(*offset+count > MAX_BUFFER_LENGTH){
		count = MAX_BUFFER_LENGTH - *offset;
	}
	if(copy_to_user(buffer, &(dev->message[*offset]), count) != 0){
		retval = -EFAULT;
		printk(KERN_ALERT "Error with copy_to_user()\n");
		return retval;
	}	
	*offset += count;
	retval =  count;
	return retval;
}

static ssize_t cryptctl_write(struct file *pfile, const char __user *buffer, size_t count, loff_t *offset){
	struct cryptctl_dev *dev = (struct cryptctl_dev *)pfile->private_data;
	ssize_t retval = 0;
	printk(KERN_ALERT "Inside %s\n", __FUNCTION__);
	if(dev != cryptctl){
		printk(KERN_ALERT "pfile private data is not equal to cryptctl\n");
		return -ENODEV;
	}
	
	if(*offset >= MAX_BUFFER_LENGTH){
		retval = -EINVAL;
		return retval;
	}
	if(*offset + count > MAX_BUFFER_LENGTH){
		count = MAX_BUFFER_LENGTH - *offset;
	}
	if(copy_from_user(&(dev->buffer[*offset]), buffer, count) != 0){
		retval = -EFAULT;
		return retval;
	}
	
	*offset += count;
	retval = count;
	return retval;
}

static ssize_t ed_dev_write(struct file *pfile, const char __user *buffer, size_t count, loff_t *offset){
	printk(KERN_ALERT "Inside %s\n", __FUNCTION__);
	return count;
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
	.read    = ed_dev_read,
	.write   = ed_dev_write,
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
	cryptctl->buffer = NULL;
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
		kfree((void *)cryptctl->buffer);
		class_destroy(cryptctl_class);
		unregister_chrdev_region(MKDEV(dev_major_number,cryptctl->minor_number),1);
		cryptctl_class = NULL;
		cryptctl = NULL;
	}
	
	return;
}

module_init(cryptctl_init);
module_exit(cryptctl_exit);
