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

typedef struct ioctl_input {
	int index;
	char key[33];
	char buffer[257];
} ioctl_input;

#define CRYPT_CREATE _IO(CRYPT_IOC_MAGIC, 0)
#define CRYPT_DESTROY _IOR(CRYPT_IOC_MAGIC, 1, ioctl_input)
#define CRYPT_CONFIGURE _IOR(CRYPT_IOC_MAGIC, 2, ioctl_input)
#define CRYPT_WRITE _IOWR(CRYPT_IOC_MAGIC, 3, ioctl_input)
#define CRYPT_READ _IOWR(CRYPT_IOC_MAGIC, 4, ioctl_input)

int main() {
	int fd;
	int loop = 1;
	int num_pairs = 0;
	int index;
	char key[33];
	char buffer[257];
	char type; //C - Create D - Destroy O - cOnfigure R - Read W - Write

	ioctl_input message = {0, "", ""};

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
					if (index + 1 > num_pairs) {
						printf("Index doesn't exist, please try again with a lower number\n");
					}
					else if (index < 0) {
						printf("Negative index, please try again with a positive number\n");
					}
					else {
						num_pairs -= 1;
						message.index = index;
						ioctl(fd, CRYPT_DESTROY, (ioctl_input*) &message);
					}
				}
				break;
			case 'O':
				if (num_pairs <= 0) {
					printf("No pairs to configure, please create (C) a pair\n");
				} 
				else {
					printf("Enter the index you want configured (0-9):\n");
					scanf("%d", &index);
					if (index + 1 > num_pairs) {
						printf("Index doesn't exist, please try again with a lower number\n");
					}
					else if (index < 0) {
						printf("Negative index, please try again with a positive number\n");
					}
					else {
						printf("Enter the key to configure (32 char max):\n");
						fgets(key, 33, stdin);
						message.key = key;
						message.index = index;
						ioctl(fd, CRYPT_CONFIGURE, (ioctl_input*) &message);
					}
				}
				break;
			case 'R':
				if (num_pairs <= 0) {
					printf("No pairs to read, please create (C) a pair\n");
				} 
				else {
					printf("Enter the index you want to read (0-9):\n");
					scanf("%d", &index);
					if (index + 1 > num_pairs) {
						printf("Index doesn't exist, please try again with a lower number\n");
					}
					else if (index < 0) {
						printf("Negative index, please try again with a positive number\n");
					}
					else {
						printf("Enter the message to read (256 char max):\n");
						fgets(buffer, 257, stdin);
						message.buffer = buffer;
						message.index = index;
						ioctl(fd, CRYPT_READ, (ioctl_input*) &message);
						buffer = ioctl(fd, CRYPT_READ);
						printf("%s\n", buffer);
					}
				}
				break;
			case 'W':
				if (num_pairs <= 0) {
					printf("No pairs to write, please create (C) a pair\n");
				} 
				else {
					printf("Enter the index you want to write (0-9):\n");
					scanf("%d", &index);
					if (index + 1 > num_pairs) {
						printf("Index doesn't exist, please try again with a lower number\n");
					}
					else if (index < 0) {
						printf("Negative index, please try again with a positive number\n");
					}
					else {
						printf("Enter the message to read (256 char max):\n");
						fgets(buffer, 257, stdin);
						message.buffer = buffer;
						message.index = index;
						ioctl(fd, CRYPT_WRITE, (ioctl_input*) &message);
						buffer = ioctl(fd, CRYPT_WRITE);
						printf("%s\n", buffer);
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

	return 0;
}