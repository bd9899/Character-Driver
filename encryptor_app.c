#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stath.h>
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
#define CRYPT_DESTROY _IOR(CRYPT_IOC_MAGIC, 1, int)
#define CRYPT_CONFIGURE _IOR(CRYPT_IOC_MAGIC, 2, configure_input)

int main() {
	int fd;
	int encrypt_fd;
	int decrypt_fd;
	int loop = 1;
	int num_pairs = 0;
	int index;
	int retval;
	char key[33];
	char buffer[257];
	char type; //C - Create D - Destroy O - cOnfigure R - Read W - Write

	configure_input config = {0, ""};

	fd = open("/dev/cryptctl", O_RDWR);
	if (fd < 0) {
		printf("Cannot open device file\n");
		return 1;
	}

	while (loop) {
		printf("Enter an input (C - Create; D - Destroy; O - cOnfigure; R - Read; W - Write; B - Stop) for kernel action:\n");
		scanf("%c", &type);
		switch (type) {
			case 'C':
				if (num_pairs >= 10) {
					printf("Too many pairs, please destroy (D) a pair\n");
				}
				else {
					num_pairs += 1;
					ioctl(fd, CRYPT_CREATE);
				}
				break;
			case 'D':
				if (num_pairs <= 0) {
					printf("No pairs to destroy, please create (C) a pair\n");
				} 
				else {
					printf("Enter the index you want destroyed (0-9):\n");
					scanf("%d", &index);
					if (index < 0 || index > 10) {
						printf("Index is out of bounds\n");
					}
					else {
						retval = ioctl(fd, CRYPT_DESTROY, &index);
						if (retval < 0) {
							printf("Index doesn't exist\n");
						}
						else {
							num_pairs -= 1;
						}
					}
				}
				break;
			case 'O':
				if (num_pairs <= 0) {
					printf("No pairs to destroy, please create (C) a pair\n");
				} 
				else {
					printf("Enter the index you want destroyed (0-9):\n");
					scanf("%d", &index);
					if (index < 0 || index > 10) {
						printf("Index is out of bounds\n");
					}
					else {
						printf("Enter the key to configure (32 char max):\n");
						fgets(key, 33, stdin);
						strcpy(config.key, key);
						config.index = index;
						retval = ioctl(fd, CRYPT_DESTROY, &config);
						if (retval < 0) {
							printf("Index doesn't exist\n");
						}
					}
				}
				break;
			case 'R':
				if (num_pairs <= 0) {
					printf("No pairs to read from, please create (C) a pair\n");
				} 
				else {
					printf("Enter the index you want to read from (0-9):\n");
					scanf("%d", &index);
					printf("Enter the message (256 char max):\n");
					fgets(buffer, 257, stdin);
					switch(index) {
						case 0:
						
							break;
						case 1:
							
							break;
						case 2:
							
							break;
						case 3:
							
							break;
						case 4:
							
							break;
						case 5:
							
							break;
						case 6:
							
							break;
						case 7:
							
							break;
						case 8:
							
							break;
						case 9:
							
							break;
						default:
							printf("Index out of bounds, please enter an index from 0-9")
					}
				}
				break;
			case 'W':
				if (num_pairs <= 0) {
					printf("No pairs to write to, please create (C) a pair\n");
				} 
				else {
					printf("Enter the index you want to write to (0-9):\n");
					scanf("%d", &index);
					printf("Enter the message(256 char max):\n");
					fgets(buffer, 257, stdin);
					switch(index) {
						case 0:
							break;
						case 1:
							
							break;
						case 2:
							
							break;
						case 3:
							
							break;
						case 4:
							
							break;
						case 5:
							
							break;
						case 6:
							
							break;
						case 7:
							
							break;
						case 8:
							
							break;
						case 9:
							
							break;
						default:
							printf("Index out of bounds, please enter an index from 0-9")
					}
				}
				break;
			case 'B':
				loop = 0;
				break;
			default:
				printf("Invalid input\n");
		}

	}
	close(fd);
	return 0;
}
