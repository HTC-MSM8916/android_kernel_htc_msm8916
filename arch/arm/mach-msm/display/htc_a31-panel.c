#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <asm/mach-types.h>
#include <mach/msm_memtypes.h>
#include <mach/board.h>
#include <mach/debug_display.h>
#include "../../../../drivers/video/msm/mdss/mdss_dsi.h"

#define PANEL_ID_A31_SUCCESS_ILI9807 1
#define PANEL_ID_A31_SUCCESS_R61318A1 2
#define PANEL_ID_A31_BOOYI_OTM1284A 3

struct dsi_power_data {
	uint32_t sysrev;         
	struct regulator *vdda;  
	struct regulator *vdd;    
};

#ifdef MODULE
extern struct module __this_module;
#define THIS_MODULE (&__this_module)
#else
#define THIS_MODULE ((struct module *)0)
#endif

static int htc_a31_regulator_init(struct platform_device *pdev)
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

	
	pwrdata->vdda = devm_regulator_get(&pdev->dev, "vdda");
	if (IS_ERR(pwrdata->vdda)) {
		pr_err("%s: could not get vdda reg, rc=%ld\n",
			__func__, PTR_ERR(pwrdata->vdda));
		return PTR_ERR(pwrdata->vdda);
	}

	
	pwrdata->vdd = devm_regulator_get(&pdev->dev, "vdd");
	if (IS_ERR(pwrdata->vdd)) {
		pr_err("%s: could not get vdd reg, rc=%ld\n",
			__func__, PTR_ERR(pwrdata->vdd));
		return PTR_ERR(pwrdata->vdd);
	}

	ret = regulator_set_voltage(pwrdata->vdda, 1200000,
		1200000);
	if (ret) {
		pr_err("%s: set voltage failed on vdda vreg, rc=%d\n",
			__func__, ret);
		return ret;
	}

	ret = regulator_set_voltage(pwrdata->vdd, 2850000,
		2850000);
	if (ret) {
		pr_err("%s: set voltage failed on vdd vreg, rc=%d\n",
			__func__, ret);
		return ret;
	}

	return ret;
}

static int htc_a31_regulator_deinit(struct platform_device *pdev)
{
	
	return 0;
}

void htc_a31_panel_reset(struct mdss_panel_data *pdata, int enable)
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

		if (pdata->panel_info.panel_id == PANEL_ID_A31_SUCCESS_ILI9807 ||
			pdata->panel_info.panel_id == PANEL_ID_A31_SUCCESS_R61318A1 ||
			pdata->panel_info.panel_id == PANEL_ID_A31_BOOYI_OTM1284A) {
			gpio_direction_output((ctrl_pdata->rst_gpio), 1);
			usleep(20000);
			gpio_direction_output((ctrl_pdata->rst_gpio), 0);
			usleep(20000);
			gpio_direction_output((ctrl_pdata->rst_gpio), 1);
			usleep(120000);
		}
	} else {
		gpio_set_value((ctrl_pdata->rst_gpio), 0);
		if (!pdata->panel_info.first_power_on)
			gpio_free(ctrl_pdata->rst_gpio);
	}
}

static int htc_a31_panel_power_on(struct mdss_panel_data *pdata, int enable)
{
	int ret = 0;

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
		ret = regulator_set_optimum_mode(pwrdata->vdda, 100000);
		if (ret < 0) {
			pr_err("%s: vdda set opt mode failed.\n",
				__func__);
			return ret;
		}

		ret = regulator_set_optimum_mode(pwrdata->vdd, 100000);
		if (ret < 0) {
			pr_err("%s: vdd set opt mode failed.\n",
				__func__);
			return ret;
		}
		usleep_range(12000,12500);

		ret = regulator_enable(pwrdata->vdda);
		if (ret) {
			pr_err("%s: Failed to enable regulator.\n",
				__func__);
			return ret;
		}
		usleep_range(1000,1500);

		ret = regulator_enable(pwrdata->vdd);
		if (ret) {
			pr_err("%s: Failed to enable regulator.\n",
				__func__);
			return ret;
		}
		usleep_range(1000,1500);
	} else {
		usleep_range(2000,2500);
		gpio_set_value((ctrl_pdata->rst_gpio), 0);

		ret = regulator_disable(pwrdata->vdd);
		if (ret) {
			pr_err("%s: Failed to disable regulator : vdd.\n",
				__func__);
			return ret;
		}

#if 0
		ret = regulator_disable(pwrdata->vdda);
		if (ret) {
			pr_err("%s: Failed to disable regulator : vdda.\n",
				__func__);
			return ret;
		}
#endif

		ret = regulator_set_optimum_mode(pwrdata->vdd, 100);
		if (ret < 0) {
			pr_err("%s: vdd_vreg set opt mode failed.\n",
				__func__);
			return ret;
		}

#if 0
		ret = regulator_set_optimum_mode(pwrdata->vdda, 100);
		if (ret < 0) {
			pr_err("%s: vdda_vreg set opt mode failed.\n",
				__func__);
			return ret;
		}
#endif
	}

	PR_DISP_INFO("%s: en=%d done\n", __func__, enable);

	return ret;
}

static struct mdss_dsi_pwrctrl dsi_pwrctrl = {
	.dsi_regulator_init = htc_a31_regulator_init,
	.dsi_regulator_deinit = htc_a31_regulator_deinit,
	.dsi_power_on = htc_a31_panel_power_on,
	.dsi_panel_reset = htc_a31_panel_reset,
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
