#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MEM_BASE 0x9f200000
#define REG_BASE  0x9f201000
#define MEM_SIZE	(1024)
#define REG_SIZE	(8)

#define PLAT_IO_FLAG_REG		(0) /*Offset of flag register*/
#define PLAT_IO_SIZE_REG		(4) /*Offset of flag register*/
#define PLAT_IO_DATA_READY	(1) /*IO data ready flag */


extern int errno;

int main(int argc, char **argv)
{
	volatile unsigned int *reg_addr = NULL, *count_addr, *flag_addr;
	volatile unsigned char *mem_addr = NULL;
	unsigned int i, num = 0, val;

	int fd = open("/dev/mem",O_RDWR|O_SYNC);
	if(fd < 0)
	{
		printf("Can't open /dev/mem\n");
		return -1;
	}
	mem_addr = (unsigned char *) mmap(0, MEM_SIZE, PROT_WRITE, MAP_SHARED, fd, MEM_BASE);
	if(mem_addr == NULL)
	{
		printf("Can't mmap\n");
		return -1;
	}

	reg_addr = (unsigned int *) mmap(0, REG_SIZE, PROT_WRITE |PROT_READ, MAP_SHARED, fd, REG_BASE);

	flag_addr = reg_addr;
	count_addr = reg_addr;
	count_addr++;
	for (i=0; i< 50; i++) {
		*mem_addr++ = 0x41 + i;
	}
	*count_addr = 50;
	*flag_addr = PLAT_IO_DATA_READY;

	return 0;
}
