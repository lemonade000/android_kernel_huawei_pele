/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__

#include <linux/module.h>
#include "msm_led_flash.h"
#include "msm_camera_io_util.h"
#include "../msm_sensor.h"
#include "msm_led_flash.h"
#include "../cci/msm_cci.h"
#include <linux/debugfs.h>

/* < DTS2014042602749 yangjiangjun 20140426 begin */
#ifdef CONFIG_HUAWEI_HW_DEV_DCT
#include <linux/hw_dev_dec.h>
#endif
/* <DTS2014122607866 zhangbo00166742 20141226 begin */
#define LM3642_CHIP_ID_MASK 0x07
#define LM3642_CHIP_ID 0x0
#define LM3646_CHIP_ID_MASK 0x38
#define LM3646_CHIP_ID 0x10
/*DTS2014122607866 zhangbo00166742 20141226 end >*/
/* DTS2014042602749 yangjiangjun 20140426 end >*/

#define FLASH_NAME "camera-led-flash"
#define CAM_FLASH_PINCTRL_STATE_SLEEP "cam_flash_suspend"
#define CAM_FLASH_PINCTRL_STATE_DEFAULT "cam_flash_default"
/*#define CONFIG_MSMB_CAMERA_DEBUG*/
#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
/* < DTS2014042700594 yangjiangjun 20140427 begin */
#define TEMPERATUE_NORMAL 1  //normal
#define TEMPERATUE_ABNORMAL 0 //abnormal
/* < DTS2015071400818 yuguangcai 20150714 begin */
//delete one line code
/* DTS2015071400818 yuguangcai 20150714 end > */

extern int msm_flash_lm3642_led_off(struct msm_led_flash_ctrl_t *fctrl);
/* <DTS2014122607866 zhangbo00166742 20141226 begin */
extern int msm_flash_lm3646_led_off(struct msm_led_flash_ctrl_t *fctrl);
/*DTS2014122607866 zhangbo00166742 20141226 end >*/
/* DTS2014042700594 yangjiangjun 20140427 end > */

int32_t msm_led_i2c_trigger_get_subdev_id(struct msm_led_flash_ctrl_t *fctrl,
	void *arg)
{
	uint32_t *subdev_id = (uint32_t *)arg;
	if (!subdev_id) {
		pr_err("failed\n");
		return -EINVAL;
	}
	*subdev_id = fctrl->subdev_id;

	CDBG("subdev_id %d\n", *subdev_id);
	return 0;
}

