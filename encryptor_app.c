#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

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

void cleanup(int fd){
	for (int i = 0; i <= 9; i++){
		ioctl(fd, CRYPT_DESTROY, &i);
	}
}

int main() {
	int fd;
	int read_write_fd;
	int loop = 1;
	int num_pairs = 0;
	int index;
	int encrypt_decrypt;
	long ret_val;
	char key[33];
	char *buffer = NULL;
	char dev_path[20] = "/dev/";
	char dev[15];
	char type; //C - Create D - Destroy O - cOnfigure R - Read W - Write
	ssize_t count = -1;
	configure_input config = {0, ""};

	fd = open("/dev/cryptctl", O_RDWR);
	if (fd < 0) {
		printf("Cannot open device file\n");
		return 1;
	}
	
	
	while (loop) {
		printf("Enter an input (C - Create; D - Destroy; O - cOnfigure; R - Read; W - Write; B - Stop) for kernel action: ");
		scanf("%c", &type);
		switch (type) {
			case 'C':
				ret_val = ioctl(fd, CRYPT_CREATE, NULL);
				
				if(ret_val == -1){
					printf("Too many pairs, please destroy (D) a pair\n");
				}else if(ret_val < -1){
					printf("Error creating device pair\n");
				}else{
					printf("Encryption/Decryption Pair ID: %ld\n", ret_val);
				}

				break;
			case 'D':
				printf("Enter the Encryption/Decryption Pair ID you want destroyed (0-9): ");
				scanf("%d", &index);
				if (index < 0 || index > 10) {
					printf("ID is out of bounds\n");
				}
				else {
					ret_val = ioctl(fd, CRYPT_DESTROY, &index);
					if (ret_val == -1) {
						printf("No pair exists with the given ID\n");
					}else if(ret_val < -1){
						printf("Error in copy_from_user()\n");
					}
				}
			
				break;
			case 'O':
				printf("Enter the ID of the pair you want configured (0-9): ");
				scanf("%d", &index);
				if (index < 0 || index > 10) {
					printf("Index is out of bounds\n");
				}
				else {
					printf("Enter the key to configure (32 char max, will be trimmed if longer): ");
					fgets(key, 33, stdin);
					strcpy(config.key, key);
					config.index = index;
					ret_val = ioctl(fd, CRYPT_CONFIGURE, &config);
					if (ret_val == -1) {
						printf("ID doesn't exist\n");
					}else if(ret_val < -1){
						printf("Error with copy_from_user()\n");
					}
				}
				//}
				break;
			case 'R':
				
				printf("Enter the name of the device you want to read from (cryptEncryptXX or cryptDecryptXX): ");
				fgets(dev, 15, stdin);
				strcat(dev_path, dev);
				read_write_fd = open(dev_path,O_RDWR);
				if(read_write_fd < 0){
					printf("Invalid device name\n");
				}else{
					if(buffer != NULL){
						free((void *)buffer);
					}
					buffer = (char *)malloc(257);
					while((ret_val = read(read_write_fd, buffer, 256)) != 0){
						if(ret_val == -1){
							printf("Device needs to be configured with a key first\n");
							break;
						}
						count = count + ret_val;
					}
					
					if(ret_val == 0){
						buffer[count] = '\0';
						printf("Message: %s\n", buffer);
					}
					close(read_write_fd);
				}
				
							
				break;
			case 'W':
				printf("Enter the name of the device you want to write to (cryptEncryptXX or cryptDecryptXX): ");
				fgets(dev, 15, stdin);
				strcat(dev_path, dev);
				read_write_fd = open(dev_path,O_RDWR);
				if(read_write_fd < 0){
					printf("Invalid device name\n");
				}else{
					if(buffer != NULL){
						free((void *)buffer);
					}
					buffer = (char *)malloc(257);
					printf("Enter a message to encrypt/decrypt (max length is 256): ");
					fgets(buffer, 257, stdin);
					count = strlen(buffer);
					while((ret_val = write(read_write_fd, buffer, 256)) != 0){
						if(ret_val == -1){
							printf("Device needs to be configured with a key first\n");
							break;
						}
						count = count - ret_val;
					}
					
					close(read_write_fd);
				}

				break;
			case 'B':
				loop = 0;
				break;
			case '\n': break;
			
			default:
				printf("Invalid input\n");
		}

	}
	close(fd);
	return 0;
}
