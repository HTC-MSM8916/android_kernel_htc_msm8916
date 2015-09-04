#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/wakelock.h>
#include <linux/of_gpio.h>
#include <linux/pn547.h>
#include <mach/board_htc.h>
#include <linux/clk-private.h>
#include <linux/clk.h>

int is_debug = 0;
int s_wdcmd_cnt = 0;
int is_alive = 1;
int is_uicc_swp = 1;

#define DBUF(buff,count) \
	if (is_debug) \
		for (i = 0; i < count; i++) \
			printk(KERN_DEBUG "[NFC] %s : [%d] = 0x%x\n", \
				__func__, i, buff[i]);

#define D(x...)	\
	if (is_debug)	\
		printk(KERN_DEBUG "[NFC] " x)
#define I(x...) printk(KERN_INFO "[NFC] " x)
#define E(x...) printk(KERN_ERR "[NFC] [Err] " x)

#define MAX_BUFFER_SIZE	512
#define PN547_RESET_CMD 	0
#define PN547_DOWNLOAD_CMD	1

#define I2C_RETRY_COUNT 10

struct clk {
	unsigned long		rate;
	const struct clk_ops	*ops;
	const struct icst_params *params;
	void __iomem		*vcoreg;
};

struct pn547_dev	{
	struct class		*pn547_class;
	struct device		*pn_dev;
	struct device		*comn_dev;
	wait_queue_head_t	read_wq;
	struct mutex		read_mutex;
	struct wake_lock io_wake_lock;
	struct i2c_client	*client;
	struct miscdevice	pn547_device;
	unsigned int		irq_gpio;
	bool			irq_enabled;
	spinlock_t		irq_enabled_lock;
	unsigned int 		ven_gpio;
	unsigned int		ven_value;
	unsigned int 		firm_gpio;
	unsigned int 		pwr_en;
	void (*gpio_init) (void);
	unsigned int 		ven_enable;
	int boot_mode;
	bool                     isReadBlock;
	struct	clk		*s_clk;
	const	char		*clk_src_name;
	
	unsigned int		clkreq_gpio;

};

struct pn547_dev *pn_info;
int ignoreI2C = 0;

static int pn547_RxData(uint8_t *rxData, int length)
{
	uint8_t loop_i;
	struct pn547_dev *pni = pn_info;

	struct i2c_msg msg[] = {
		{
		 .addr = pni->client->addr,
		 .flags = I2C_M_RD,
		 .len = length,
		 .buf = rxData,
		 },
	};

	D("%s: [addr=%x flag=%x len=%x]\n", __func__,
		msg[0].addr, msg[0].flags, msg[0].len);

	if (ignoreI2C) {
		I("ignore pn547_RxData %d\n", ignoreI2C);
		return 0;
	}

	rxData[0] = 0;

	for (loop_i = 0; loop_i < I2C_RETRY_COUNT; loop_i++) {
		D("%s: retry %d ........\n", __func__, loop_i);
		if (i2c_transfer(pni->client->adapter, msg, 1) > 0)
			break;

		mdelay(10);
	}

	if (loop_i >= I2C_RETRY_COUNT) {
		E("%s Error: retry over %d\n", __func__,
			I2C_RETRY_COUNT);
		return -EIO;
	}

	return 0;
}

static int pn547_TxData(uint8_t *txData, int length)
{
	uint8_t loop_i;
	struct pn547_dev *pni = pn_info;
	struct i2c_msg msg[] = {
		{
		 .addr = pni->client->addr,
		 .flags = 0,
		 .len = length,
		 .buf = txData,
		 },
	};

	D("%s: [addr=%x flag=%x len=%x]\n", __func__,
		msg[0].addr, msg[0].flags, msg[0].len);

	if (ignoreI2C) {
		I("ignore pn547_TxData %d\n", ignoreI2C);
		return 0;
	}

	for (loop_i = 0; loop_i < I2C_RETRY_COUNT; loop_i++) {
		D("%s: retry %d ........\n", __func__, loop_i);
		if (i2c_transfer(pni->client->adapter, msg, 1) > 0)
			break;

		msleep(10);
	}

	if (loop_i >= I2C_RETRY_COUNT) {
		E("%s:  Error: retry over %d\n",
			__func__, I2C_RETRY_COUNT);
		return -EIO;
	}

	return 0;
}