int32_t msm_led_i2c_trigger_config(struct msm_led_flash_ctrl_t *fctrl,
	void *data)
{
	int rc = 0;
	int i = 0;
	struct msm_camera_led_cfg_t *cfg = (struct msm_camera_led_cfg_t *)data;
	CDBG("called led_state %d\n", cfg->cfgtype);

	if (!fctrl->func_tbl) {
		pr_err("failed\n");
		return -EINVAL;
	}

	/* < DTS2014042700594 yangjiangjun 20140427 begin */
	/* < DTS2015071400818 yuguangcai 20150714 begin */
	//We want to light led,but temperature flag is abnormal, just return
	//[NOTICE]if any new command added to light led, we must add it here too
	if(MSM_CAMERA_LED_LOW == cfg->cfgtype
	|| MSM_CAMERA_LED_HIGH == cfg->cfgtype || MSM_CAMERA_LED_TORCH == cfg->cfgtype){
		if(TEMPERATUE_ABNORMAL == fctrl->flash_temperature_flag[fctrl->camera_id]){
			pr_err("led can not work for camera_id %d\n", fctrl->camera_id);
			return 0;
		}
	}
	/* DTS2015071400818 yuguangcai 20150714 end > */
	/* DTS2014042700594 yangjiangjun 20140427 end > */
	/* < DTS2015072600584 zhangbo 20150812 begin */
	fctrl->flip_id = cfg->flip_id;
	/* DTS2015072600584 Zhangbo 20150812 end  >*/

	switch (cfg->cfgtype) {

	case MSM_CAMERA_LED_INIT:
		if (fctrl->func_tbl->flash_led_init)
			rc = fctrl->func_tbl->flash_led_init(fctrl);
		/* <DTS2014122607866 zhangbo00166742 20141226 begin */
		/*<DTS2015031302796 Zhangbo 20150311 begin*/
		if (DUAL_LED_MODE_OFF == cfg->DualLedMode)
		{
			for (i = 0; i < MAX_LED_TRIGGERS; i++) {
				cfg->flash_current[i] =
					fctrl->flash_max_current[i];
				cfg->flash_duration[i] =
					fctrl->flash_max_duration[i];
				cfg->torch_current[i] =
					fctrl->torch_max_current[i];
			}
		}
		/*DTS2015031302796 Zhangbo 20150311 end>*/
		/*DTS2014122607866 zhangbo00166742 20141226 end >*/
		break;

	case MSM_CAMERA_LED_RELEASE:
		if (fctrl->func_tbl->flash_led_release)
			rc = fctrl->func_tbl->
				flash_led_release(fctrl);
		break;

	case MSM_CAMERA_LED_OFF:
		if (fctrl->func_tbl->flash_led_off)
			rc = fctrl->func_tbl->flash_led_off(fctrl);
		break;

	case MSM_CAMERA_LED_LOW:
		/* <DTS2014122607866 zhangbo00166742 20141226 begin */
		/*<DTS2015031302796 Zhangbo 20150311 begin*/
		/* < DTS2015032400512 Zhangbo 20150320 begin */
		{
			for (i = 0; i < fctrl->torch_num_sources; i++) {
				if (fctrl->torch_max_current[i] > 0) {
					fctrl->torch_op_current[i] =
						(cfg->torch_current[i] < fctrl->torch_max_current[i]) ?
						cfg->torch_current[i] : fctrl->torch_max_current[i];
					CDBG("torch source%d: op_current %d max_current %d\n",
						i, fctrl->torch_op_current[i], fctrl->torch_max_current[i]);
				}
			}
			if (fctrl->func_tbl->flash_led_low)
				rc = fctrl->func_tbl->flash_led_low(fctrl);		
		}
		/* DTS2015032400512 Zhangbo 20150320 end > */
		/*DTS2015031302796 Zhangbo 20150311 end>*/
		/*DTS2014122607866 zhangbo00166742 20141226 end >*/
		break;

	case MSM_CAMERA_LED_HIGH:
		/* <DTS2014122607866 zhangbo00166742 20141226 begin */
		/*<DTS2015031302796 Zhangbo 20150311 begin*/
		{
			if (DUAL_LED_MODE_OFF != cfg->DualLedMode)
			{
				cfg->DualLedMode = DUAL_LED_MODE_FLASH;
				if (fctrl->func_tbl->flash_led_on)
					rc = fctrl->func_tbl->flash_led_on(fctrl, data);
			}
			else
			{
				for (i = 0; i < fctrl->flash_num_sources; i++) {
					if (fctrl->flash_max_current[i] > 0) {
						fctrl->flash_op_current[i] =
							(cfg->flash_current[i] < fctrl->flash_max_current[i]) ?
							cfg->flash_current[i] : fctrl->flash_max_current[i];
						CDBG("flash source%d: op_current %d max_current %d\n",
							i, fctrl->flash_op_current[i], fctrl->flash_max_current[i]);
					}
				}
				if (fctrl->func_tbl->flash_led_high)
					rc = fctrl->func_tbl->flash_led_high(fctrl);
			}
		}
		/*DTS2015031302796 Zhangbo 20150311 end>*/
		/*DTS2014122607866 zhangbo00166742 20141226 end >*/
		break;

	/* < DTS2014042700594 yangjiangjun 20140427 begin */
	case MSM_CAMERA_LED_TORCH:
		/* <DTS2014122607866 zhangbo00166742 20141226 begin */
		/*<DTS2015031302796 Zhangbo 20150311 begin*/
		/* < DTS2015032400512 Zhangbo 20150320 begin */
		/*< DTS2015032411021  Zhangbo 20150325 begin */
		/* < DTS2015041303206 Zhangbo 20150417 begin */
		{
			if (DUAL_LED_MODE_OFF != cfg->DualLedMode)
			{
				fctrl->df_torch_type = cfg->DfTorchIndex;
			}
			
			if (fctrl->func_tbl->torch_led_on)
			{
				msleep(200);    //have to sleep to solve the flash problem of torch app
				rc = fctrl->func_tbl->torch_led_on(fctrl);
			}
		}
		/* DTS2015041303206 Zhangbo 20150417 end > */
		/* DTS2015032411021 Zhangbo 20150325 end >*/
		/* DTS2015032400512 Zhangbo 20150320 end > */
		/*DTS2015031302796 Zhangbo 20150311 end>*/
		/*DTS2014122607866 zhangbo00166742 20141226 end >*/
		break;

	//normal
	/* < DTS2015071400818 yuguangcai 20150714 begin */
	/* < DTS2015072504784 yuguangcai 20150725 begin */
	case MSM_CAMERA_LED_TORCH_POWER_NORMAL:
		pr_err("resume the led for camera_id %d \n", cfg->camera_id);
		fctrl->flash_temperature_flag[cfg->camera_id] = TEMPERATUE_NORMAL;
		break;
	//abnormal
	case MSM_CAMERA_LED_TORCH_POWER_ABNORMAL:
		//need run MSM_CAMERA_LED_OFF to take off the led
		pr_err("disable the led for camera_id %d\n", cfg->camera_id);
		fctrl->flash_temperature_flag[cfg->camera_id] = TEMPERATUE_ABNORMAL;
		//close flash
		if (cfg->camera_id == fctrl->camera_id
			&& fctrl->func_tbl->flash_led_off)
		{
			pr_err("turn off led for temperature issue\n");
			rc = fctrl->func_tbl->flash_led_off(fctrl);
		}
		break;
	/* DTS2015072504784 yuguangcai 20150725 end > */
	/* DTS2014042700594 yangjiangjun 20140427 end > */
	case MSM_CAMERA_LED_CAMERA_ID:
		pr_err("set current camera_id = %d \n", cfg->camera_id);
		fctrl->camera_id = cfg->camera_id;
		break;
	/* DTS2015071400818 yuguangcai 20150714 end > */

	default:
		rc = -EFAULT;
		break;
	}
	CDBG("flash_set_led_state: return %d\n", rc);
	return rc;
}
static int msm_flash_pinctrl_init(struct msm_led_flash_ctrl_t *ctrl)
{
	struct msm_pinctrl_info *flash_pctrl = NULL;

	flash_pctrl = &ctrl->pinctrl_info;
	flash_pctrl->pinctrl = devm_pinctrl_get(&ctrl->pdev->dev);

	if (IS_ERR_OR_NULL(flash_pctrl->pinctrl)) {
		pr_err("%s:%d Getting pinctrl handle failed\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	flash_pctrl->gpio_state_active = pinctrl_lookup_state(
					       flash_pctrl->pinctrl,
					       CAM_FLASH_PINCTRL_STATE_DEFAULT);

	if (IS_ERR_OR_NULL(flash_pctrl->gpio_state_active)) {
		pr_err("%s:%d Failed to get the active state pinctrl handle\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	flash_pctrl->gpio_state_suspend = pinctrl_lookup_state(
						flash_pctrl->pinctrl,
						CAM_FLASH_PINCTRL_STATE_SLEEP);

	if (IS_ERR_OR_NULL(flash_pctrl->gpio_state_suspend)) {
		pr_err("%s:%d Failed to get the suspend state pinctrl handle\n",
				__func__, __LINE__);
		return -EINVAL;
	}
	return 0;
}


int msm_flash_led_init(struct msm_led_flash_ctrl_t *fctrl)
{
	int rc = 0;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	CDBG("%s:%d called\n", __func__, __LINE__);

	flashdata = fctrl->flashdata;
	power_info = &flashdata->power_info;
	fctrl->led_state = MSM_CAMERA_LED_RELEASE;
	if (power_info->gpio_conf->cam_gpiomux_conf_tbl != NULL) {
		pr_err("%s:%d mux install\n", __func__, __LINE__);
	}

	/* CCI Init */
	if (fctrl->flash_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = fctrl->flash_i2c_client->i2c_func_tbl->i2c_util(
			fctrl->flash_i2c_client, MSM_CCI_INIT);
		if (rc < 0) {
			pr_err("cci_init failed\n");
			return rc;
		}
	}
	rc = msm_camera_request_gpio_table(
		power_info->gpio_conf->cam_gpio_req_tbl,
		power_info->gpio_conf->cam_gpio_req_tbl_size, 1);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
		return rc;
	}

	if (fctrl->pinctrl_info.use_pinctrl == true) {
		CDBG("%s:%d PC:: flash pins setting to active state",
				__func__, __LINE__);
		rc = pinctrl_select_state(fctrl->pinctrl_info.pinctrl,
				fctrl->pinctrl_info.gpio_state_active);
		if (rc < 0) {
			devm_pinctrl_put(fctrl->pinctrl_info.pinctrl);
			pr_err("%s:%d cannot set pin to active state",
					__func__, __LINE__);
		}
	}
	msleep(20);

	CDBG("before FL_RESET\n");
	if (power_info->gpio_conf->gpio_num_info->
			valid[SENSOR_GPIO_FL_RESET] == 1)
		gpio_set_value_cansleep(
			power_info->gpio_conf->gpio_num_info->
			gpio_num[SENSOR_GPIO_FL_RESET],
			GPIO_OUT_HIGH);

	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_EN],
		GPIO_OUT_HIGH);

	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_NOW],
		GPIO_OUT_HIGH);

	if (fctrl->flash_i2c_client && fctrl->reg_setting) {
		rc = fctrl->flash_i2c_client->i2c_func_tbl->i2c_write_table(
			fctrl->flash_i2c_client,
			fctrl->reg_setting->init_setting);
		if (rc < 0)
			pr_err("%s:%d failed\n", __func__, __LINE__);
	}
	fctrl->led_state = MSM_CAMERA_LED_INIT;
	return rc;
}

