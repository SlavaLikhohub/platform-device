#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#define MEM_BASE	0x9f200000
#define REG_BASE	0x9f201000
#define TIME_REG_BASE 	0x9f20100c
#define MEM_SIZE	4096
#define REG_SIZE	12
#define TIME_REG_SIZE   4

#define PLAT_IO_FLAG_REG		0 /* Offset of flag register */
#define PLAT_IO_SIZE_REG		4 /* Offset of size register */
#define PLAT_IO_TIME_REG_LOCK		8 /* Offset of time_reg_lock register */

int main(int argc, char **argv)
{
	volatile uint32_t *reg_addr = NULL;
	volatile uint32_t *time_reg_lock_addr;
	volatile uint32_t *time_reg_addr = NULL;
	uint32_t jiffies_data = 0;

	int fd = open("/dev/mem", O_RDWR|O_SYNC);
	if (fd < 0)
	{
		printf("Can't open /dev/mem\n");
		return -1;
	}

	reg_addr = (uint32_t *) mmap(0, REG_SIZE, PROT_READ | PROT_WRITE,
				     MAP_SHARED, fd, REG_BASE);

	if (!reg_addr) {
		printf("Can't mmap reg_addr\n");
		return -1;
	}

	time_reg_addr = (uint32_t *) mmap(0, TIME_REG_SIZE, PROT_READ,
					  MAP_SHARED, fd, TIME_REG_BASE);
	if (!time_reg_addr)
	{
		printf("Can't mmap time_reg_addr\n");
		return -1;
	}

	time_reg_lock_addr = reg_addr + PLAT_IO_TIME_REG_LOCK;

	jiffies_data = *time_reg_addr;

	printf("jiffies_data: %u\n", jiffies_data);

	return 0;
}