static void pn547_disable_irq(struct pn547_dev *pn547_dev)
{
	unsigned long flags;

	spin_lock_irqsave(&pn547_dev->irq_enabled_lock, flags);
	if (pn547_dev->irq_enabled) {
		disable_irq_nosync(pn547_dev->client->irq);
		pn547_dev->irq_enabled = false;
	}
	spin_unlock_irqrestore(&pn547_dev->irq_enabled_lock, flags);
}

static irqreturn_t pn547_dev_irq_handler(int irq, void *dev_id)
{
	struct pn547_dev *pn547_dev = dev_id;
	static unsigned long orig_jiffies = 0;

	if (gpio_get_value(pn547_dev->irq_gpio) == 0) {
		I("%s: irq_workaround\n", __func__);
		return IRQ_HANDLED;
	}
	pn547_disable_irq(pn547_dev);

	
	wake_up(&pn547_dev->read_wq);

	if (time_after(jiffies, orig_jiffies + msecs_to_jiffies(1000)))
		I("%s: irq=%d\n", __func__, irq);
	orig_jiffies = jiffies;

	return IRQ_HANDLED;
}

static void pn547_Enable(void)
{
	struct pn547_dev *pni = pn_info;
	unsigned int set_value = pni->ven_enable;
	I("%s: gpio=%d set_value=%d\n", __func__, pni->ven_gpio, set_value);

	gpio_set_value(pni->ven_gpio, set_value);
	pni->ven_value = 1;
}

static void pn547_Disable(void)
{
	struct pn547_dev *pni = pn_info;
	unsigned int set_value = !pni->ven_enable;
	I("%s: gpio=%d set_value=%d\n", __func__, pni->ven_gpio, set_value);

	gpio_set_value(pni->ven_gpio, set_value);
	pni->ven_value = 0;
}


static int pn547_isEn(void)
{
	struct pn547_dev *pni = pn_info;
	
	return pni->ven_value;
}
uint8_t read_buffer[MAX_BUFFER_SIZE];

static ssize_t pn547_dev_read(struct file *filp, char __user *buf,
		size_t count, loff_t *offset)
{
	struct pn547_dev *pni = pn_info;
	int ret;
	int val;
	int i;
	i = 0;

	D("%s: start count = %u\n", __func__, count);

	if (count > MAX_BUFFER_SIZE) {
		E("%s : count =%d> MAX_BUFFER_SIZE\n", __func__, count);
		count = MAX_BUFFER_SIZE;
	}

	val = gpio_get_value(pni->irq_gpio);

	D("%s: reading %zu bytes, irq_gpio = %d\n",
		__func__, count, val);

	mutex_lock(&pni->read_mutex);

	if (!gpio_get_value(pni->irq_gpio)) {
		if (filp->f_flags & O_NONBLOCK) {
			I("%s : f_flags & O_NONBLOCK read again\n", __func__);
			ret = -EAGAIN;
			goto fail;
		}

		pni->irq_enabled = true;
		enable_irq(pni->client->irq);
		D("%s: waiting read-event INT, because "
			"irq_gpio = 0\n", __func__);
		pni->isReadBlock = true;
		ret = wait_event_interruptible(pni->read_wq,
				gpio_get_value(pni->irq_gpio));

		pn547_disable_irq(pni);

		D("%s : wait_event_interruptible done\n", __func__);

		if (ret) {
			I("pn547_dev_read wait_event_interruptible breaked ret=%d\n", ret);
			goto fail;
		}

	}

	pni->isReadBlock = false;
    wake_lock_timeout(&pni ->io_wake_lock, IO_WAKE_LOCK_TIMEOUT);
	
	memset(read_buffer, 0, MAX_BUFFER_SIZE);
	ret = pn547_RxData(read_buffer, count);
	mutex_unlock(&pni->read_mutex);

	if (ret < 0) {
		E("%s: i2c_master_recv returned %d\n", __func__, ret);
		return ret;
	}
	if (ret > count) {
		E("%s: received too many bytes from i2c (%d)\n",
			__func__, ret);
		return -EIO;
	}

	DBUF(read_buffer, count);

	if (copy_to_user(buf, read_buffer, count)) {
		E("%s : failed to copy to user space\n", __func__);
		return -EFAULT;
	}

	D("%s done count = %u\n", __func__, count);
	return count;

fail:
	mutex_unlock(&pni->read_mutex);
	return ret;
}