int msm_flash_led_release(struct msm_led_flash_ctrl_t *fctrl)
{
	int rc = 0, ret = 0;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;

	CDBG("%s:%d called\n", __func__, __LINE__);
	if (!fctrl) {
		pr_err("%s:%d fctrl NULL\n", __func__, __LINE__);
		return -EINVAL;
	}
	flashdata = fctrl->flashdata;
	power_info = &flashdata->power_info;

	if (fctrl->led_state != MSM_CAMERA_LED_INIT) {
		pr_err("%s:%d invalid led state\n", __func__, __LINE__);
		return -EINVAL;
	}
	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_EN],
		GPIO_OUT_LOW);
	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_NOW],
		GPIO_OUT_LOW);
	if (power_info->gpio_conf->gpio_num_info->
			valid[SENSOR_GPIO_FL_RESET] == 1)
		gpio_set_value_cansleep(
			power_info->gpio_conf->gpio_num_info->
			gpio_num[SENSOR_GPIO_FL_RESET],
			GPIO_OUT_LOW);

	if (fctrl->pinctrl_info.use_pinctrl == true) {
		ret = pinctrl_select_state(fctrl->pinctrl_info.pinctrl,
				fctrl->pinctrl_info.gpio_state_suspend);
		if (ret < 0) {
			devm_pinctrl_put(fctrl->pinctrl_info.pinctrl);
			pr_err("%s:%d cannot set pin to suspend state",
				__func__, __LINE__);
		}
	}
	rc = msm_camera_request_gpio_table(
		power_info->gpio_conf->cam_gpio_req_tbl,
		power_info->gpio_conf->cam_gpio_req_tbl_size, 0);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
		return rc;
	}

	fctrl->led_state = MSM_CAMERA_LED_RELEASE;
	/* CCI deInit */
	if (fctrl->flash_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = fctrl->flash_i2c_client->i2c_func_tbl->i2c_util(
			fctrl->flash_i2c_client, MSM_CCI_RELEASE);
		if (rc < 0)
			pr_err("cci_deinit failed\n");
	}

	return 0;
}

