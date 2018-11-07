#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <err.h>
#include <errno.h>

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
	char *dev_path = NULL;
	char dev[15];
	char type; //C - Create D - Destroy O - cOnfigure R - Read W - Write
	ssize_t count = -1;
	int dont_print = 1;
	configure_input config = {0, ""};

	fd = open("/dev/cryptctl", O_RDWR);
	if (fd < 0) {
		printf("\nCannot open device file");
		return 1;
	}
	
	
	while (loop) {
		if(dont_print){
			printf("\nEnter an input (C - Create; D - Destroy; O - Configure/Change Key; R - Read; W - Write; B - Stop) for kernel action: ");
			
		}		
		dont_print = 1;
		scanf("%1c", &type);
		fseek(stdin,0,SEEK_END);
		switch (type) {
			case 'C':
				ret_val = ioctl(fd, CRYPT_CREATE, NULL);
				
				if(ret_val == -1){
					printf("\nToo many pairs, please destroy (D) a pair");
				}else if(ret_val < -1){
					printf("\nError creating device pair");
				}else{
					printf("\nEncryption/Decryption Pair ID: %ld\n", ret_val);
				}

				break;
			case 'D':
				printf("\nEnter the Encryption/Decryption Pair ID you want destroyed (0-9): ");
				scanf("%d", &index);
				fseek(stdin,0,SEEK_END);
				if (index < 0 || index > 10) {
					printf("\nID is out of bounds\n");
				}
				else {
					ret_val = ioctl(fd, CRYPT_DESTROY, &index);
					if (ret_val == -1) {
						printf("\nNo pair exists with the given ID\n");
					}else if(ret_val < -1){
						printf("\nError in copy_from_user()\n");
					}
				}
			
				break;
			case 'O':
				printf("\nEnter the ID of the pair you want configured (0-9): ");
				scanf("%d", &index);
				fseek(stdin,0,SEEK_END);
				if (index < 0 || index > 10) {
					printf("\nIndex is out of bounds\n");
				}
				else {
					printf("\nEnter the key to configure (32 char max, will be trimmed if longer): ");
					//fgets(key, 33, stdin);
					scanf("%32[^\n]s", key);
					fseek(stdin,0,SEEK_END);
					strcpy(config.key, key);
					config.index = index;
					ret_val = ioctl(fd, CRYPT_CONFIGURE, &config);
					if (ret_val == -1) {
						printf("\nID doesn't exist\n");
					}else if(ret_val < -1){
						printf("\nError with copy_from_user()\n");
					}
				}
				//}
				break;
			case 'R':
				
				printf("\nEnter the name of the device you want to read from (cryptEncryptXX or cryptDecryptXX): ");
				//fgets(dev, 15, stdin);
				if(dev_path != NULL){
					free(dev_path);
				}
				dev_path = (char*)malloc(20);
				strcpy(dev_path, "/dev/");
				scanf("%14s", dev);
				fseek(stdin,0,SEEK_END);
				strcat(dev_path, dev);
				//printf("DEVPATH: %s\n", dev_path);
				read_write_fd = open(dev_path,O_RDWR);
				if(read_write_fd < 0){
					printf("\nInvalid device name\n");
				}else{
					if(buffer != NULL){
						free((void *)buffer);
					}
					buffer = (char *)malloc(257);
					while((ret_val = read(read_write_fd, buffer, 256)) != 0){
						if(ret_val == -1){
							printf("\nDevice needs a Key or Message before Reading\n");
							break;
						}else if(ret_val < -1){
							printf("\nError in read\n");				
						}
						count = count + ret_val;
					}
					
					if(ret_val == 0){
						buffer[count] = '\0';
						printf("\nMessage: %s", buffer);
					}
					close(read_write_fd);
				}
				
							
				break;
			case 'W':
				printf("\nEnter the name of the device you want to write to (cryptEncryptXX or cryptDecryptXX): ");
				//fgets(dev, 15, stdin);
				if(dev_path != NULL){
					free(dev_path);
				}
				dev_path = (char*)malloc(20);
				strcpy(dev_path, "/dev/");
				scanf("%14s", dev);
				fseek(stdin,0,SEEK_END);
				strcat(dev_path, dev);
				//printf("DEVPATH: %s\n", dev_path);
				
				read_write_fd = open(dev_path,O_RDWR);
				if(read_write_fd < 0){
					printf("\nInvalid device name\n");
				}else{
					if(buffer != NULL){
						free((void *)buffer);
					}
					buffer = (char *)malloc(257);
					printf("\nEnter a message to encrypt/decrypt (max length is 256): ");
					//fgets(buffer, 257, stdin);
					scanf("%256[^\n]s", buffer);
					fseek(stdin,0,SEEK_END);
					while((ret_val = write(read_write_fd, buffer, strlen(buffer)+1)) != strlen(buffer)+1){
						//printf("RET VAL: %ld\n", ret_val);
						if(ret_val == -1){
							printf("\nDevice needs to be configured with a key first\n");
							break;
						}else if(ret_val < -1){
							printf("\nError in write\n");	
							break;
						}
					}
					
					close(read_write_fd);
				}

				break;
			case 'B':
				loop = 0;
				break;
			case '\n': 
				dont_print = 0;
				break;
			
			default:
				printf("\nInvalid input\n");
		}

	}
	close(fd);
	return 0;
}
