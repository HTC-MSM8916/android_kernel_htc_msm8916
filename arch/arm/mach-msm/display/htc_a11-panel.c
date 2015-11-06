#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <asm/mach-types.h>
#include <mach/msm_memtypes.h>
#include <mach/board.h>
#include <mach/debug_display.h>
#include "../../../../drivers/video/msm/mdss/mdss_dsi.h"

#define PANEL_ID_A11_TIANMA_ORISE 1
#define PANEL_ID_A11_TRULY_ORISE 2

/* HTC: dsi_power_data overwrite the role of dsi_drv_cm_data
   in mdss_dsi_ctrl_pdata structure */
struct dsi_power_data {
	uint32_t sysrev;         /* system revision info */
	struct regulator *vddio; /* 1.8v */
	struct regulator *vdda;  /* 1.2v */
	struct regulator *vddpll; /* mipi 1.8v */
	struct regulator *vlcm3v; /* 3v */

	int lcm_bl_en;
};

#ifdef MODULE
extern struct module __this_module;
#define THIS_MODULE (&__this_module)
#else
#define THIS_MODULE ((struct module *)0)
#endif

static struct i2c_adapter	*i2c_bus_adapter = NULL;

struct i2c_dev_info {
	uint8_t				dev_addr;
	struct i2c_client	*client;
};

#define I2C_DEV_INFO(addr) \
	{.dev_addr = addr >> 1, .client = NULL}

static struct i2c_dev_info device_addresses[] = {
	I2C_DEV_INFO(0x6C)
};

static inline int platform_write_i2c_block(struct i2c_adapter *i2c_bus
								, u8 page
								, u8 offset
								, u16 count
								, u8 *values
								)
{
	struct i2c_msg msg;
	u8 *buffer;
	int ret;

	buffer = kmalloc(count + 1, GFP_KERNEL);
	if (!buffer) {
		pr_err("%s:%d buffer allocation failed\n",__FUNCTION__,__LINE__);
		return -ENOMEM;
	}

	buffer[0] = offset;
	memmove(&buffer[1], values, count);

	msg.flags = 0;
	msg.addr = page >> 1;
	msg.buf = buffer;
	msg.len = count + 1;

	ret = i2c_transfer(i2c_bus, &msg, 1);

	kfree(buffer);

	if (ret != 1) {
		pr_err("%s:%d I2c write failed 0x%02x:0x%02x\n"
				,__FUNCTION__,__LINE__, page, offset);
		ret = -EIO;
	} else {
		ret = 0;
	}

	return ret;
}

static int lm3630_add_i2c(struct i2c_client *client)
{
	struct i2c_adapter *adapter = client->adapter;
	int idx;

	/* "Hotplug" the MHL transmitter device onto the 2nd I2C bus  for BB-xM or 4th for pandaboard*/
	i2c_bus_adapter = adapter;
	if (i2c_bus_adapter == NULL) {
		PR_DISP_ERR("%s() failed to get i2c adapter\n", __func__);
		return ENODEV;
	}

	for (idx = 0; idx < ARRAY_SIZE(device_addresses); idx++) {
		if(idx == 0)
			device_addresses[idx].client = client;
		else {
			device_addresses[idx].client = i2c_new_dummy(i2c_bus_adapter,
											device_addresses[idx].dev_addr);
			if (device_addresses[idx].client == NULL){
				return ENODEV;
			}
		}
	}

	return 0;
}

static int lm3630_i2c_probe(struct i2c_client *client,
					      const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	int ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("[DISP] %s: Failed to i2c_check_functionality \n", __func__);
		return -EIO;
	}

	if (!client->dev.of_node) {
		pr_err("[DISP] %s: client->dev.of_node = NULL\n", __func__);
		return -ENOMEM;
	}

	ret = lm3630_add_i2c(client);

	if(ret < 0) {
		pr_err("[DISP] %s: Failed to lm3630_add_i2c, ret=%d\n", __func__,ret);
		return ret;
	}

	return 0;
}

static const struct i2c_device_id lm3630_id[] = {
	{"lm3630", 0}
};

static struct of_device_id lm3630_match_table[] = {
	{.compatible = "pwm-lm3630",}
};

static struct i2c_driver lm3630_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "lm3630",
		.of_match_table = lm3630_match_table,
		},
	.id_table = lm3630_id,
	.probe = lm3630_i2c_probe,
	.command = NULL,
};