int msm_flash_led_off(struct msm_led_flash_ctrl_t *fctrl)
{
	int rc = 0;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;

	if (!fctrl) {
		pr_err("%s:%d fctrl NULL\n", __func__, __LINE__);
		return -EINVAL;
	}
	flashdata = fctrl->flashdata;
	power_info = &flashdata->power_info;
	CDBG("%s:%d called\n", __func__, __LINE__);
	if (fctrl->flash_i2c_client && fctrl->reg_setting) {
		rc = fctrl->flash_i2c_client->i2c_func_tbl->i2c_write_table(
			fctrl->flash_i2c_client,
			fctrl->reg_setting->off_setting);
		if (rc < 0)
			pr_err("%s:%d failed\n", __func__, __LINE__);
	}
	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_NOW],
		GPIO_OUT_LOW);

	return rc;
}

int msm_flash_led_low(struct msm_led_flash_ctrl_t *fctrl)
{
	int rc = 0;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	CDBG("%s:%d called\n", __func__, __LINE__);

	flashdata = fctrl->flashdata;
	power_info = &flashdata->power_info;
	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_EN],
		GPIO_OUT_HIGH);

	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_NOW],
		GPIO_OUT_HIGH);


	if (fctrl->flash_i2c_client && fctrl->reg_setting) {
		rc = fctrl->flash_i2c_client->i2c_func_tbl->i2c_write_table(
			fctrl->flash_i2c_client,
			fctrl->reg_setting->low_setting);
		if (rc < 0)
			pr_err("%s:%d failed\n", __func__, __LINE__);
	}

	return rc;
}

int msm_flash_led_high(struct msm_led_flash_ctrl_t *fctrl)
{
	int rc = 0;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	CDBG("%s:%d called\n", __func__, __LINE__);

	flashdata = fctrl->flashdata;
	power_info = &flashdata->power_info;
	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_EN],
		GPIO_OUT_HIGH);

	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_NOW],
		GPIO_OUT_HIGH);

	if (fctrl->flash_i2c_client && fctrl->reg_setting) {
		rc = fctrl->flash_i2c_client->i2c_func_tbl->i2c_write_table(
			fctrl->flash_i2c_client,
			fctrl->reg_setting->high_setting);
		if (rc < 0)
			pr_err("%s:%d failed\n", __func__, __LINE__);
	}

	return rc;
}

