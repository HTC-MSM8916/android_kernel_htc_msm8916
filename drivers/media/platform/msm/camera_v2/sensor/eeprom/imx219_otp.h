#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/crc32.h>
#include "msm_sd.h"
#include "msm_cci.h"
#include "msm_eeprom.h"



int imx219_eeprom_parse_memory_map(struct device_node *of,
				       struct msm_eeprom_memory_block_t *data);
				       
				       
 int imx219_read_eeprom_memory(struct msm_eeprom_ctrl_t *e_ctrl,
			      struct msm_eeprom_memory_block_t *block);