static ssize_t pn547_dev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *offset)
{
	struct pn547_dev *pni = pn_info;
	char buffer[MAX_BUFFER_SIZE];
	int ret;
	int i;
	i = 0;

	D("%s: start count = %u\n", __func__, count);
	wake_lock_timeout(&pni ->io_wake_lock, IO_WAKE_LOCK_TIMEOUT);

	if (count > MAX_BUFFER_SIZE) {
		E("%s : count =%d> MAX_BUFFER_SIZE\n", __func__, count);
		count = MAX_BUFFER_SIZE;
	}

	if ( is_debug && (s_wdcmd_cnt++ < 3))
		I("%s: writing %zu bytes\n",__func__, count);
	else {
		is_debug = 0;
		s_wdcmd_cnt = 4;
	}

	if (copy_from_user(buffer, buf, count)) {
		E("%s : failed to copy from user space\n", __func__);
		return -EFAULT;
	}

	DBUF(buffer, count);

	
	ret = pn547_TxData(buffer, count);
	if (ret < 0) {
		E("%s : i2c_master_send returned %d\n", __func__, ret);
		ret = -EIO;
	} else {
		D("%s done count = %u\n", __func__, count);
		return count;
	}

	return ret;
}

static int pn547_dev_open(struct inode *inode, struct file *filp)
{
	struct pn547_dev *pn547_dev = container_of(filp->private_data,
						struct pn547_dev,
						pn547_device);

	filp->private_data = pn547_dev;
	I("%s : major=%d, minor=%d\n", \
		__func__, imajor(inode), iminor(inode));

	return 0;
}

static long pn547_dev_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	struct pn547_dev *pni = pn_info;
	uint8_t buffer[] = {0x05, 0xF9, 0x04, 0x00, 0xC3, 0xE5};

	switch (cmd) {
	case PN547_SET_PWR:
		if (arg == 3) {
			
			I("%s Software reset\n", __func__);
			if (pn547_TxData(buffer, 6) < 0)
				E("%s, SW-Reset TxData error!\n", __func__);
		} else if (arg == 2) {
			I("%s power on with firmware\n", __func__);
			gpio_set_value(pni->pwr_en, 1);
			msleep(1);
			pn547_Enable();
			gpio_set_value(pni->firm_gpio, 1);
			msleep(50);
			pn547_Disable();
			msleep(50);
			pn547_Enable();
			msleep(50);
		} else if (arg == 1) {
			
			I("%s power on (delay50)\n", __func__);
			gpio_set_value(pni->pwr_en, 1);
			msleep(1);
			gpio_set_value(pni->firm_gpio, 0);
			pn547_Enable();
			msleep(50);
			is_debug = 1;
			s_wdcmd_cnt = 0;
			I("%s pn547_Enable, set is_debug = %d, s_wdcmd_cnt : %d\n", __func__, is_debug, s_wdcmd_cnt);
		} else  if (arg == 0) {
			
			I("%s power off (delay50)\n", __func__);
			gpio_set_value(pni->firm_gpio, 0);
			pn547_Disable();
			msleep(50);
			gpio_set_value(pni->pwr_en, 0);
			is_debug = 0;
			I("%s pn547_Disable, set is_debug = %d, s_wdcmd_cnt = %d\n", __func__, is_debug, s_wdcmd_cnt);
		} else {
			E("%s bad arg %lu\n", __func__, arg);
			goto fail;
		}
		break;
	default:
		E("%s bad ioctl %u\n", __func__, cmd);
		goto fail;
	}

	return 0;
fail:
	return -EINVAL;
}

static const struct file_operations pn547_dev_fops = {
	.owner	= THIS_MODULE,
	.llseek	= no_llseek,
	.read	= pn547_dev_read,
	.write	= pn547_dev_write,
	.open	= pn547_dev_open,
	.unlocked_ioctl  = pn547_dev_ioctl,
};

static ssize_t pn_temp1_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int val = -1;
	struct pn547_dev *pni = pn_info;
	uint8_t buffer[MAX_BUFFER_SIZE];
	int i = 0;

	I("%s:\n", __func__);
	val = gpio_get_value(pni->irq_gpio);

	memset(buffer, 0, MAX_BUFFER_SIZE);