static int32_t msm_led_get_dt_data(struct device_node *of_node,
		struct msm_led_flash_ctrl_t *fctrl)
{
	int32_t rc = 0, i = 0;
	struct msm_camera_gpio_conf *gconf = NULL;
	struct device_node *flash_src_node = NULL;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	uint32_t count = 0;
	uint16_t *gpio_array = NULL;
	uint16_t gpio_array_size = 0;
	uint32_t id_info[3];

	CDBG("called\n");

	if (!of_node) {
		pr_err("of_node NULL\n");
		return -EINVAL;
	}

	fctrl->flashdata = kzalloc(sizeof(
		struct msm_camera_sensor_board_info),
		GFP_KERNEL);
	if (!fctrl->flashdata) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}

	flashdata = fctrl->flashdata;
	power_info = &flashdata->power_info;

	rc = of_property_read_u32(of_node, "cell-index", &fctrl->subdev_id);
	if (rc < 0) {
		pr_err("failed\n");
		return -EINVAL;
	}

	CDBG("subdev id %d\n", fctrl->subdev_id);

	rc = of_property_read_string(of_node, "label",
		&flashdata->sensor_name);
	CDBG("%s label %s, rc %d\n", __func__,
		flashdata->sensor_name, rc);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR1;
	}

	/* < DTS2014042700594 yangjiangjun 20140427 begin */
	// Get the flash high current from .dtsi file. If failed to get the value of current,
	// set register as the default value 1031.25mA.
	rc = of_property_read_u32(of_node, "qcom,flash-high-current", &fctrl->flash_high_current);
	if (rc < 0) {
		pr_err("get flash_high_current failed\n");
	}

	fctrl->reg_setting->high_setting->reg_setting[0].reg_data = fctrl->flash_high_current;
	CDBG("flash_high_current %d\n", fctrl->flash_high_current);
	/* DTS2014042700594 yangjiangjun 20140427 end > */
	
	rc = of_property_read_u32(of_node, "qcom,cci-master",
		&fctrl->cci_i2c_master);
	CDBG("%s qcom,cci-master %d, rc %d\n", __func__, fctrl->cci_i2c_master,
		rc);
	if (rc < 0) {
		/* Set default master 0 */
		fctrl->cci_i2c_master = MASTER_0;
		rc = 0;
	}

	fctrl->pinctrl_info.use_pinctrl = false;
	fctrl->pinctrl_info.use_pinctrl = of_property_read_bool(of_node,
						"qcom,enable_pinctrl");
	if (of_get_property(of_node, "qcom,flash-source", &count)) {
		count /= sizeof(uint32_t);
		CDBG("count %d\n", count);
		if (count > MAX_LED_TRIGGERS) {
			pr_err("failed\n");
			return -EINVAL;
		}
		for (i = 0; i < count; i++) {
			flash_src_node = of_parse_phandle(of_node,
				"qcom,flash-source", i);
			if (!flash_src_node) {
				pr_err("flash_src_node NULL\n");
				continue;
			}

			rc = of_property_read_string(flash_src_node,
				"linux,default-trigger",
				&fctrl->flash_trigger_name[i]);
			if (rc < 0) {
				pr_err("failed\n");
				of_node_put(flash_src_node);
				continue;
			}

			CDBG("default trigger %s\n",
				 fctrl->flash_trigger_name[i]);

			rc = of_property_read_u32(flash_src_node,
				"qcom,max-current",
				&fctrl->flash_op_current[i]);
			if (rc < 0) {
				pr_err("failed rc %d\n", rc);
				of_node_put(flash_src_node);
				continue;
			}

			of_node_put(flash_src_node);

			CDBG("max_current[%d] %d\n",
				i, fctrl->flash_op_current[i]);

			led_trigger_register_simple(
				fctrl->flash_trigger_name[i],
				&fctrl->flash_trigger[i]);
		}

	} else { /*Handle LED Flash Ctrl by GPIO*/
		power_info->gpio_conf =
			 kzalloc(sizeof(struct msm_camera_gpio_conf),
				 GFP_KERNEL);
		if (!power_info->gpio_conf) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			rc = -ENOMEM;
			return rc;
		}
		gconf = power_info->gpio_conf;

		gpio_array_size = of_gpio_count(of_node);
		CDBG("%s gpio count %d\n", __func__, gpio_array_size);

		if (gpio_array_size) {
			gpio_array = kzalloc(sizeof(uint16_t) * gpio_array_size,
				GFP_KERNEL);
			if (!gpio_array) {
				pr_err("%s failed %d\n", __func__, __LINE__);
				rc = -ENOMEM;
				goto ERROR4;
			}
			for (i = 0; i < gpio_array_size; i++) {
				gpio_array[i] = of_get_gpio(of_node, i);
				CDBG("%s gpio_array[%d] = %d\n", __func__, i,
					gpio_array[i]);
			}

			rc = msm_camera_get_dt_gpio_req_tbl(of_node, gconf,
				gpio_array, gpio_array_size);
			if (rc < 0) {
				pr_err("%s failed %d\n", __func__, __LINE__);
				goto ERROR4;
			}

			rc = msm_camera_get_dt_gpio_set_tbl(of_node, gconf,
				gpio_array, gpio_array_size);
			if (rc < 0) {
				pr_err("%s failed %d\n", __func__, __LINE__);
				goto ERROR5;
			}

			rc = msm_camera_init_gpio_pin_tbl(of_node, gconf,
				gpio_array, gpio_array_size);
			if (rc < 0) {
				pr_err("%s failed %d\n", __func__, __LINE__);
				goto ERROR6;
			}
		}

		/* Read the max current for an LED if present */
		if (of_get_property(of_node, "qcom,max-current", &count)) {
			count /= sizeof(uint32_t);

			if (count > MAX_LED_TRIGGERS) {
				pr_err("failed\n");
				rc = -EINVAL;
				goto ERROR8;
			}

			fctrl->flash_num_sources = count;
			fctrl->torch_num_sources = count;

			rc = of_property_read_u32_array(of_node,
				"qcom,max-current",
				fctrl->flash_max_current, count);
			if (rc < 0) {
				pr_err("%s failed %d\n", __func__, __LINE__);
				goto ERROR8;
			}

			for (; count < MAX_LED_TRIGGERS; count++)
				fctrl->flash_max_current[count] = 0;

			for (count = 0; count < MAX_LED_TRIGGERS; count++)
				fctrl->torch_max_current[count] =
					fctrl->flash_max_current[count] >> 1;
		}

		/* Read the max duration for an LED if present */
		if (of_get_property(of_node, "qcom,max-duration", &count)) {
			count /= sizeof(uint32_t);

			if (count > MAX_LED_TRIGGERS) {
				pr_err("failed\n");
				rc = -EINVAL;
				goto ERROR8;
			}

			rc = of_property_read_u32_array(of_node,
				"qcom,max-duration",
				fctrl->flash_max_duration, count);
			if (rc < 0) {
				pr_err("%s failed %d\n", __func__, __LINE__);
				goto ERROR8;
			}

			for (; count < MAX_LED_TRIGGERS; count++)
				fctrl->flash_max_duration[count] = 0;
		}

		flashdata->slave_info =
			kzalloc(sizeof(struct msm_camera_slave_info),
				GFP_KERNEL);
		if (!flashdata->slave_info) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			rc = -ENOMEM;
			goto ERROR8;
		}

		rc = of_property_read_u32_array(of_node, "qcom,slave-id",
			id_info, 3);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR9;
		}
		fctrl->flashdata->slave_info->sensor_slave_addr = id_info[0];
		fctrl->flashdata->slave_info->sensor_id_reg_addr = id_info[1];
		fctrl->flashdata->slave_info->sensor_id = id_info[2];

		kfree(gpio_array);
		return rc;
