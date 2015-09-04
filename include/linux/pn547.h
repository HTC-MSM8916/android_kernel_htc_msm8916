/*
 * Copyright (C) 2010 NXP Semiconductors
 */

#define PN547_I2C_NAME "pn544"

#define PN547_MAGIC	0xE9
#define PN547_SET_PWR	_IOW(PN547_MAGIC, 0x01, unsigned int)
#define IO_WAKE_LOCK_TIMEOUT (2*HZ)

struct pn547_i2c_platform_data {
	void (*gpio_init) (void);
	unsigned int irq_gpio;
	uint32_t irq_gpio_flags;
	unsigned int ven_gpio;
	uint32_t ven_gpio_flags;
	unsigned int firm_gpio;
	uint32_t firm_gpio_flags;
	unsigned int pwr_en;
	uint32_t pwr_en_flags;
	unsigned int ven_isinvert;
	const char *clk_src_name;
	
	unsigned int		clkreq_gpio;
};