static int htc_a11_regulator_init(struct platform_device *pdev)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct dsi_power_data *pwrdata = NULL;

	PR_DISP_INFO("%s\n", __func__);
	if (!pdev) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = platform_get_drvdata(pdev);
	if (!ctrl_pdata) {
		pr_err("%s: invalid driver data\n", __func__);
		return -EINVAL;
	}

	pwrdata = devm_kzalloc(&pdev->dev,
				sizeof(struct dsi_power_data), GFP_KERNEL);
	if (!pwrdata) {
		pr_err("[DISP] %s: FAILED to alloc pwrdata\n", __func__);
		return -ENOMEM;
	}

	ctrl_pdata->dsi_pwrctrl_data = pwrdata;

	pwrdata->vddio = devm_regulator_get(&pdev->dev, "vddio");//L15 LCM_IO 1.8V
	if (IS_ERR(pwrdata->vddio)) {
		pr_err("%s: could not get vddio reg, rc=%ld\n",
			__func__, PTR_ERR(pwrdata->vddio));
		return PTR_ERR(pwrdata->vddio);
	}

	pwrdata->vlcm3v = devm_regulator_get(&pdev->dev, "vdd");//L10 LCM_3V 3V
	if (IS_ERR(pwrdata->vlcm3v)) {
		pr_err("%s: could not get vdd reg, rc=%ld\n",
			__func__, PTR_ERR(pwrdata->vlcm3v));
		return PTR_ERR(pwrdata->vlcm3v);
	}

	ret = regulator_set_voltage(pwrdata->vddio, 1800000,
	        1800000);
	if (ret) {
		pr_err("%s: set voltage failed on vddio vreg, rc=%d\n",
			__func__, ret);
		return ret;
	}

#if 0 // L2 is not only for MIPI analog rails. it also is for DDR/eMMC power.
	pwrdata->vdda = devm_regulator_get(&pdev->dev, "vdda");//L2 MIPI 1.2V
	if (IS_ERR(pwrdata->vdda)) {
		pr_err("%s: could not get vdda vreg, rc=%ld\n",
			__func__, PTR_ERR(pwrdata->vdda));
		return PTR_ERR(pwrdata->vdda);
	}
	ret = regulator_set_voltage(pwrdata->vdda, 1200000,
		1200000);
	if (ret) {
	    pr_err("%s: set voltage failed on vdda vreg, rc=%d\n",
	        __func__, ret);
	    return ret;
	}
#endif
	ret = regulator_set_voltage(pwrdata->vlcm3v, 3000000,
	        3000000);
	if (ret) {
		pr_err("%s: set voltage failed on vlcm3v vreg, rc=%d\n",
			__func__, ret);
		return ret;
	}

	pwrdata->lcm_bl_en = of_get_named_gpio(pdev->dev.of_node,
						"htc,lcm_bl_en-gpio", 0);

	ret = i2c_add_driver(&lm3630_i2c_driver);
	if (ret < 0) {
		pr_err("[DISP] %s: FAILED to add i2c_add_driver ret=%x\n",
			__func__, ret);
	}
	return 0;
}

static int htc_a11_regulator_deinit(struct platform_device *pdev)
{
	/* devm_regulator() will automatically free regulators
	   while dev detach. */
	/* nothing */
	return 0;
}

void htc_a11_panel_reset(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	if (!gpio_is_valid(ctrl_pdata->rst_gpio)) {
		pr_err("%s:%d, reset line not configured\n",
			   __func__, __LINE__);
		return;
	}

	pr_debug("%s: enable = %d\n", __func__, enable);
	if (enable) {
		if (pdata->panel_info.first_power_on == 1) {
			PR_DISP_INFO("reset already on in first time\n");
			return;
		}

		if (gpio_request(ctrl_pdata->rst_gpio, "disp_rst_n")) {
			pr_err("%s: request reset gpio failed\n", __func__);
			return;
		}

		if (pdata->panel_info.panel_id == PANEL_ID_A11_TIANMA_ORISE ||
			pdata->panel_info.panel_id == PANEL_ID_A11_TRULY_ORISE) {
			gpio_direction_output((ctrl_pdata->rst_gpio), 1);
			usleep(2000);
			gpio_direction_output((ctrl_pdata->rst_gpio), 0);
			usleep(100);
			gpio_direction_output((ctrl_pdata->rst_gpio), 1);
			usleep(25000);
		}
	} else {
		gpio_set_value((ctrl_pdata->rst_gpio), 0);
		if (!pdata->panel_info.first_power_on)
			gpio_free(ctrl_pdata->rst_gpio);
	}
}

static void htc_a11_bkl_en(struct mdss_panel_data *pdata, int enable)
{
	static int en = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct dsi_power_data *pwrdata = NULL;
	u8 val = 0x00;

	if(en == enable)
		return;

	en = enable;
	PR_DISP_INFO("%s: en=%d\n", __func__, enable);

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
								panel_data);
	pwrdata = ctrl_pdata->dsi_pwrctrl_data;

	if (enable) {
		gpio_set_value(pwrdata->lcm_bl_en, 1);
		usleep(1000);

		/* Filter Strength */
		val = 0x02;
		platform_write_i2c_block(i2c_bus_adapter, 0x6C, 0x50, 1, &val);

		/* Configuration */
		/* Enable Feedback on Bank A, Enables the PWM for Bank A */
		val = 0x09;
		platform_write_i2c_block(i2c_bus_adapter, 0x6C, 0x01, 1, &val);

		/* Boost Control */
		/* frequency 500kHz, current limit 1.0A, voltage limit 40V */
		val = 0x70;
		platform_write_i2c_block(i2c_bus_adapter, 0x6C, 0x02, 1, &val);

		/* Current A setting */
		val = 0x14;
		platform_write_i2c_block(i2c_bus_adapter, 0x6C, 0x05, 1, &val);

		/* Control */
		/* Enables the LED A output */
		val = 0x04;
		platform_write_i2c_block(i2c_bus_adapter, 0x6C, 0x00, 1, &val);

		usleep(6000);

		/* Brightness A setting */
		val = 0xFF;
		platform_write_i2c_block(i2c_bus_adapter, 0x6C, 0x03, 1, &val);
	} else {
		gpio_set_value(pwrdata->lcm_bl_en, 0);
	}
}