ERROR9:
		kfree(fctrl->flashdata->slave_info);
ERROR8:
		kfree(fctrl->flashdata->power_info.gpio_conf->gpio_num_info);
ERROR6:
		kfree(gconf->cam_gpio_set_tbl);
ERROR5:
		kfree(gconf->cam_gpio_req_tbl);
ERROR4:
		kfree(gconf);
ERROR1:
		kfree(fctrl->flashdata);
		kfree(gpio_array);
	}
	return rc;
}

static struct msm_camera_i2c_fn_t msm_sensor_qup_func_tbl = {
	.i2c_read = msm_camera_qup_i2c_read,
	.i2c_read_seq = msm_camera_qup_i2c_read_seq,
	.i2c_write = msm_camera_qup_i2c_write,
	.i2c_write_table = msm_camera_qup_i2c_write_table,
	.i2c_write_seq_table = msm_camera_qup_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_qup_i2c_write_table_w_microdelay,
};

static struct msm_camera_i2c_fn_t msm_sensor_cci_func_tbl = {
	.i2c_read = msm_camera_cci_i2c_read,
	.i2c_read_seq = msm_camera_cci_i2c_read_seq,
	.i2c_write = msm_camera_cci_i2c_write,
	.i2c_write_table = msm_camera_cci_i2c_write_table,
	.i2c_write_seq_table = msm_camera_cci_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_cci_i2c_write_table_w_microdelay,
	.i2c_util = msm_sensor_cci_i2c_util,
	.i2c_write_conf_tbl = msm_camera_cci_i2c_write_conf_tbl,
};

