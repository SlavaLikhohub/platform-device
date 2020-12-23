#include <linux/hrtimer.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <asm/io.h>
#include <linux/timer.h>

MODULE_AUTHOR("Viacheslava Lykhohub <viacheslav.lykhohub@globallogic.com>");
MODULE_DESCRIPTION("Dummy platform driver");
MODULE_LICENSE("GPL");

#define MEM_SIZE 4096

#define DRV_NAME  "plat_dummy"
/*Device has 2 resources:
* 1) 4K of memory at address defined by dts - used for data transfer;
* 2) Three 32-bit registers at address (defined by dts)
*  2.1. Flag Register: @offset 0
*	bit 0: PLAT_IO_DATA_READY - set to 1 if data from device ready
*	other bits: reserved;
* 2.2. Data size Register @offset 4: - Contain data size from device (0..4095);
* 2.3. Lock of time_reg @offset 8 - set to 1 if time_reg is locked
* 3) 32-bit register to save jiffies value
*/
#define DEVICE_POOLING_PERIOD_MS    10000 /* ms */
#define TIME_WRITE_PERIOD_MS          500 /* ms */
#define PLAT_IO_FLAG_REG		0 /* Offset of flag register */
#define PLAT_IO_SIZE_REG		4 /* Offset of size register */
#define PLAT_IO_TIME_REG_LOCK		8 /* Offset of time_reg_lock register */
#define PLAT_IO_DATA_READY_MASK		1 /* IO data ready flag */
#define PLAT_IO_TIME_REG_LOCK_MASK	1 /* IO data ready flag */
#define MAX_DUMMY_PLAT_THREADS  	1 /* Maximum amount of threads */

#define N_FIELDS 3 // Number of resources in :c:type:`plat_dummy_device`.

struct plat_dummy_device {
	void __iomem *buff;
	void __iomem *regs;
	void __iomem *time_reg;
	struct delayed_work     dwork;
	struct workqueue_struct *data_read_wq;
	u64 js_pool_time;
	spinlock_t regs_lock;
	struct timer_list timer;
	u64 timer_write_period;
};

static inline u32 plat_dummy_buff_read8(struct plat_dummy_device *dev,
					u32 offset)
{
	return ioread8(dev->buff + offset);
}

static inline u32 plat_dummy_reg_read32(struct plat_dummy_device *dev,
					u32 offset)
{
	u32 res;
	spin_lock(&dev->regs_lock);
	res = ioread32(dev->regs + offset);
	spin_unlock(&dev->regs_lock);

	return res;
}
static inline void plat_dummy_reg_write32(struct plat_dummy_device *dev,
					  u32 offset, u32 val)
{
	spin_lock(&dev->regs_lock);
	iowrite32(val, dev->regs + offset);
	spin_unlock(&dev->regs_lock);
}

static inline void plat_dummy_time_reg_lock(struct plat_dummy_device *dev)
{
	while (plat_dummy_reg_read32(dev, PLAT_IO_TIME_REG_LOCK))
		;
	mb();
	plat_dummy_reg_write32(dev, PLAT_IO_TIME_REG_LOCK, 1);
}

static inline void plat_dummy_time_reg_unlock(struct plat_dummy_device *dev)
{
	plat_dummy_reg_write32(dev, PLAT_IO_TIME_REG_LOCK, 0);
	mb();
}

static inline void plat_dummy_write_current_time(struct plat_dummy_device *dev)
{
	plat_dummy_time_reg_lock(dev);
	iowrite32(jiffies, dev->time_reg);
	plat_dummy_time_reg_unlock(dev);
}

static inline u32 plat_dummy_read_current_time(struct plat_dummy_device *dev)
{
	u32 res;
	plat_dummy_time_reg_lock(dev);
	res = ioread32(dev->time_reg);
	plat_dummy_time_reg_unlock(dev);

	return res;
}
static void plat_dummy_work(struct work_struct *work)
{
	struct plat_dummy_device *device;
	u32 i, size, status;
	u8 data;
	u32 reg_time_data;

	pr_debug("++%s(%u)\n", __func__, jiffies_to_msecs(jiffies));

	device = container_of(work, struct plat_dummy_device, dwork.work);

	reg_time_data = plat_dummy_read_current_time(device);
	pr_info("reg_time_data: %u\n", reg_time_data);

	status = plat_dummy_reg_read32(device, PLAT_IO_FLAG_REG);

	if (status & PLAT_IO_DATA_READY_MASK) {
		size = plat_dummy_reg_read32(device, PLAT_IO_SIZE_REG);
		pr_info("%s: size = %d\n", __func__, size);

		if (size > MEM_SIZE)
			size = MEM_SIZE;

		for(i = 0; i < size; i++) {
			data = plat_dummy_buff_read8(device, i);
			pr_info("%s:buff[%d] = 0x%x ('%c')\n", __func__,  i,
				data, data);
		}
		rmb();
		status &= ~PLAT_IO_DATA_READY_MASK;
		plat_dummy_reg_write32(device, PLAT_IO_FLAG_REG, status);

	}
	queue_delayed_work(device->data_read_wq, &device->dwork,
			   device->js_pool_time);
}