#if 1
	if (val == 1) {
		ret = pn547_RxData(buffer, 33);
		if (ret < 0) {
			E("%s, i2c Rx error!\n", __func__);
		} else {
			for (i = 0; i < 10; i++)
			I("%s : [%d] = 0x%x\n", __func__, i, buffer[i]);
		}
	} else {
		E("%s, data not ready\n", __func__);
	}
#else
	if (val != 1)
		E("%s, ####### data not ready -> force to read!#########\n", __func__);

	ret = pn547_RxData(buffer, 33);
	if (ret < 0) {
		E("%s, i2c Rx error!\n", __func__);
	} else {
		for (i = 0; i < 10; i++)
		D("%s : [%d] = 0x%x\n", __func__, i, buffer[i]);
	}
#endif

	ret = sprintf(buf, "GPIO INT = %d "
		"Rx:ret=%d [0x%x, 0x%x, 0x%x, 0x%x]\n", val, ret, buffer[0], buffer[1],
		buffer[2], buffer[3]);

	return ret;
}


#define i2cw_size (20)
static ssize_t pn_temp1_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int code = -1;
	int ret = -1;
	struct pn547_dev *pni = pn_info;
	uint8_t buffer[] = {0x05, 0xF9, 0x04, 0x00, 0xC3, 0xE5};
	
	uint8_t i2cw[i2cw_size];
	uint32_t scan_data = 0;
	int i2cw_len = 0;
	int i = 0;
	char *ptr;

	sscanf(buf, "%d", &code);

	I("%s: irq = %d,  ven_gpio = %d,  firm_gpio = %d +\n", __func__, \
		gpio_get_value(pni->irq_gpio), pn547_isEn(), \
		gpio_get_value(pni->firm_gpio));
	I("%s: store value = %d\n", __func__, code);

	switch (code) {
	case 1:
			I("%s: case 1\n", __func__);
			ret = pn547_TxData(buffer, 6);
			if (ret < 0)
				E("%s, i2c Tx error!\n", __func__);
			break;
	case 2:
			I("%s: case 2 %d\n", __func__, pni->ven_gpio);
			pn547_Disable();
			break;
	case 3:
			I("%s: case 3 %d\n", __func__, pni->ven_gpio);
			pn547_Enable();
			break;
	case 4:
			I("%s: case 4 %d\n", __func__, pni->firm_gpio);
			gpio_set_value(pni->firm_gpio, 0);
			break;
	case 5:
			I("%s: case 5 %d\n", __func__, pni->firm_gpio);
			gpio_set_value(pni->firm_gpio, 1);
			break;
	case 6:
			memset(i2cw, 0, i2cw_size);
			sscanf(buf, "%d %d", &code, &i2cw_len);
			I("%s: case 6 i2cw_len=%u\n", __func__, i2cw_len);

			ptr = strpbrk(buf, " ");	
			if (ptr != NULL) {
				for (i = 0 ; i <= i2cw_len ; i++) {
					sscanf(ptr, "%x", &scan_data);
					i2cw[i] = (uint8_t)scan_data;
					I("%s: i2cw[%d]=%x\n", \
						__func__, i, i2cw[i]);
					ptr = strpbrk(++ptr, " ");
					if (ptr == NULL)
						break;
				}

				ret = pn547_TxData(i2cw, i2cw_len+1);
				

				if (ret < 0)
					E("%s, i2c Tx error!\n", __func__);
			} else {
				I("%s: skip no data found\n", __func__);
			}
			break;
	case 7:
			I("%s: case 7 disable i2c\n", __func__);
			ignoreI2C = 1;
			break;
	case 8:
			I("%s: case 8 enable i2c\n", __func__);
			ignoreI2C = 0;
			break;
	default:
			E("%s: case default\n", __func__);
			break;
	}

	I("%s: irq = %d,  ven_gpio = %d,  firm_gpio = %d -\n", __func__, \
		gpio_get_value(pni->irq_gpio), pn547_isEn(), gpio_get_value(pni->firm_gpio));
	return count;
}

static DEVICE_ATTR(pn_temp1, 0664, pn_temp1_show, pn_temp1_store);


static ssize_t debug_enable_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret = 0;
	I("debug_enable_show\n");

	ret = sprintf(buf, "is_debug=%d\n", is_debug);
	return ret;
}

static ssize_t debug_enable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	sscanf(buf, "%d", &is_debug);
	return count;
}