#ifdef CONFIG_DEBUG_FS
static int set_led_status(void *data, u64 val)
{
	struct msm_led_flash_ctrl_t *fctrl =
		 (struct msm_led_flash_ctrl_t *)data;
	int rc = -1;
	pr_debug("set_led_status: Enter val: %llu", val);
	if (!fctrl) {
		pr_err("set_led_status: fctrl is NULL");
		return rc;
	}
	if (!fctrl->func_tbl) {
		pr_err("set_led_status: fctrl->func_tbl is NULL");
		return rc;
	}
	if (val == 0) {
		pr_debug("set_led_status: val is disable");
		rc = msm_flash_led_off(fctrl);
	} else {
		pr_debug("set_led_status: val is enable");
		rc = msm_flash_led_low(fctrl);
	}

	return rc;
}

DEFINE_SIMPLE_ATTRIBUTE(ledflashdbg_fops,
	NULL, set_led_status, "%llu\n");
#endif

int msm_flash_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int rc = 0;
	struct msm_led_flash_ctrl_t *fctrl = NULL;
#ifdef CONFIG_DEBUG_FS
	struct dentry *dentry;
#endif

/* < DTS2014042602749 yangjiangjun 20140426 begin */
#ifdef CONFIG_HUAWEI_HW_DEV_DCT
	uint16_t tmp_data = 0;
#endif
/* DTS2014042602749 yangjiangjun 20140426 end >*/
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("i2c_check_functionality failed\n");
		goto probe_failure;
	}

	fctrl = (struct msm_led_flash_ctrl_t *)(id->driver_data);
	if (fctrl->flash_i2c_client)
		fctrl->flash_i2c_client->client = client;
	/* Set device type as I2C */
	fctrl->flash_device_type = MSM_CAMERA_I2C_DEVICE;

	/* Assign name for sub device */
	snprintf(fctrl->msm_sd.sd.name, sizeof(fctrl->msm_sd.sd.name),
		"%s", id->name);

	rc = msm_led_get_dt_data(client->dev.of_node, fctrl);
	if (rc < 0) {
		pr_err("%s failed line %d\n", __func__, __LINE__);
		return rc;
	}

	if (fctrl->pinctrl_info.use_pinctrl == true)
		msm_flash_pinctrl_init(fctrl);

	if (fctrl->flash_i2c_client != NULL) {
		fctrl->flash_i2c_client->client = client;
		if (fctrl->flashdata->slave_info->sensor_slave_addr)
			fctrl->flash_i2c_client->client->addr =
				fctrl->flashdata->slave_info->
				sensor_slave_addr;
	} else {
		pr_err("%s %s sensor_i2c_client NULL\n",
			__func__, client->name);
		rc = -EFAULT;
		return rc;
	}

	if (!fctrl->flash_i2c_client->i2c_func_tbl)
		fctrl->flash_i2c_client->i2c_func_tbl =
			&msm_sensor_qup_func_tbl;

/* < DTS2014042602749 yangjiangjun 20140426 begin */
#ifdef CONFIG_HUAWEI_HW_DEV_DCT
	/* read chip id */

	//clear the err and unlock IC, this function must be called before read and write register
	//msm_flash_clear_err_and_unlock(fctrl);

	//tmp_data = i2c_smbus_read_byte_data(client, 0x00);
	rc = fctrl->flash_i2c_client->i2c_func_tbl->i2c_read(
		fctrl->flash_i2c_client,0x00,&tmp_data, MSM_CAMERA_I2C_BYTE_DATA);
	if(rc < 0)
	{
		pr_err("%s: FLASHCHIP READ I2C error!\n", __func__);
		goto probe_failure;
	}

	/* <DTS2014122607866 zhangbo00166742 20141226 begin */
	if ( LM3642_CHIP_ID == (tmp_data & LM3642_CHIP_ID_MASK) )
	{
		pr_err("%s : Read LM3642 chip id ok!Chip ID is %d.\n", __func__, tmp_data);
		/* detect current device successful, set the flag as present */
		set_hw_dev_flag(DEV_I2C_FLASH);
		pr_err("%s : LM3642 probe succeed!\n", __func__);
	}
	else if ( LM3646_CHIP_ID == (tmp_data & LM3646_CHIP_ID_MASK) )
	{
		pr_err("%s : Read LM3646 chip id ok!Chip ID is %d.\n", __func__, tmp_data);
		/* detect current device successful, set the flag as present */
		set_hw_dev_flag(DEV_I2C_FLASH);
		pr_err("%s : LM3646 probe succeed!\n", __func__);
	}
	/*DTS2014122607866 zhangbo00166742 20141226 end >*/
	else
	{
		pr_err("%s : read chip id error!Chip ID is %d.\n", __func__, tmp_data);
		rc = -ENODEV;
		goto probe_failure;
	}
