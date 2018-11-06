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
#include <linux/string.h>

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

static char table[] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F','G','H','I','J','K','L','M','N',
'O','P','Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s',
't','u','v','w','x','y','z', ' ', '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.' , '/', ':', ';', '<',
'=', '>', '?', '`', '{', '}', '|', '~'};

int dev_major_number = 0;
struct cryptctl_dev *cryptctl = NULL;
struct class *cryptctl_class = NULL;
struct class *ed_dev_class = NULL;

struct encrypt_decrypt_dev{
	char *key; //+1 is for null terminating character
	char *message;
	int encrypt_or_decrypt; //0 if encryptor 1 if decryptor
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
	int j;
	int k;
	int message_len = strlen(message);
	int key_len = strlen(key);
	int num_into_message = message_len / key_len;
	int left_over = message_len % key_len;
		
	char *full_key = (char *)kzalloc(message_len+1, GFP_KERNEL);
	if(num_into_message > 0){
		strcpy(full_key, key);
		num_into_message -= 1;
		while(num_into_message > 0){
			strcat(full_key, key);
			num_into_message = num_into_message - 1;
		}
	}
	
	if(left_over > 0){
		strncat(full_key, key, left_over);
	}
	
	for(i=0; i < message_len; i++){
		for(j=0; j < 89; j++){
			if(message[i] == table[j]){
				for(k=0; k < 89; k++){
					if(full_key[i] == table[k]){
						message[i] = table[MOD((j+k),89)];
						break;
					}
				}
				break;
			}
		}
	}
}