static int htc_a11_panel_power_on(struct mdss_panel_data *pdata, int enable)
{
	int ret;

	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct dsi_power_data *pwrdata = NULL;

	PR_DISP_INFO("%s: en=%d\n", __func__, enable);
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	pwrdata = ctrl_pdata->dsi_pwrctrl_data;

	if (!pwrdata) {
		pr_err("%s: pwrdata not initialized\n", __func__);
		return -EINVAL;
	}

	if (enable) {
		ret = regulator_set_optimum_mode(pwrdata->vddio, 100000);
		if (ret < 0) {
			pr_err("%s: vddio set opt mode failed.\n",
				__func__);
			return ret;
		}

#if 0 // L2 is not only for MIPI analog rails. it also is for DDR/eMMC power.
		ret = regulator_set_optimum_mode(pwrdata->vdda, 100000);
		if (ret < 0) {
			pr_err("%s: vdda set opt mode failed.\n",
				__func__);
			return ret;
		}
#endif

		ret = regulator_set_optimum_mode(pwrdata->vlcm3v, 100000);
		if (ret < 0) {
			pr_err("%s: vlcm3v set opt mode failed.\n",
				__func__);
			return ret;
		}
		usleep_range(12000,12500);

		ret = regulator_enable(pwrdata->vddio);//L15 LCM_IO 1.8V
		if (ret) {
			pr_err("%s: Failed to enable regulator.\n",
				__func__);
			return ret;
		}
		usleep_range(1000,1500);

		ret = regulator_enable(pwrdata->vlcm3v);//L10 LCM_3V 3V
		if (ret) {
			pr_err("%s: Failed to enable regulator : vlcm3v.\n",
				__func__);
			return ret;
		}
		usleep_range(1000,1500);

#if 0 // L2 is not only for MIPI analog rails. it also is for DDR/eMMC power.
		/*ENABLE 1V2*/
		ret = regulator_enable(pwrdata->vdda);//L2 MIPI 1.2V
		if (ret) {
			pr_err("%s: Failed to enable regulator : vdda.\n",
				__func__);
			return ret;
		}
		msleep(20);
#endif
	} else {
		usleep_range(2000,2500);
		gpio_set_value((ctrl_pdata->rst_gpio), 0);

#if 0 // L2 is not only for MIPI analog rails. it also is for DDR/eMMC power.
		ret = regulator_disable(pwrdata->vdda);//L2 MIPI 1.2V
		if (ret) {
			pr_err("%s: Failed to disable regulator.\n",
				__func__);
			return ret;
		}
#endif

		ret = regulator_disable(pwrdata->vlcm3v);//L10 LCM_3V 3V
		if (ret) {
			pr_err("%s: Failed to disable regulator : vlcm3v.\n",
				__func__);
			return ret;
		}

		ret = regulator_disable(pwrdata->vddio);//L15 LCM_IO 1.8V
		if (ret) {
			pr_err("%s: Failed to disable regulator : vddio.\n",
				__func__);
			return ret;
		}

		usleep_range(2000,2500);

		ret = regulator_set_optimum_mode(pwrdata->vddio, 100);
		if (ret < 0) {
			pr_err("%s: vdd_io_vreg set opt mode failed.\n",
				__func__);
			return ret;
		}

		ret = regulator_set_optimum_mode(pwrdata->vlcm3v, 100);
		if (ret < 0) {
			pr_err("%s: vlcm3v_vreg set opt mode failed.\n",
				__func__);
			return ret;
		}

#if 0 // L2 is not only for MIPI analog rails. it also is for DDR/eMMC power.
		ret = regulator_set_optimum_mode(pwrdata->vdda, 100);
		if (ret < 0) {
			pr_err("%s: vdda_vreg set opt mode failed.\n",
				__func__);
			return ret;
		}
#endif
	}
	PR_DISP_INFO("%s: en=%d done\n", __func__, enable);

	return 0;
}

static struct mdss_dsi_pwrctrl dsi_pwrctrl = {
	.dsi_regulator_init = htc_a11_regulator_init,
	.dsi_regulator_deinit = htc_a11_regulator_deinit,
	.dsi_power_on = htc_a11_panel_power_on,
	.dsi_panel_reset = htc_a11_panel_reset,
	.bkl_config = htc_a11_bkl_en,
};

static struct platform_device dsi_pwrctrl_device = {
	.name          = "mdss_dsi_pwrctrl",
	.id            = -1,
	.dev.platform_data = &dsi_pwrctrl,
};

int __init htc_8916_dsi_panel_power_register(void)
{
	pr_info("%s#%d\n", __func__, __LINE__);
	platform_device_register(&dsi_pwrctrl_device);
	return 0;
}
