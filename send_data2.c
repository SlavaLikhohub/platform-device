#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>



#define MEM_BASE1 0x9f200000
#define REG_BASE1  0x9f201000

#define MEM_BASE2 0x9f210000
#define REG_BASE2 0x9f211000

#define MEM_SIZE	0x1000
#define REG_SIZE	8

#define PLAT_IO_FLAG_REG		(0) /*Offset of flag register*/
#define PLAT_IO_SIZE_REG		(4) /*Offset of flag register*/
#define PLAT_IO_DATA_READY	(1) /*IO data ready flag */

#define MAX_DEVICES	2

extern int errno;

struct my_device {
	uint32_t mem_base;
	uint32_t mem_size;
	uint32_t reg_base;
	uint32_t reg_size;	
};

static struct my_device my_devices[MAX_DEVICES] = {{
	.mem_base = MEM_BASE1,
	.mem_size = MEM_SIZE,
	.reg_base = REG_BASE1,
	.reg_size = REG_SIZE,
	},
	{
	.mem_base = MEM_BASE2,
	.mem_size = MEM_SIZE,
	.reg_base = REG_BASE2,
	.reg_size = REG_SIZE,
	},
};
int usage(char **argv)
{
	printf("Program sends file to the specific device\n");
	printf("Usage: %s <device> <file>\n", argv[0]);
	return -1;
}

int main(int argc, char **argv)
{
	volatile unsigned int *reg_addr = NULL, *count_addr, *flag_addr;
	volatile unsigned char *mem_addr = NULL;
	unsigned int i, device, ret, len, count;
	struct stat st;
	uint8_t *buf;
	FILE *f;

	if (argc != 3) {
		return usage(argv);
	}

	device = atoi(argv[1]);
	if (device >= MAX_DEVICES)
		return usage(argv);

	ret = stat(argv[2], &st);
	if (ret) {
		printf("ERROR: Can't find (%s)\n", argv[2]);
		return -1;
	}
	buf = (uint8_t *) malloc(st.st_size);
	if (!buf) {
		printf("ERROR: Out of memory");
		return -1;
	}

	f = fopen(argv[2], "rb");
	if (!f) {
		printf("fopen error (%s)\n", argv[2]);
		return -1;
	}
	len = fread(buf, 1U, st.st_size, f);
	fclose(f);
	if (len != st.st_size) {
		printf("File read Error (%s)\n", argv[2]);
		return -1;
	}

	int fd = open("/dev/mem",O_RDWR|O_SYNC);
	if(fd < 0)
	{
		printf("Can't open /dev/mem\n");
		return -1;
	}
	mem_addr = (unsigned char *) mmap(0, my_devices[device].mem_size,
				PROT_WRITE, MAP_SHARED, fd, my_devices[device].mem_base);
	if(mem_addr == NULL)
	{
		printf("Can't mmap\n");
		return -1;
	}

	reg_addr = (unsigned int *) mmap(0, my_devices[device].reg_size,
			PROT_WRITE | PROT_READ, MAP_SHARED, fd, my_devices[device].reg_base);

	flag_addr = reg_addr;
	count_addr = reg_addr;
	count_addr++;

	*flag_addr = 0;
	while (len) {
		while (*flag_addr & PLAT_IO_DATA_READY) {
			usleep(50000);
		}
		count = len > my_devices[device].mem_size ? 
				my_devices[device].mem_size : len;
		memcpy((void *)mem_addr, buf, count);
		len -= count;
		buf += count;
		*count_addr = count;
		*flag_addr = PLAT_IO_DATA_READY;
	}

	return 0;
}