static void decrypt(char *key, char *message){
	int i;
	int j;
	int k;
	int message_len = strlen(message);
	int key_len = strlen(key);
	int num_into_message = message_len / key_len;
	int left_over = message_len % key_len;
		
	char *full_key = (char *)kzalloc(message_len+1, GFP_KERNEL);
	if(num_into_message > 0){
		strcpy(full_key, key);
		num_into_message -= 1;
		while(num_into_message > 0){
			strcat(full_key, key);
			num_into_message = num_into_message - 1;
		}
	}
	
	if(left_over > 0){
		strncat(full_key, key, left_over);
	}
	
	for(i=0; i < message_len; i++){
		for(j=0; j < 89; j++){
			if(message[i] == table[j]){
				for(k=0; k < 89; k++){
					if(full_key[i] == table[k]){
						message[i] = table[MOD((j-k),89)];
						break;
					}
				}
				break;
			}
		}
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
			cryptctl->buffer = (char *)kzalloc(MAX_BUFFER_LENGTH+1, GFP_KERNEL);
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
	
	if(dev->key == NULL){
		return -5;
	}
	if(dev->encrypt_or_decrypt == 0){ //encrypt
		encrypt(dev->key, dev->message);
	}else{
		decrypt(dev->key, dev->message);
	}
	
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
	ssize_t retval = 0;	
	struct cryptctl_dev *dev = (struct cryptctl_dev *)pfile->private_data;
	
	printk(KERN_ALERT "Inside %s\n", __FUNCTION__);
	if(dev->buffer != NULL){
		kfree(dev->buffer);
		dev->buffer = (char *)kzalloc(MAX_BUFFER_LENGTH+1, GFP_KERNEL);
		*offset = 0;
	}
	if(dev != cryptctl){
		printk(KERN_ALERT "pfile private data is not equal to cryptctl\n");
		return -ENODEV;
	}
	
	if(*offset >= MAX_BUFFER_LENGTH+1){
		retval = -EINVAL;
		return retval;
	}
	if(*offset + count > MAX_BUFFER_LENGTH+1){
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
	ssize_t retval = 0;	
	struct encrypt_decrypt_dev *dev = (struct encrypt_decrypt_dev *)pfile->private_data;
	printk(KERN_ALERT "Inside %s\n", __FUNCTION__);
	
	if(dev->key == NULL){
		return -5;
	}
	if(dev->message != NULL){
		kfree(dev->message);
		dev->message = (char *)kzalloc(MAX_BUFFER_LENGTH+1, GFP_KERNEL);
		*offset = 0;
	}
	if(*offset >= MAX_BUFFER_LENGTH+1){
		retval = -EINVAL;
		return retval;
	}
	if(*offset + count > MAX_BUFFER_LENGTH+1){
		count = MAX_BUFFER_LENGTH - *offset;
	}
	if(copy_from_user(&(dev->message[*offset]), buffer, count) != 0){
		retval = -EFAULT;
		return retval;
	}
	
	*offset += count;
	retval = count;
	return retval;
}

static int cryptctl_close(struct inode *pinode, struct file *pfile){
	printk(KERN_ALERT "Inside %s\n", __FUNCTION__);
	return 0;
}

struct file_operations ed_dev_file_operations = {
	.owner   = THIS_MODULE,
	.open    = cryptctl_open,
	.read    = ed_dev_read,
	.write   = ed_dev_write,
	.release = cryptctl_close
};

static long cryptctl_ioctl(struct file *pfile, unsigned int cmd, unsigned long arg){
	int err = 0;
	long retval = 0;
	int err2;
	int i;
	char name_enc[15] = "cryptEncrypt";
	char name_dec[15] = "cryptDecrypt";
	char num[3];
	dev_t device_region_enc;
	dev_t device_region_dec;
	struct device *my_device_enc = NULL;
	struct device *my_device_dec = NULL;
	int *arg_num;
	int enc_index;
	int dec_index;
	configure_input *data;

	if (_IOC_TYPE(cmd) != CRYPT_IOC_MAGIC) {
		return -ENOTTY;
	}
	printk(KERN_ALERT "Inside %s\n", __FUNCTION__);
	switch (cmd) {
		case CRYPT_CREATE:
			//create pair
			;

			for(i=0; i<MAX_COUNT; i++){
				if(cryptctl->devs[i] == NULL){
					break;
				}
			}
			printk(KERN_ALERT "Inside %s\n", __FUNCTION__);
			if(i==MAX_COUNT){ //no spots available
				retval = -1;
				return retval;
			}
			retval = i/2; //number for pair
			if(retval >= 0 && retval < 10){
				snprintf(num, 2, "%ld", retval);
			}else{
				snprintf(num, 3, "%ld", retval);
			}
			printk(KERN_ALERT "Num: %s\n", num);
			strcat(name_enc, num);
			strcat(name_dec, num);
			device_region_enc = MKDEV(dev_major_number, 1+i);
			device_region_dec = MKDEV(dev_major_number, 2+i);
			
			
			ed_dev_class = class_create(THIS_MODULE, "ed_dev");
			if(IS_ERR(ed_dev_class)){
				printk(KERN_ALERT "class_create() failed\n");
				retval = -2;
				return retval;
			}
			
			err = register_chrdev_region(device_region_enc, 1, name_enc);
			err2 = register_chrdev_region(device_region_dec, 1, name_dec);
			if(err < 0 || err2 < 0){
				printk(KERN_ALERT "register_chrdev_region() failed\n");
				retval = -2;
				return retval;
			}

			cryptctl->devs[i] = (struct encrypt_decrypt_dev *)kmalloc(sizeof(struct encrypt_decrypt_dev), GFP_KERNEL);
			cryptctl->devs[i]->message = NULL;
			cryptctl->devs[i]->key = NULL;
			cryptctl->devs[i]->encrypt_or_decrypt = 0;
			cryptctl->devs[i]->minor_number = MINOR(device_region_enc);
			cdev_init(&cryptctl->devs[i]->ed_cdev, &ed_dev_file_operations);
			cryptctl->devs[i]->ed_cdev.owner = THIS_MODULE;
			err = cdev_add(&cryptctl->devs[i]->ed_cdev, device_region_enc, 1);
			
			cryptctl->devs[i+1] = (struct encrypt_decrypt_dev *)kmalloc(sizeof(struct encrypt_decrypt_dev), GFP_KERNEL);
			cryptctl->devs[i+1]->message = NULL;
			cryptctl->devs[i+1]->key = NULL;
			cryptctl->devs[i+1]->encrypt_or_decrypt = 1;
			cryptctl->devs[i+1]->minor_number = MINOR(device_region_dec);
			cdev_init(&cryptctl->devs[i+1]->ed_cdev, &ed_dev_file_operations);
			cryptctl->devs[i+1]->ed_cdev.owner = THIS_MODULE;
			err2 = cdev_add(&cryptctl->devs[i]->ed_cdev, device_region_dec, 1);
			
			if(err !=0 || err2 != 0){
				printk(KERN_ALERT "Failed to create encrypt/decrypt pair\n");
				retval = -2;
				return retval;
			}
			
			my_device_enc = device_create(ed_dev_class, NULL, device_region_enc, NULL, name_enc);
			my_device_dec = device_create(ed_dev_class, NULL, device_region_dec, NULL, name_dec);
			
			if(IS_ERR(my_device_enc) != 0 || IS_ERR(my_device_dec) != 0){
				retval = -2;
				printk(KERN_ALERT "device_create() for encrpyt/decrypt pair creation failed\n");
				cdev_del(&cryptctl->devs[i]->ed_cdev);
				cdev_del(&cryptctl->devs[i+1]->ed_cdev);
				unregister_chrdev_region(device_region_enc, 1);
				unregister_chrdev_region(device_region_dec, 1);
				return retval;
			}
	
			return retval;
		case CRYPT_DESTROY:
			//delete pair
			arg_num = (int *)kzalloc(sizeof(int), GFP_KERNEL);

			if(copy_from_user(arg_num, (int *)arg, sizeof(int)) != 0){
				retval = -2;
				return retval;
			}
			
			enc_index = *(arg_num)*2;
			dec_index = enc_index+1;
			
			if(cryptctl->devs[enc_index] == NULL || cryptctl->devs[dec_index] == NULL){
				retval = -1; //doesn't exist
				return retval;
			}
			
			device_destroy(ed_dev_class, MKDEV(dev_major_number, cryptctl->devs[enc_index]->minor_number));
			cdev_del(&cryptctl->devs[enc_index]->ed_cdev);
			unregister_chrdev_region(MKDEV(dev_major_number, cryptctl->devs[enc_index]->minor_number),1);
			kfree((void *)cryptctl->devs[enc_index]->message);
			kfree((void *)cryptctl->devs[enc_index]->key);
			cryptctl->devs[enc_index] = NULL;
			
			device_destroy(ed_dev_class, MKDEV(dev_major_number, cryptctl->devs[dec_index]->minor_number));
			cdev_del(&cryptctl->devs[dec_index]->ed_cdev);
			unregister_chrdev_region(MKDEV(dev_major_number, cryptctl->devs[dec_index]->minor_number),1);
			kfree((void *)cryptctl->devs[dec_index]->message);
			kfree((void *)cryptctl->devs[dec_index]->key);
			cryptctl->devs[dec_index] = NULL;
			
			return retval;
		case CRYPT_CONFIGURE:
			//change key
			data = (configure_input *)kzalloc(sizeof(configure_input), GFP_KERNEL);
			if(copy_from_user(data, (configure_input *)arg, sizeof(configure_input)) != 0){
				retval = -2;
				return retval;
			}
			
			enc_index = data->index * 2;
			dec_index = enc_index + 1;
			if(cryptctl->devs[enc_index] == NULL || cryptctl->devs[dec_index] == NULL){
				retval = -1; //doesn't existing
				return retval;
			}
			
			if(cryptctl->devs[enc_index]->key != NULL){ //reconfigure key
				kfree((void *)cryptctl->devs[enc_index]->key);
				kfree((void *)cryptctl->devs[enc_index]->key);
			}
			
			cryptctl->devs[enc_index]->key = (char *)kzalloc(strlen(data->key)+1, GFP_KERNEL);
			cryptctl->devs[dec_index]->key = (char *)kzalloc(strlen(data->key)+1, GFP_KERNEL);
			strcpy(cryptctl->devs[enc_index]->key, data->key);
			strcpy(cryptctl->devs[dec_index]->key, data->key);
			
			return retval;
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
		unregister_chrdev_region(device_region, 1);
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
				device_destroy(ed_dev_class, MKDEV(dev_major_number, cryptctl->devs[i]->minor_number));
				cdev_del(&cryptctl->devs[i]->ed_cdev);
				kfree((void *)cryptctl->devs[i]->message);
				kfree((void *)cryptctl->devs[i]->key);
				unregister_chrdev_region(MKDEV(dev_major_number, cryptctl->devs[i]->minor_number),1);
				cryptctl->devs[i] = NULL;
			}
		}
		//deleted all of the existing encrpyt/decrypt devices
		device_destroy(cryptctl_class, MKDEV(dev_major_number, cryptctl->minor_number));
		cdev_del(&cryptctl->cryptctl_cdev);
		kfree((void *)cryptctl->buffer);
		class_destroy(cryptctl_class);
		class_destroy(ed_dev_class);
		unregister_chrdev_region(MKDEV(dev_major_number,cryptctl->minor_number),1);
		cryptctl_class = NULL;
		cryptctl = NULL;
	}
	
	return;
}

module_init(cryptctl_init);
module_exit(cryptctl_exit);