static DEVICE_ATTR(debug_enable, 0664, debug_enable_show, debug_enable_store);

static ssize_t nxp_chip_alive_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	I("%s is %d\n", __func__, is_alive);
	ret = sprintf(buf, "%d\n", is_alive);
	return ret;
}

static DEVICE_ATTR(nxp_chip_alive, 0664, nxp_chip_alive_show, NULL);

static ssize_t nxp_uicc_swp_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret = 0;
	I("%s is %d\n", __func__, is_uicc_swp);
	ret = sprintf(buf, "%d\n", is_uicc_swp);
	return ret;
}

static ssize_t nxp_uicc_swp_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	sscanf(buf, "%d", &is_uicc_swp);
	return count;
}

static DEVICE_ATTR(nxp_uicc_swp, 0664, nxp_uicc_swp_show, nxp_uicc_swp_store);

static int pn547_parse_dt(struct device *dev, struct pn547_i2c_platform_data *pdata)
{
	struct property *prop;
	struct device_node *dt = dev->of_node;
	I("%s: Start\n", __func__);

	prop = of_find_property(dt, "nxp,ven_isinvert", NULL);
	if (prop) {
		of_property_read_u32(dt, "nxp,ven_isinvert", &pdata->ven_isinvert);
		printk(KERN_INFO "[NFC] %s:ven_isinvert = %d", __func__, pdata->ven_isinvert);
	}

	prop = of_find_property(dt, "nxp,isalive", NULL);
	if (prop) {
		of_property_read_u32(dt, "nxp,isalive", &is_alive);
		printk(KERN_INFO "[NFC] %s:is_alive = %d", __func__, is_alive);
	}

	
	pdata->irq_gpio = of_get_named_gpio_flags(dt, "nxp,irq-gpio",
				0, &pdata->irq_gpio_flags);
	pdata->ven_gpio = of_get_named_gpio_flags(dt, "nxp,ven-gpio",
				0, &pdata->ven_gpio_flags);
	pdata->firm_gpio = of_get_named_gpio_flags(dt, "nxp,fwdl-gpio",
				0, &pdata->firm_gpio_flags);
	pdata->pwr_en = of_get_named_gpio_flags(dt, "nxp,pwr-en",
				0, &pdata->pwr_en_flags);
#if defined(CONFIG_NFC_CLK_PLL)	
	of_property_read_string(dt, "qcom,clk-src", &pdata->clk_src_name);
	if (strcmp(pdata->clk_src_name, "BBCLK2")){
	  	I("Get BBCLK2 fail from dts!\n"); 
	}	
	pdata->clkreq_gpio = of_get_named_gpio(dt, "nxp,clkreq-gpio", 0);
	I("%s:clk_name:%s, clkreq_gpio:%d\n", __func__, pdata->clk_src_name, pdata->clkreq_gpio);
#endif
	I("%s: End, irq_gpio:%d, ven_gpio:%d, firm_gpio:%d, pwr_en:%d,\n", __func__, pdata->irq_gpio, pdata->ven_gpio,pdata->firm_gpio, pdata->pwr_en);

	return 0;
}

#if defined(CONFIG_NFC_CLK_PLL)
static int nfc_clock_select(struct pn547_dev *pn547_dev)
{
	int ret = 0;
	I("enter nfc_clock_select\n");

	if (!strcmp(pn547_dev->clk_src_name, "BBCLK2")) {
		pn547_dev->s_clk  = clk_get(&pn547_dev->client->dev, "ref_clk");		
		if (pn547_dev->s_clk == NULL){
			I("pn547_dev->s_clk == NULL\n");
			ret = -1;
		} else{
			ret = clk_prepare_enable(pn547_dev->s_clk);	
			I("clk_enable ret=%d\n", ret);
			if(ret!=0){
				ret = -2;
			}
 		}
	} else{
			ret = -1;
	}

	I("exit nfc_clock_select\n");
	
	return ret;

}

static int nfc_clock_deselect(struct pn547_dev *pn547_dev)
{
	if (pn547_dev->s_clk != NULL) {
		 clk_disable_unprepare(pn547_dev->s_clk);
	}
	
	return 0;
}

#endif