void timer_function(struct timer_list *self)
{
	struct plat_dummy_device *device = container_of(self,
							struct plat_dummy_device,
							timer);

	pr_debug("++%s\n", __func__);

	plat_dummy_write_current_time(device);

	self->expires = jiffies + device->timer_write_period;
	add_timer(self);
}

static const struct of_device_id plat_dummy_of_match[] = {
	{
		.compatible = "ti,plat_dummy",
	}, {
	},
 };

static int plat_dummy_probe(struct platform_device *pdev)
{
	int i;
	struct device *dev = &pdev->dev;
	struct plat_dummy_device *device = NULL;

	struct resource *res;
	struct device_node *np = pdev->dev.of_node;
	void **registers[N_FIELDS];

	pr_debug("++%s\n", __func__);

	if (!np) {
		pr_err("No device node found!\n");
		return -ENOMEM;
	}

	device = devm_kzalloc(dev, sizeof(struct plat_dummy_device),
			         GFP_KERNEL);
	pr_info("Device name: %s\n", np->name);
	if (!device)
		return -ENOMEM;

	/* Init spinlock */
	spin_lock_init(&device->regs_lock);

	/* Map the memmory */
	registers[0] = &device->buff;
	registers[1] = &device->regs;
	registers[2] = &device->time_reg;

	for (i = 0; i < ARRAY_SIZE(registers); i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		*registers[i] = devm_ioremap_resource(&pdev->dev, res);
		pr_info("res %d = %zx..%zx\n", i, res->start, res->end);
		pr_info("register %zx\n", (size_t)*(registers[i]));
		if (IS_ERR(*registers[i]))
			return PTR_ERR(*registers[i]);
	}

	platform_set_drvdata(pdev, device);

	plat_dummy_time_reg_unlock(device);

	pr_info("Buffer mapped to %p\n", device->buff);
	pr_info("Registers mapped to %p\n", device->regs);
	pr_info("Time register mapped to %p\n", device->time_reg);

	/* Init data read WQ */
	device->data_read_wq = alloc_workqueue("plat_dummy_read",
					WQ_UNBOUND, MAX_DUMMY_PLAT_THREADS);

	if (!device->data_read_wq)
		return -ENOMEM;

	INIT_DELAYED_WORK(&device->dwork, plat_dummy_work);
	device->js_pool_time = msecs_to_jiffies(DEVICE_POOLING_PERIOD_MS);
	queue_delayed_work(device->data_read_wq, &device->dwork, 0);

	/* Init timer */
	timer_setup(&device->timer, timer_function, 0);

	device->timer_write_period = msecs_to_jiffies(TIME_WRITE_PERIOD_MS);
	device->timer.expires = jiffies + device->timer_write_period;

	add_timer(&device->timer);

	return PTR_ERR_OR_ZERO(device->buff);
}

static int plat_dummy_remove(struct platform_device *pdev)
{
	struct plat_dummy_device *device = platform_get_drvdata(pdev);

	pr_debug("++%s\n", __func__);

	if (device->data_read_wq) {
	/* Destroy work Queue */
		cancel_delayed_work_sync(&device->dwork);
		destroy_workqueue(device->data_read_wq);
	}

	del_timer_sync(&device->timer);

	plat_dummy_time_reg_unlock(device);

        return 0;
}

static struct platform_driver plat_dummy_driver = {
	.driver = {
		.name = DRV_NAME,
		.of_match_table = plat_dummy_of_match,
	},
	.probe		= plat_dummy_probe,
	.remove		= plat_dummy_remove,
};

MODULE_DEVICE_TABLE(of, plat_dummy_of_match);

module_platform_driver(plat_dummy_driver);