#endif
/* DTS2014042602749 yangjiangjun 20140426 end >*/

	rc = msm_led_i2c_flash_create_v4lsubdev(fctrl);

	/* < DTS2014042700594 yangjiangjun 20140427 begin */
	/*when the flashlight open background, hold power key for more than 10s*/
	/*would enter HW reset, without turn off the light. So we need to close*/
	/*light after we reboot*/
	/* <DTS2014122607866 zhangbo00166742 20141226 begin */
	if ( LM3646_CHIP_ID == (tmp_data & LM3646_CHIP_ID_MASK) )
	{
		msm_flash_lm3646_led_off(fctrl);
	}
	else
	{
		msm_flash_lm3642_led_off(fctrl);
	}
	/*DTS2014122607866 zhangbo00166742 20141226 end >*/
	/* DTS2014042700594 yangjiangjun 20140427 end > */
	/* < DTS2015071400818 yuguangcai 20150714 begin */
	memset(fctrl->flash_temperature_flag, TEMPERATUE_NORMAL, sizeof(fctrl->flash_temperature_flag));
	/* DTS2015071400818 yuguangcai 20150714 end > */

#ifdef CONFIG_DEBUG_FS
	dentry = debugfs_create_file("ledflash", S_IRUGO, NULL, (void *)fctrl,
		&ledflashdbg_fops);
	if (!dentry)
		pr_err("Failed to create the debugfs ledflash file");
#endif
	CDBG("%s:%d probe success\n", __func__, __LINE__);
	return 0;

probe_failure:
	CDBG("%s:%d probe failed\n", __func__, __LINE__);
	return rc;
}

int msm_flash_probe(struct platform_device *pdev,
	const void *data)
{
	int rc = 0;
	struct msm_led_flash_ctrl_t *fctrl =
		(struct msm_led_flash_ctrl_t *)data;
	struct device_node *of_node = pdev->dev.of_node;
	struct msm_camera_cci_client *cci_client = NULL;

	if (!of_node) {
		pr_err("of_node NULL\n");
		goto probe_failure;
	}
	fctrl->pdev = pdev;

	rc = msm_led_get_dt_data(pdev->dev.of_node, fctrl);
	if (rc < 0) {
		pr_err("%s failed line %d rc = %d\n", __func__, __LINE__, rc);
		return rc;
	}

	if (fctrl->pinctrl_info.use_pinctrl == true)
		msm_flash_pinctrl_init(fctrl);

	/* Assign name for sub device */
	snprintf(fctrl->msm_sd.sd.name, sizeof(fctrl->msm_sd.sd.name),
			"%s", fctrl->flashdata->sensor_name);
	/* Set device type as Platform*/
	fctrl->flash_device_type = MSM_CAMERA_PLATFORM_DEVICE;

	if (NULL == fctrl->flash_i2c_client) {
		pr_err("%s flash_i2c_client NULL\n",
			__func__);
		rc = -EFAULT;
		goto probe_failure;
	}

	fctrl->flash_i2c_client->cci_client = kzalloc(sizeof(
		struct msm_camera_cci_client), GFP_KERNEL);
	if (!fctrl->flash_i2c_client->cci_client) {
		pr_err("%s failed line %d kzalloc failed\n",
			__func__, __LINE__);
		return rc;
	}

	cci_client = fctrl->flash_i2c_client->cci_client;
	cci_client->cci_subdev = msm_cci_get_subdev();
	cci_client->cci_i2c_master = fctrl->cci_i2c_master;
	if (fctrl->flashdata->slave_info->sensor_slave_addr)
		cci_client->sid =
			fctrl->flashdata->slave_info->sensor_slave_addr >> 1;
	cci_client->retries = 3;
	cci_client->id_map = 0;

	if (!fctrl->flash_i2c_client->i2c_func_tbl)
		fctrl->flash_i2c_client->i2c_func_tbl =
			&msm_sensor_cci_func_tbl;

	rc = msm_led_flash_create_v4lsubdev(pdev, fctrl);

	CDBG("%s: probe success\n", __func__);
	return 0;

probe_failure:
	CDBG("%s probe failed\n", __func__);
	return rc;
}