static int pn547_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret;
	struct pn547_i2c_platform_data *platform_data;
	struct pn547_dev *pni;

	I("Enter %s:\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		E("%s : need I2C_FUNC_I2C\n", __func__);
		return  -ENODEV;
	}

	if (client->dev.of_node) {
		 platform_data = kzalloc(sizeof(*platform_data), GFP_KERNEL);
		 if (platform_data == NULL) {
			 E("%s : nfc probe fail because platform_data \
				 is NULL\n", __func__);
			 return  -ENODEV;
		 }
		 ret = pn547_parse_dt(&client->dev, platform_data);
		 if (ret) {
	                 E("%s : pn547_parse_dt fail\n", __func__);
	                 ret = -ENODEV;
	                 goto err_exit;
	         }
	} else {
		 platform_data = client->dev.platform_data;
		 if (platform_data == NULL) {
			 E("%s : nfc probe fail because platform_data \
				 is NULL\n", __func__);
			 return  -ENODEV;
		 }
	}

	
	ret = gpio_request(platform_data->irq_gpio, "nfc_int");
	if (ret) {
		E("%s : request gpio%d fail\n",
			__func__, platform_data->irq_gpio);
		ret = -ENODEV;
		goto err_exit;
	}

	
	ret = gpio_request(platform_data->ven_gpio, "nfc_en");
	if (ret) {
		E("%s : request gpio %d fail\n",
			__func__, platform_data->ven_gpio);
		ret = -ENODEV;
		goto err_request_gpio_ven;
	}
	

	ret = gpio_request(platform_data->firm_gpio, "nfc_firm");
	if (ret) {
		E("%s : request gpio %d fail\n",
			__func__, platform_data->firm_gpio);
		ret = -ENODEV;
		goto err_request_gpio_firm;
	}
	
	if (platform_data->pwr_en) {
		ret = gpio_request(platform_data->pwr_en, "pwr_en");
		if (ret){
			E("%s : request gpio %d fail\n",
				__func__, platform_data->pwr_en);
			ret = -ENODEV;
			goto err_request_gpio_firm;
		}

		ret = gpio_direction_output(platform_data->pwr_en, 1);
		if (ret) {
			E("unable to set direction for gpio [%d]\n", platform_data->pwr_en);
			goto err_pwr_en;
		}
		
		gpio_set_value(platform_data->pwr_en, 1);
	}

	pni = kzalloc(sizeof(struct pn547_dev), GFP_KERNEL);
	if (pni == NULL) {
		dev_err(&client->dev, \
				"pn547_probe : failed to allocate \
				memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	pn_info = pni;

	if (platform_data->gpio_init != NULL) {
		I("%s: gpio_init\n", __func__);
		platform_data->gpio_init();
	}

	pni->irq_gpio    = platform_data->irq_gpio;
	pni->ven_gpio    = platform_data->ven_gpio;
	pni->firm_gpio   = platform_data->firm_gpio;
	if (platform_data->pwr_en){
	pni->pwr_en      = platform_data->pwr_en;
	I("%s :pwr_en:%d\n", __func__, pni->pwr_en);
	}
	pni->client      = client;
	pni->gpio_init   = platform_data->gpio_init;
	pni->ven_enable  = !platform_data->ven_isinvert;
	pni->boot_mode   = board_mfg_mode();
	pni->isReadBlock = false;
	I("%s : irq_gpio:%d, ven_gpio:%d, firm_gpio:%d, ven_enable:%d\n", __func__, pni->irq_gpio, pni->ven_gpio, pni->firm_gpio, pni->ven_enable);

#if defined(CONFIG_NFC_CLK_PLL)
	
	I("%s: nfc_clk\n", platform_data->clk_src_name);
	pni->clk_src_name = platform_data->clk_src_name;
	ret = nfc_clock_select(pni);
	if (ret != 0) {
		if (ret == -1){
			E("BBCLK2 ERR.\n");
			goto err_clk_get;
		} else{
			E("Enable clk fail.\n");
			goto err_clk;
		}
	}

	if (!strcmp(pni->clk_src_name, "BBCLK2")) {
		if (gpio_is_valid(platform_data->clkreq_gpio)) {
			ret = gpio_request(platform_data->clkreq_gpio, "nfc_clkreq_gpio");
			if (ret) {
				E("unable to request gpio [%d]\n", platform_data->clkreq_gpio);
				goto err_clkreq_gpio;
			}
			ret = gpio_direction_input(platform_data->clkreq_gpio);
			if (ret) {
				E("unable to set direction for gpio [%d]\n", platform_data->clkreq_gpio);
				goto err_clkreq_gpio;
			}
		} else {
			E("clkreq gpio not provided\n");
			goto err_clk;
		}

		pni->clkreq_gpio = platform_data->clkreq_gpio;
	}
#endif

	

	
	init_waitqueue_head(&pni->read_wq);
	mutex_init(&pni->read_mutex);
	spin_lock_init(&pni->irq_enabled_lock);

	I("%s: init io_wake_lock\n", __func__);
	wake_lock_init(&pni->io_wake_lock, WAKE_LOCK_SUSPEND, PN547_I2C_NAME);

	pni->pn547_device.minor = MISC_DYNAMIC_MINOR;
	pni->pn547_device.name = "pn544";
	pni->pn547_device.fops = &pn547_dev_fops;

#if 1
	ret = misc_register(&pni->pn547_device);
#else
	ret = 0;
#endif
	if (ret) {
		E("%s : misc_register failed\n", __FILE__);
		goto err_misc_register;
	}


	client->irq = gpio_to_irq(platform_data->irq_gpio);
	I("%s : requesting IRQ %d\n", __func__, client->irq);

	pni->irq_enabled = true;
	ret = request_irq(client->irq, pn547_dev_irq_handler,
			  IRQF_TRIGGER_HIGH, client->name, pni);
	if (ret) {
		dev_err(&client->dev, "pn547_probe : request_irq failed\n");
		goto err_request_irq_failed;
	}
	pn547_disable_irq(pni);
	i2c_set_clientdata(client, pni);

	pni->pn547_class = class_create(THIS_MODULE, "NFC_sensor");
	if (IS_ERR(pni->pn547_class)) {
		ret = PTR_ERR(pni->pn547_class);
		pni->pn547_class = NULL;
		E("%s : class_create failed\n", __func__);
		goto err_create_class;
	}

	pni->pn_dev = device_create(pni->pn547_class, NULL, 0, "%s", "pn544");
	if (unlikely(IS_ERR(pni->pn_dev))) {
		ret = PTR_ERR(pni->pn_dev);
		pni->pn_dev = NULL;
		E("%s : device_create failed\n", __func__);
		goto err_create_pn_device;
	}

	
	ret = device_create_file(pni->pn_dev, &dev_attr_pn_temp1);
	if (ret) {
		E("%s : device_create_file dev_attr_pn_temp1 failed\n", __func__);
		goto err_create_pn_file;
	}

	ret = device_create_file(pni->pn_dev, &dev_attr_debug_enable);
	if (ret) {
		E("pn547_probe device_create_file dev_attr_debug_enable failed\n");
		goto err_create_pn_file;
	}

	pni->comn_dev = device_create(pni->pn547_class, NULL, 0, "%s", "comn");
	if (unlikely(IS_ERR(pni->comn_dev))) {
		ret = PTR_ERR(pni->comn_dev);
		pni->comn_dev = NULL;
		E("%s : device_create failed\n", __func__);
		goto err_create_pn_device;
	}

	ret = device_create_file(pni->comn_dev, &dev_attr_nxp_uicc_swp);
	if (ret) {
		E("pn547_probe device_create_file dev_attrnxp_uicc_swp failed\n");
	}

	if (is_alive) {
		
		if (pni->boot_mode != 5) {
			I("%s: disable NFC by default (bootmode = %d)\n", __func__, pni->boot_mode);
			pn547_Disable();
		} else if (pni->pwr_en){
			I("%s:NFC will pwr off when off-mode charging (bootmode = %d)\n", __func__, pni->boot_mode);	
			gpio_set_value(pni->firm_gpio, 0);
			gpio_set_value(pni->ven_gpio, 0);
			msleep(1);
			gpio_set_value(pni->pwr_en, 0);
			msleep(5);
			gpio_set_value(pni->ven_gpio, 1);
		}
	}

	if (is_alive == 0) {
		I("%s: Without NFC, device_create_file dev_attr_nxp_chip_alive \n", __func__);
		ret = device_create_file(pni->pn_dev, &dev_attr_nxp_chip_alive);
		if (ret) {
			E("pn547_probe device_create_file dev_attr_nxp_chip_alive failed\n");
			goto err_create_pn_file;
		}
	}

	I("%s: Probe success! is_alive : %d, is_uicc_swp : %d\n", __func__, is_alive, is_uicc_swp);
	return 0;

err_create_pn_file:
	device_unregister(pni->pn_dev);
err_create_pn_device:
	class_destroy(pni->pn547_class);
err_create_class:
err_request_irq_failed:
	misc_deregister(&pni->pn547_device);
#if defined(CONFIG_NFC_CLK_PLL)
err_clkreq_gpio:
	gpio_free(pni->clkreq_gpio);
#endif
err_pwr_en:
	gpio_free(pni->pwr_en);
#if defined(CONFIG_NFC_CLK_PLL)
err_clk:
	nfc_clock_deselect(pni);
err_clk_get:
#endif
err_misc_register:
	mutex_destroy(&pni->read_mutex);
	wake_lock_destroy(&pni->io_wake_lock);
	kfree(pni);
	pn_info = NULL;
	gpio_free(platform_data->firm_gpio);
err_request_gpio_firm:
	gpio_free(platform_data->ven_gpio);
err_request_gpio_ven:
	gpio_free(platform_data->irq_gpio);
err_exit:
	E("%s: prob fail\n", __func__);
	return ret;
}

static int pn547_remove(struct i2c_client *client)
{
	struct pn547_dev *pn547_dev;
	I("%s:\n", __func__);

	pn547_dev = i2c_get_clientdata(client);
	free_irq(client->irq, pn547_dev);
	misc_deregister(&pn547_dev->pn547_device);
	mutex_destroy(&pn547_dev->read_mutex);
	wake_lock_destroy(&pn547_dev->io_wake_lock);
	gpio_free(pn547_dev->irq_gpio);
	gpio_free(pn547_dev->ven_gpio);
	gpio_free(pn547_dev->firm_gpio);
#if defined(CONFIG_NFC_CLK_PLL)	
	if (!strcmp(pn547_dev->clk_src_name, "BBCLK2")) {
		gpio_free(pn547_dev->clkreq_gpio);
	}
#endif
	kfree(pn547_dev);
	pn_info = NULL;

	return 0;
}

#ifdef CONFIG_PM
static int pn547_suspend(struct i2c_client *client, pm_message_t state)
{
	struct pn547_dev *pni = pn_info;

        I("%s: irq = %d, ven_gpio = %d, isEn = %d, isReadBlock =%d\n", __func__, \
                gpio_get_value(pni->irq_gpio), gpio_get_value(pni->ven_gpio), pn547_isEn(), pni->isReadBlock);

	if (pni->ven_value && pni->isReadBlock && is_alive) {
		pni->irq_enabled = true;
		enable_irq(pni->client->irq);
		irq_set_irq_wake(pni->client->irq, 1);
	}

	return 0;
}

static int pn547_resume(struct i2c_client *client)
{
	struct pn547_dev *pni = pn_info;

        I("%s: irq = %d, ven_gpio = %d, isEn = %d, isReadBlock =%d\n", __func__, \
                gpio_get_value(pni->irq_gpio), gpio_get_value(pni->ven_gpio), pn547_isEn(), pni->isReadBlock);

	if (pni->ven_value && pni->isReadBlock && is_alive) {
		pn547_disable_irq(pni);
		irq_set_irq_wake(pni->client->irq, 0);
	}

	return 0;
}
#endif

static const struct i2c_device_id pn547_id[] = {
	{ "pn544", 0 },
	{ }
};

static struct of_device_id pn547_match_table[] = {
	{ .compatible = "nxp,pn544-nfc",},
	{ },
};

static struct i2c_driver pn547_driver = {
	.id_table	= pn547_id,
	.probe		= pn547_probe,
	.remove		= pn547_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "pn544",
		.of_match_table = pn547_match_table,
	},
#if CONFIG_PM
	.suspend	= pn547_suspend,
	.resume		= pn547_resume,
#endif
};


static int __init pn547_dev_init(void)
{
	I("%s: Loading pn547 driver\n", __func__);
	return i2c_add_driver(&pn547_driver);
}
module_init(pn547_dev_init);

static void __exit pn547_dev_exit(void)
{
	I("%s: Unloading pn547 driver\n", __func__);
	i2c_del_driver(&pn547_driver);
}
module_exit(pn547_dev_exit);

MODULE_AUTHOR("Sylvain Fonteneau");
MODULE_DESCRIPTION("NFC PN547 driver");
MODULE_LICENSE("GPL");
