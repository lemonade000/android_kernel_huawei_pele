/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/qpnp/pin.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/qpnp/pwm.h>
#include <linux/err.h>

#include "mdss_dsi.h"
/* < DTS2015082807971  liyunlong 20150829 begin */
/*add boe esd  solutions*/
/*<DTS2015010902582 lijiechun/l00101002 20150109 begin */
#ifdef CONFIG_LOG_JANK
#include <linux/log_jank.h>
#endif
/*<DTS2015010902582 lijiechun/l00101002 20150109 end */
/* <DTS2014042405897 d00238048 20140425 begin */
#include <misc/app_info.h>
/* DTS2014042405897 d00238048 20140425 end> */
/* < DTS2014051503541 daiyuhong 20140515 begin */
/*< DTS2014042905347 zhaoyuxia 20140429 begin */
#include <linux/hw_lcd_common.h>
#include <hw_lcd_debug.h>
/*DTS2014101301850 zhoujian 20141013 begin */
/*DTS2015032801385 renxigang 20150328 begin */
/*delete cpuget() to avoid panic*/
/*DTS2015032801385 renxigang 20150328 end > */
/*DTS2014101301850 zhoujian 20141013 end */
/* < DTS2015061105286 liujunchao 20150618 begin */
/* < DTS2015071309027 zhaihaipeng 20150713 begin */
#ifdef CONFIG_HUAWEI_LCD
int lcd_debug_mask = LCD_INFO;
#define INVERSION_OFF 0
#define INVERSION_ON 1
module_param_named(lcd_debug_mask, lcd_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);
static bool enable_initcode_debugging = FALSE;
module_param_named(enable_initcode_debugging, enable_initcode_debugging, bool, S_IRUGO | S_IWUSR);
extern struct msmfb_cabc_config g_cabc_cfg_foresd;
extern bool enable_PT_test;
/*<DTS2015090700926 zhangming 20150917 begin>*/
/*<add cabc node>*/
/* < DTS2015121009904  feishilin 20151215 begin */
/*ref enum cabc_mode*/
enum {
	CABC_OFF,
	CABC_UI,
	CABC_STILL,
	CABC_MOVING,
};
/*  DTS2015121009904  feishilin 20151215 end >*/
/*<DTS2015090700926 zhangming 20150917 end>*/
#endif
/* DTS2015071309027 zhaihaipeng 20150713 end >*/
/* DTS2015061105286 liujunchao 20150618 end > */
/* DTS2014042905347 zhaoyuxia 20140429 end >*/
/* DTS2014051503541 daiyuhong 20140515 end > */
#define DT_CMD_HDR 6

/* NT35596 panel specific status variables */
#define NT35596_BUF_3_STATUS 0x02
#define NT35596_BUF_4_STATUS 0x40
#define NT35596_BUF_5_STATUS 0x80
#define NT35596_MAX_ERR_CNT 2

#define MIN_REFRESH_RATE 30

/* < DTS2015072108592 tianye 20150731 begin */
/* FPC unlock can't light lcd backlight */
static int LcdPow_DelayTime = false;
/* DTS2015072108592 tianye 20150731 end > */
/* < DTS2015102906691 tianye 20151109 begin */
/*open tp gesture can't wake up screen probability*/
static bool get_tp_reset_status = false;
/* DTS2015102906691 tianye 20151109 end > */

/* < DTS2015122303338  feishilin 20151229 begin */
static int old_dimming_flag=0xFF; // flag for first status
/*  DTS2015122303338  feishilin 20151229 end >*/

DEFINE_LED_TRIGGER(bl_led_trigger);

void mdss_dsi_panel_pwm_cfg(struct mdss_dsi_ctrl_pdata *ctrl)
{
	if (ctrl->pwm_pmi)
		return;

	ctrl->pwm_bl = pwm_request(ctrl->pwm_lpg_chan, "lcd-bklt");
	if (ctrl->pwm_bl == NULL || IS_ERR(ctrl->pwm_bl)) {
		pr_err("%s: Error: lpg_chan=%d pwm request failed",
				__func__, ctrl->pwm_lpg_chan);
	}
	ctrl->pwm_enabled = 0;
}

static void mdss_dsi_panel_bklt_pwm(struct mdss_dsi_ctrl_pdata *ctrl, int level)
{
	int ret;
	u32 duty;
	u32 period_ns;

	if (ctrl->pwm_bl == NULL) {
		pr_err("%s: no PWM\n", __func__);
		return;
	}

	if (level == 0) {
		if (ctrl->pwm_enabled) {
			ret = pwm_config_us(ctrl->pwm_bl, level,
					ctrl->pwm_period);
			if (ret)
				pr_err("%s: pwm_config_us() failed err=%d.\n",
						__func__, ret);
			pwm_disable(ctrl->pwm_bl);
		}
		ctrl->pwm_enabled = 0;
		return;
	}

	duty = level * ctrl->pwm_period;
	duty /= ctrl->bklt_max;

	pr_debug("%s: bklt_ctrl=%d pwm_period=%d pwm_gpio=%d pwm_lpg_chan=%d\n",
			__func__, ctrl->bklt_ctrl, ctrl->pwm_period,
				ctrl->pwm_pmic_gpio, ctrl->pwm_lpg_chan);

	pr_debug("%s: ndx=%d level=%d duty=%d\n", __func__,
					ctrl->ndx, level, duty);

	if (ctrl->pwm_period >= USEC_PER_SEC) {
		ret = pwm_config_us(ctrl->pwm_bl, duty, ctrl->pwm_period);
		if (ret) {
			pr_err("%s: pwm_config_us() failed err=%d.\n",
					__func__, ret);
			return;
		}
	} else {
		period_ns = ctrl->pwm_period * NSEC_PER_USEC;
		ret = pwm_config(ctrl->pwm_bl,
				level * period_ns / ctrl->bklt_max,
				period_ns);
		if (ret) {
			pr_err("%s: pwm_config() failed err=%d.\n",
					__func__, ret);
			return;
		}
	}

	if (!ctrl->pwm_enabled) {
		ret = pwm_enable(ctrl->pwm_bl);
		if (ret)
			pr_err("%s: pwm_enable() failed err=%d\n", __func__,
				ret);
		ctrl->pwm_enabled = 1;
	}
}

/* < DTS2014071607596 renxigang 20140717 begin */
/*set the variable from globle to local avoid Competition when esd and running test read at the same time*/
#ifndef CONFIG_HUAWEI_LCD
static char dcs_cmd[2] = {0x54, 0x00}; /* DTYPE_DCS_READ */
static struct dsi_cmd_desc dcs_read_cmd = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(dcs_cmd)},
	dcs_cmd
};
#endif
/* DTS2014071607596 renxigang 20140717 end >*/
u32 mdss_dsi_panel_cmd_read(struct mdss_dsi_ctrl_pdata *ctrl, char cmd0,
		char cmd1, void (*fxn)(int), char *rbuf, int len)
{
	struct dcs_cmd_req cmdreq;
	struct mdss_panel_info *pinfo;
/* < DTS2014071607596 renxigang 20140717 begin */
	/*delete DTS2014070102909*/
#ifdef CONFIG_HUAWEI_LCD
/* < DTS2015061105286 liujunchao 20150618 begin */
	struct dsi_panel_cmds *pcmds;
/* DTS2015061105286 liujunchao 20150618 end >*/
	char dcs_cmd[2] = {0x00, 0x00}; /* DTYPE_DCS_READ */
	struct dsi_cmd_desc dcs_read_cmd = {
	{	DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(dcs_cmd)},
		dcs_cmd
	};
#endif

	pinfo = &(ctrl->panel_data.panel_info);
	if (pinfo->dcs_cmd_by_left) {
		if (ctrl->ndx != DSI_CTRL_LEFT)
			return -EINVAL;
	}

	dcs_cmd[0] = cmd0;
	dcs_cmd[1] = cmd1;
	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &dcs_read_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT;
/* < DTS2015061105286 liujunchao 20150618 begin */
/*set hs mode flag for read cmd*/
#ifdef CONFIG_HUAWEI_LCD
	if(ctrl->esd_check_enable)
	{
		pcmds = &ctrl->esd_cmds;
		if (pcmds->link_state == DSI_HS_MODE)
			cmdreq.flags  |= CMD_REQ_HS_MODE;
	}
#endif
/* DTS2015061105286 liujunchao 20150618 end >*/
	cmdreq.rlen = len;
	cmdreq.rbuf = rbuf;
	cmdreq.cb = fxn; /* call back */
	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
	/*
	 * blocked here, until call back called
	 */

	return 0;
}
/* DTS2014071607596 renxigang 20140717 end >*/
static void mdss_dsi_panel_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_panel_cmds *pcmds)
{
	struct dcs_cmd_req cmdreq;
	struct mdss_panel_info *pinfo;

	pinfo = &(ctrl->panel_data.panel_info);
	if (pinfo->dcs_cmd_by_left) {
		if (ctrl->ndx != DSI_CTRL_LEFT)
			return;
	}

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = pcmds->cmds;
	cmdreq.cmds_cnt = pcmds->cmd_cnt;
	cmdreq.flags = CMD_REQ_COMMIT;

	/*Panel ON/Off commands should be sent in DSI Low Power Mode*/
	if (pcmds->link_state == DSI_LP_MODE)
		cmdreq.flags  |= CMD_REQ_LP_MODE;
/* < DTS2015053001908 tianye 20150530 begin */
/* TE Signal instable lead to mdp-fence timeout or blank screen and can't wake up*/
	else if (pcmds->link_state == DSI_HS_MODE)
	cmdreq.flags |= CMD_REQ_HS_MODE;
/* DTS2015053001908 tianye 20150530 end > */
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}


static char led_pwm1[2] = {0x51, 0x0};	/* DTYPE_DCS_WRITE1 */
static struct dsi_cmd_desc backlight_cmd = {
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(led_pwm1)},
	led_pwm1
};
/*  DTS2015122303338  feishilin 20151229 end >*/
/*  DTS2016032204986  zhangming 20160329 begin */
static char dimming_on_pwm1[2] = {0x53, 0x2C};	/* DTYPE_DCS_WRITE1 */
static char dimming_off_pwm1[2] = {0x53, 0x24};	/* DTYPE_DCS_WRITE1 */
static struct dsi_cmd_desc backlight_and_dimming_cmds[] = {
	{{DTYPE_DCS_WRITE1, 0, 0, 0, 1, sizeof(led_pwm1)}, led_pwm1},
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(dimming_on_pwm1)}, dimming_on_pwm1},
};

static struct dsi_cmd_desc poweroff_dimming_cmds[] = {
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(dimming_off_pwm1)}, dimming_off_pwm1},
};
/*  DTS2016032204986  zhangming 20160329 end */
/*  DTS2015122303338  feishilin 20151229 end >*/

static char unlock_part1[] = {0x00, 0x00};	/* DTYPE_GEN_LWRITE */
static char unlock_part2[] = {0x99, 0x95, 0x27};	/* DTYPE_GEN_LWRITE */
static char lock_part1[] = {0x00, 0x00};	/* DTYPE_GEN_LWRITE */
static char lock_part2[] = {0x99, 0x11, 0x11};	/* DTYPE_GEN_LWRITE */

static struct dsi_cmd_desc backlight_cmd_boe[] = {
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(unlock_part1)},unlock_part1},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(unlock_part2)},unlock_part2},
	{{DTYPE_DCS_WRITE1, 0, 0, 0, 0, sizeof(led_pwm1)},led_pwm1},
	{{DTYPE_GEN_LWRITE, 0, 0, 0, 0, sizeof(lock_part1)},lock_part1},
	{{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(lock_part2)},lock_part2},
};

/* < DTS2015070607268 zhangming 20150716 begin */
/* < DTS2015111104987 zhangming 20151111 begin*/
static char led_pwm_pad[2] = {0x9f, 0x0};
static char chang_page0_index0[2] = {0x83, 0x0};
static char chang_page0_index1[2] = {0x84, 0x0};
static struct dsi_cmd_desc backlight_cmd_pad[] = {
	{{DTYPE_DCS_WRITE1, 0, 0, 0, 1, sizeof(chang_page0_index0)},chang_page0_index0},
	{{DTYPE_DCS_WRITE1, 0, 0, 0, 1, sizeof(chang_page0_index1)},chang_page0_index1},
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(led_pwm_pad)},led_pwm_pad},
};
/*  DTS2015111104987 zhangming 20151111 end>*/
/*  DTS2015070607268 zhangming 20150716 end>*/


static void mdss_dsi_panel_bklt_dcs(struct mdss_dsi_ctrl_pdata *ctrl, int level)
{
	struct dcs_cmd_req cmdreq;
	struct mdss_panel_info *pinfo;

	pinfo = &(ctrl->panel_data.panel_info);
	if (pinfo->dcs_cmd_by_left) {
		if (ctrl->ndx != DSI_CTRL_LEFT)
			return;
	}

	pr_debug("%s: level=%d\n", __func__, level);

	led_pwm1[1] = (unsigned char)level;

	memset(&cmdreq, 0, sizeof(cmdreq));
	if (!strcmp(pinfo->panel_name,"BOE_OTM1901_5P5_1080P_VIDEO")) {
            cmdreq.cmds = backlight_cmd_boe;
	    cmdreq.cmds_cnt = sizeof(backlight_cmd_boe) / sizeof((backlight_cmd_boe)[0]);
	    cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL | CMD_REQ_HS_MODE;
	}
	else {
            cmdreq.cmds = &backlight_cmd;
	    cmdreq.cmds_cnt = 1;
	    cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	}
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
/*< DTS2014042905347 zhaoyuxia 20140429 begin */
#ifdef CONFIG_HUAWEI_LCD
	LCD_LOG_DBG("%s: level=%d\n", __func__, level);
#endif
/* DTS2014042905347 zhaoyuxia 20140429 end >*/
}


/* < DTS2015070607268 zhangming 20150716 begin */

/* <	DTS2015111104987 zhangming 20151111 begin*/

/*  DTS2016032204986  zhangming 20160329 begin */
static void mdss_dsi_power_off_dimming_pad(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct dcs_cmd_req cmdreq;
	struct mdss_panel_info *pinfo;

	pinfo = &(ctrl->panel_data.panel_info);
	if (pinfo->dcs_cmd_by_left) {
		if (ctrl->ndx != DSI_CTRL_LEFT)
			return;
	}
	memset(&cmdreq, 0, sizeof(cmdreq));

	cmdreq.cmds = poweroff_dimming_cmds;
	cmdreq.cmds_cnt = sizeof(poweroff_dimming_cmds) / sizeof((poweroff_dimming_cmds)[0]);
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL | CMD_REQ_HS_MODE;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}
/*  DTS2016032204986  zhangming 20160329 end */

static void mdss_dsi_panel_bklt_dcs_pad(struct mdss_dsi_ctrl_pdata *ctrl, int level)
{
	struct dcs_cmd_req cmdreq;
	struct mdss_panel_info *pinfo;

	pinfo = &(ctrl->panel_data.panel_info);
	if (pinfo->dcs_cmd_by_left) {
		if (ctrl->ndx != DSI_CTRL_LEFT)
			return;
	}
	pr_debug("%s: level=%d\n", __func__, level);
	memset(&cmdreq, 0, sizeof(cmdreq));
	/* < DTS2015081903221 luozhiming 20150814 begin */
	if (1 == ctrl->which_product_pad)
	{
		/*  DTS2015122303338  feishilin 20151229 begin >*/
		led_pwm1[1] = (unsigned char)level;
		cmdreq.cmds = &backlight_cmd;
		cmdreq.cmds_cnt = 1;
		cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL | CMD_REQ_HS_MODE;
		/*  DTS2016032204986  zhangming 20160329 begin */
		if(level==0)
			{
				LCD_LOG_INFO("power off~%s:dimming off\n",__func__);
				mdss_dsi_power_off_dimming_pad(ctrl);
			}
		/*  DTS2016032204986  zhangming 20160329 end */
		if( ctrl->nodimming_brightnessval){
			if(level > ctrl->nodimming_brightnessval || old_dimming_flag != 0xFF){
				if(1 != old_dimming_flag)
				{
						//do dimming
						cmdreq.cmds = backlight_and_dimming_cmds;
						cmdreq.cmds_cnt = 2;
						old_dimming_flag = 1;
						#ifdef CONFIG_HUAWEI_LCD
						LCD_LOG_INFO("%s:dimming on\n",__func__);
						#endif
				}
			}
			else{ //first power on, old dimming is off
				old_dimming_flag = 0;
			}
		}
		/*  DTS2015122303338  feishilin 20151229 end >*/
	}
	else
	{
		led_pwm_pad[1] =(unsigned char)level;
		cmdreq.cmds = backlight_cmd_pad;
		cmdreq.cmds_cnt = sizeof(backlight_cmd_pad) / sizeof((backlight_cmd_pad)[0]);
		cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL | CMD_REQ_HS_MODE;
	}
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
/*< DTS2014042905347 zhaoyuxia 20140429 begin */
#ifdef CONFIG_HUAWEI_LCD
	LCD_LOG_INFO("%s: level=%d\n", __func__, level);
#endif
	/* < DTS2015081903221 luozhiming 20150814 end */

}
/*  	DTS2015111104987 zhangming 20151111 end>*/

/*  DTS2015070607268 zhangming 20150716 end>*/

/* <DTS2015090704316 zhangming 20150921 begin*/

/* <	DTS2015111104987 zhangming 20151111 begin*/
static char cabc_moving_pad[2] = {0x95, 0xB0};
static char cabc_dimming_pad[2] = {0x90, 0x00};
static char chang_page2_index0[2] = {0x83, 0xBB};
static char chang_page2_index1[2] = {0x84, 0x22};
static struct dsi_cmd_desc moving_cmd_pad[] = {
	{{DTYPE_DCS_WRITE1, 0, 0, 0, 1, sizeof(chang_page2_index0)},chang_page2_index0},
	{{DTYPE_DCS_WRITE1, 0, 0, 0, 1, sizeof(chang_page2_index1)},chang_page2_index1},
	{{DTYPE_DCS_WRITE1, 0, 0, 0, 1, sizeof(cabc_moving_pad)},cabc_moving_pad},
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(cabc_dimming_pad)},cabc_dimming_pad},
};

/* < DTS2015122603442   zwx241012 20160128 begin */
static char pwm_freq_pad[2] = {0x94, 0x58};
static struct dsi_cmd_desc pwm_cmd_pad[] = {
	{{DTYPE_DCS_WRITE1, 0, 0, 0, 1, sizeof(chang_page2_index0)},chang_page2_index0},
	{{DTYPE_DCS_WRITE1, 0, 0, 0, 1, sizeof(chang_page2_index1)},chang_page2_index1},
	{{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(pwm_freq_pad)},pwm_freq_pad},
};


#define Pwm_freq_5K         0x4F
#define Pwm_freq_17K       0x58

static void mdss_dsi_panel_pwm_freq_pad(struct mdss_dsi_ctrl_pdata *ctrl,bool pwm_flag)
{
	struct dcs_cmd_req cmdreq;
	struct mdss_panel_info *pinfo;

	pinfo = &(ctrl->panel_data.panel_info);
	if (pinfo->dcs_cmd_by_left) {
		if (ctrl->ndx != DSI_CTRL_LEFT)
			return;
	}
	if(pwm_flag)
	pwm_freq_pad[1] = Pwm_freq_5K;
	else
	pwm_freq_pad[1] = Pwm_freq_17K;
	memset(&cmdreq, 0, sizeof(cmdreq));

	cmdreq.cmds = pwm_cmd_pad;
	cmdreq.cmds_cnt = sizeof(pwm_cmd_pad) / sizeof((pwm_cmd_pad)[0]);
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL | CMD_REQ_HS_MODE;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}
/* < DTS2015122603442   zwx241012 20160128 end */
static void mdss_dsi_panel_cabc_moving_pad(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct dcs_cmd_req cmdreq;
	struct mdss_panel_info *pinfo;

	pinfo = &(ctrl->panel_data.panel_info);
	if (pinfo->dcs_cmd_by_left) {
		if (ctrl->ndx != DSI_CTRL_LEFT)
			return;
	}

	memset(&cmdreq, 0, sizeof(cmdreq));

	cmdreq.cmds = moving_cmd_pad;
	cmdreq.cmds_cnt = sizeof(moving_cmd_pad) / sizeof((moving_cmd_pad)[0]);
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL | CMD_REQ_HS_MODE;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}
/*  	DTS2015111104987 zhangming 20151111 end>*/
/*  DTS2015090704316 zhangming 20150921 end>*/


/* < DTS2014053009543 zhaoyuxia 20140604 begin */
/* revert huawei modify */
static int mdss_dsi_request_gpios(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int rc = 0;

	if (gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
		rc = gpio_request(ctrl_pdata->disp_en_gpio,
						"disp_enable");
		if (rc) {
			pr_err("request disp_en gpio failed, rc=%d\n",
				       rc);
			goto disp_en_gpio_err;
		}
	}
	rc = gpio_request(ctrl_pdata->rst_gpio, "disp_rst_n");
	if (rc) {
		pr_err("request reset gpio failed, rc=%d\n",
			rc);
		goto rst_gpio_err;
	}
	if (gpio_is_valid(ctrl_pdata->bklt_en_gpio)) {
		rc = gpio_request(ctrl_pdata->bklt_en_gpio,
						"bklt_enable");
		if (rc) {
			pr_err("request bklt gpio failed, rc=%d\n",
				       rc);
			goto bklt_en_gpio_err;
		}
	}
	if (gpio_is_valid(ctrl_pdata->mode_gpio)) {
		rc = gpio_request(ctrl_pdata->mode_gpio, "panel_mode");
		if (rc) {
			pr_err("request panel mode gpio failed,rc=%d\n",
								rc);
			goto mode_gpio_err;
		}
	}
	return rc;

mode_gpio_err:
	if (gpio_is_valid(ctrl_pdata->bklt_en_gpio))
		gpio_free(ctrl_pdata->bklt_en_gpio);
bklt_en_gpio_err:
	gpio_free(ctrl_pdata->rst_gpio);
rst_gpio_err:
	if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
		gpio_free(ctrl_pdata->disp_en_gpio);
disp_en_gpio_err:
	return rc;
}
/* < DTS2015070607268 zhangming 20150716 begin */
/* < DTS2015080308353 zwx241012 20150814 begin */
/*Rename GPIO*/
#ifdef CONFIG_HUAWEI_LCD
static void hw_panel_bias_en(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	if (gpio_is_valid(ctrl_pdata->disp_en_gpio_vled))
	{
		gpio_set_value((ctrl_pdata->disp_en_gpio_vled), enable);
		pr_info("%s,%d set en disp_en_gpio_vled = %d \n",__func__,__LINE__,enable);
	}
	/* < DTS2015080308353 zwx241012 20150814 begin */
	if(enable){
		ctrl_pdata->hw_led_en_flag = 1;
	}else {
		ctrl_pdata->hw_led_en_flag = 0;
	}
	/* DTS2015080308353 zwx241012 20150814 end > */
}
#endif
/* DTS2015080308353 zwx241012 20150814 end > */
/*  DTS2015070607268 zhangming 20150716 end>*/
/* < DTS2014052100201 zhaoyuxia 20140526 begin */
/* optimize code */

int mdss_dsi_panel_reset(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;
	int i, rc = 0;
	/*DTS2014101301850 zhoujian 20141013 begin */
	unsigned long timeout = jiffies;
	/*DTS2014101301850 zhoujian 20141013 end */
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	if (!gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
		pr_debug("%s:%d, reset line not configured\n",
			   __func__, __LINE__);
	}

	if (!gpio_is_valid(ctrl_pdata->rst_gpio)) {
		pr_debug("%s:%d, reset line not configured\n",
			   __func__, __LINE__);
		return rc;
	}

	pr_debug("%s: enable = %d\n", __func__, enable);
	pinfo = &(ctrl_pdata->panel_data.panel_info);

	if (enable) {
		rc = mdss_dsi_request_gpios(ctrl_pdata);
		if (rc) {
			pr_err("gpio request failed\n");
			return rc;
		}
	#ifdef CONFIG_HUAWEI_LCD
		if (!pinfo->cont_splash_enabled) {
		/* < DTS2015012603845 zhaoyuxia 20150126 begin */
		LCD_MDELAY(10);
		/* DTS2015012603845 zhaoyuxia 20150126 end > */
		for (i = 0; i < pdata->panel_info.rst_seq_len; ++i) {
			gpio_set_value((ctrl_pdata->rst_gpio),
                               pdata->panel_info.rst_seq[i]);
			if (pdata->panel_info.rst_seq[++i])
                               usleep(pinfo->rst_seq[i] * 1000);
			}
		}
		/* < DTS2015070607268 zhangming 20150716 begin */
		/* < DTS2015080308353 zwx241012 20150814 begin */
		//delete
		/* DTS2015080308353 zwx241012 20150814 end > */
		/*  DTS2015070607268 zhangming 20150716 end>*/
	#else
		if (!pinfo->cont_splash_enabled) {
			if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
				gpio_set_value((ctrl_pdata->disp_en_gpio), 1);

			for (i = 0; i < pdata->panel_info.rst_seq_len; ++i) {
				gpio_set_value((ctrl_pdata->rst_gpio),
					pdata->panel_info.rst_seq[i]);
				if (pdata->panel_info.rst_seq[++i])
					usleep(pinfo->rst_seq[i] * 1000);
			}

			if (gpio_is_valid(ctrl_pdata->bklt_en_gpio))
				gpio_set_value((ctrl_pdata->bklt_en_gpio), 1);
		}
	#endif
		if (gpio_is_valid(ctrl_pdata->mode_gpio)) {
			if (pinfo->mode_gpio_state == MODE_GPIO_HIGH)
				gpio_set_value((ctrl_pdata->mode_gpio), 1);
			else if (pinfo->mode_gpio_state == MODE_GPIO_LOW)
				gpio_set_value((ctrl_pdata->mode_gpio), 0);
		}
		if (ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT) {
			pr_debug("%s: Panel Not properly turned OFF\n",
						__func__);
			ctrl_pdata->ctrl_state &= ~CTRL_STATE_PANEL_INIT;
			pr_debug("%s: Reset panel done\n", __func__);
		}
	} else {
/*< DTS2014103106441 huangli/hwx207935 20141031 begin*/
	#ifdef CONFIG_HUAWEI_LCD
		if(pinfo->mipi_rest_delay){
			LCD_LOG_INFO("%s:delay is %d\n",__func__,pinfo->mipi_rest_delay);
			LCD_MDELAY(pinfo->mipi_rest_delay);

			}
	#endif
/*DTS2014103106441 huangli/hwx207935 20141031 end >*/
/* < DTS2015071309027 zhaihaipeng 20150713 begin */
		if (gpio_is_valid(ctrl_pdata->bklt_en_gpio)) {
			gpio_set_value((ctrl_pdata->bklt_en_gpio), 0);
			gpio_free(ctrl_pdata->bklt_en_gpio);
		}
		if (gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
			gpio_set_value((ctrl_pdata->disp_en_gpio), 0);
			gpio_free(ctrl_pdata->disp_en_gpio);
		}
		/* < DTS2015080308353 zwx241012 20150814 begin */
		//delete
		/* DTS2015080308353 zwx241012 20150814 end > */

		gpio_set_value((ctrl_pdata->rst_gpio), 0);
		//Pull the reset to low-high-low only when truly_hx8399a module's leakage current be tested in PT.
		if(enable_PT_test && ctrl_pdata->reset_for_pt_flag)
		{
			usleep( 5*1000);
			gpio_set_value((ctrl_pdata->rst_gpio), 1);
			usleep( 20*1000);
			gpio_set_value((ctrl_pdata->rst_gpio), 0);
			LCD_LOG_INFO("%s: panel pull reset low-high-low when suspend.\n",__func__);
		}
/* DTS2015071309027 zhaihaipeng 20150713 end >*/
			gpio_free(ctrl_pdata->rst_gpio);
		if (gpio_is_valid(ctrl_pdata->mode_gpio))
			gpio_free(ctrl_pdata->mode_gpio);
	#ifdef CONFIG_HUAWEI_LCD
		LCD_MDELAY(10);
	#endif

	}
	/* DTS2014101301850 zhoujian 20141013 begin > */
	/* add for timeout print log */
	/*DTS2015032801385 renxigang 20150328 begin */
	/*delete cpuget() to avoid panic*/
	LCD_LOG_INFO("%s: panel reset time = %u\n",
		__func__,jiffies_to_msecs(jiffies-timeout));
	/*DTS2015032801385 renxigang 20150328 end > */
	/* DTS2014101301850 zhoujian 20141013 end > */
	return rc;
}
/* DTS2014053009543 zhaoyuxia 20140604 end > */
/* DTS2014042500432 zhaoyuxia 20140225 end > */
/* <DTS2014042409252 d00238048 20140505 begin */
#ifdef CONFIG_FB_AUTO_CABC
static int mdss_dsi_panel_cabc_ctrl(struct mdss_panel_data *pdata,struct msmfb_cabc_config cabc_cfg)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	if (pdata == NULL) {
		LCD_LOG_ERR("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	switch(cabc_cfg.mode)
	{
		case CABC_MODE_UI:
			if (ctrl_pdata->dsi_panel_cabc_ui_cmds.cmd_cnt)
				mdss_dsi_panel_cmds_send(ctrl_pdata, &ctrl_pdata->dsi_panel_cabc_ui_cmds);
			break;
		case CABC_MODE_MOVING:
		case CABC_MODE_STILL:
			if (ctrl_pdata->dsi_panel_cabc_video_cmds.cmd_cnt)
				mdss_dsi_panel_cmds_send(ctrl_pdata, &ctrl_pdata->dsi_panel_cabc_video_cmds);
			break;
		default:
			LCD_LOG_ERR("%s: invalid cabc mode: %d\n", __func__, cabc_cfg.mode);
			break;
	}
	LCD_LOG_INFO("exit %s : CABC mode= %d\n",__func__,cabc_cfg.mode);
	return 0;
}
#endif
/* DTS2014042409252 d00238048 20140505 end> */
/**
 * mdss_dsi_roi_merge() -  merge two roi into single roi
 *
 * Function used by partial update with only one dsi intf take 2A/2B
 * (column/page) dcs commands.
 */
static int mdss_dsi_roi_merge(struct mdss_dsi_ctrl_pdata *ctrl,
					struct mdss_rect *roi)
{
	struct mdss_panel_info *l_pinfo;
	struct mdss_rect *l_roi;
	struct mdss_rect *r_roi;
	struct mdss_dsi_ctrl_pdata *other = NULL;
	int ans = 0;

	if (ctrl->ndx == DSI_CTRL_LEFT) {
		other = mdss_dsi_get_ctrl_by_index(DSI_CTRL_RIGHT);
		if (!other)
			return ans;
		l_pinfo = &(ctrl->panel_data.panel_info);
		l_roi = &(ctrl->panel_data.panel_info.roi);
		r_roi = &(other->panel_data.panel_info.roi);
	} else  {
		other = mdss_dsi_get_ctrl_by_index(DSI_CTRL_LEFT);
		if (!other)
			return ans;
		l_pinfo = &(other->panel_data.panel_info);
		l_roi = &(other->panel_data.panel_info.roi);
		r_roi = &(ctrl->panel_data.panel_info.roi);
	}

	if (l_roi->w == 0 && l_roi->h == 0) {
		/* right only */
		*roi = *r_roi;
		roi->x += l_pinfo->xres;/* add left full width to x-offset */
	} else {
		/* left only and left+righ */
		*roi = *l_roi;
		roi->w +=  r_roi->w; /* add right width */
		ans = 1;
	}

	return ans;
}

static char caset[] = {0x2a, 0x00, 0x00, 0x03, 0x00};	/* DTYPE_DCS_LWRITE */
static char paset[] = {0x2b, 0x00, 0x00, 0x05, 0x00};	/* DTYPE_DCS_LWRITE */

/* pack into one frame before sent */
static struct dsi_cmd_desc set_col_page_addr_cmd[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 1, sizeof(caset)}, caset},	/* packed */
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(paset)}, paset},
};

static void mdss_dsi_send_col_page_addr(struct mdss_dsi_ctrl_pdata *ctrl,
					struct mdss_rect *roi)
{
	struct dcs_cmd_req cmdreq;

	roi = &ctrl->roi;

	caset[1] = (((roi->x) & 0xFF00) >> 8);
	caset[2] = (((roi->x) & 0xFF));
	caset[3] = (((roi->x - 1 + roi->w) & 0xFF00) >> 8);
	caset[4] = (((roi->x - 1 + roi->w) & 0xFF));
	set_col_page_addr_cmd[0].payload = caset;

	paset[1] = (((roi->y) & 0xFF00) >> 8);
	paset[2] = (((roi->y) & 0xFF));
	paset[3] = (((roi->y - 1 + roi->h) & 0xFF00) >> 8);
	paset[4] = (((roi->y - 1 + roi->h) & 0xFF));
	set_col_page_addr_cmd[1].payload = paset;

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds_cnt = 2;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL | CMD_REQ_UNICAST;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	cmdreq.cmds = set_col_page_addr_cmd;
	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}

static int mdss_dsi_set_col_page_addr(struct mdss_panel_data *pdata)
{
	struct mdss_panel_info *pinfo;
	struct mdss_rect roi;
	struct mdss_rect *p_roi;
	struct mdss_rect *c_roi;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mdss_dsi_ctrl_pdata *other = NULL;
	int left_or_both = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pinfo = &pdata->panel_info;
	p_roi = &pinfo->roi;

	/*
	 * to avoid keep sending same col_page info to panel,
	 * if roi_merge enabled, the roi of left ctrl is used
	 * to compare against new merged roi and saved new
	 * merged roi to it after comparing.
	 * if roi_merge disabled, then the calling ctrl's roi
	 * and pinfo's roi are used to compare.
	 */
	if (pinfo->partial_update_roi_merge) {
		left_or_both = mdss_dsi_roi_merge(ctrl, &roi);
		other = mdss_dsi_get_ctrl_by_index(DSI_CTRL_LEFT);
		c_roi = &other->roi;
	} else {
		c_roi = &ctrl->roi;
		roi = *p_roi;
	}

	/* roi had changed, do col_page update */
	if (!mdss_rect_cmp(c_roi, &roi)) {
		pr_debug("%s: ndx=%d x=%d y=%d w=%d h=%d\n",
				__func__, ctrl->ndx, p_roi->x,
				p_roi->y, p_roi->w, p_roi->h);

		*c_roi = roi; /* keep to ctrl */
		if (c_roi->w == 0 || c_roi->h == 0) {
			/* no new frame update */
			pr_debug("%s: ctrl=%d, no partial roi set\n",
						__func__, ctrl->ndx);
			return 0;
		}

		if (pinfo->dcs_cmd_by_left) {
			if (left_or_both && ctrl->ndx == DSI_CTRL_RIGHT) {
				/* 2A/2B sent by left already */
				return 0;
			}
		}

		if (!mdss_dsi_sync_wait_enable(ctrl)) {
			if (pinfo->dcs_cmd_by_left)
				ctrl = mdss_dsi_get_ctrl_by_index(
							DSI_CTRL_LEFT);
			mdss_dsi_send_col_page_addr(ctrl, &roi);
		} else {
			/*
			 * when sync_wait_broadcast enabled,
			 * need trigger at right ctrl to
			 * start both dcs cmd transmission
			 */
			other = mdss_dsi_get_other_ctrl(ctrl);
			if (!other)
				goto end;

			if (mdss_dsi_is_left_ctrl(ctrl)) {
				mdss_dsi_send_col_page_addr(ctrl, &ctrl->roi);
				mdss_dsi_send_col_page_addr(other, &other->roi);
			} else {
				mdss_dsi_send_col_page_addr(other, &other->roi);
				mdss_dsi_send_col_page_addr(ctrl, &ctrl->roi);
			}
		}
	}

end:
	return 0;
}

static void mdss_dsi_panel_switch_mode(struct mdss_panel_data *pdata,
							int mode)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mipi_panel_info *mipi;
	struct dsi_panel_cmds *pcmds;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	mipi  = &pdata->panel_info.mipi;

	if (!mipi->dynamic_switch_enabled)
		return;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	if (mode == DSI_CMD_MODE)
		pcmds = &ctrl_pdata->video2cmd;
	else
		pcmds = &ctrl_pdata->cmd2video;

	mdss_dsi_panel_cmds_send(ctrl_pdata, pcmds);

	return;
}

/* < DTS2015122603442   zwx241012 20160128 begin */
//avoid the possible range of backlight flicker
#define Speicial_Point_1  23
#define Speicial_Point_2  30
/* < DTS2015122603442   zwx241012 20160128 end */
static void mdss_dsi_panel_bl_ctrl(struct mdss_panel_data *pdata,
							u32 bl_level)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_dsi_ctrl_pdata *sctrl = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	/* < DTS2015122603442   zwx241012 20160128 begin */
	if ((bl_level < pdata->panel_info.bl_min) && (bl_level != 0))
		bl_level = pdata->panel_info.bl_min;
	/* < DTS2015122603442   zwx241012 20160128 end */
	/* < DTS2015080308353 zwx241012 20150814 begin */
	if (1==ctrl_pdata->hw_product_pad && 0 != bl_level && 1 != ctrl_pdata->which_product_pad) {
		bl_level = bl_level + 7;
		/* < DTS2015081903221 luozhiming 20150814 begin */
		if (0 == ctrl_pdata->hw_led_en_flag){
		/* <DTS2015090704316 zhangming 20150921 begin*/
			if (!strcmp(ctrl_pdata->panel_data.panel_info.panel_name,"BOE_NT51021_10_1200P_VIDEO")){
				mdelay(20);
			}
			hw_panel_bias_en(pdata,1);
		/*  DTS2015090704316 zhangming 20150921 end>*/
		}
		else{
			if(1==ctrl_pdata->hw_led_en_flag){
					mdss_dsi_panel_cabc_moving_pad(ctrl_pdata);
					ctrl_pdata->hw_led_en_flag=2;
			}
		}
		/* < DTS2015081903221 luozhiming 20150814 end */
	}
	/* DTS2015080308353 zwx241012 20150814 end > */
	/*
	 * Some backlight controllers specify a minimum duty cycle
	 * for the backlight brightness. If the brightness is less
	 * than it, the controller can malfunction.
	 */

	if ((bl_level > pdata->panel_info.bl_max) && (bl_level != 0))
		bl_level = pdata->panel_info.bl_max;

	/* < DTS2015122603442   zwx241012 20160128 begin */
	if (1==ctrl_pdata->hw_product_pad)
	{
		if((!strcmp(ctrl_pdata->panel_data.panel_info.panel_name,"BOE_NT51021_10_1200P_VIDEO"))||
			(!strcmp(ctrl_pdata->panel_data.panel_info.panel_name,"CMI_NT51021_10_1200P_VIDEO")))
		{
			if(bl_level>=Speicial_Point_1&&bl_level<=Speicial_Point_2)
				bl_level=Speicial_Point_2;

			if(bl_level<Speicial_Point_1)
			{

				mdss_dsi_panel_pwm_freq_pad(ctrl_pdata,1);
				LCD_LOG_INFO("enter 5K frequency %s,level=%d\n",__func__,bl_level);

			}
			else
			{

				mdss_dsi_panel_pwm_freq_pad(ctrl_pdata,0);
				LCD_LOG_INFO("enter 17K frequency %s,level=%d\n",__func__,bl_level);

			}
		}
	}
	/* < DTS2015122603442   zwx241012 20160128 end */
	switch (ctrl_pdata->bklt_ctrl) {
	case BL_WLED:
		led_trigger_event(bl_led_trigger, bl_level);
		break;
	case BL_PWM:
		mdss_dsi_panel_bklt_pwm(ctrl_pdata, bl_level);
		break;
	case BL_DCS_CMD:
/* < DTS2015070607268 zhangming 20150716 begin */
		if (!mdss_dsi_sync_wait_enable(ctrl_pdata)) {
			if (ctrl_pdata->hw_product_pad)
			mdss_dsi_panel_bklt_dcs_pad(ctrl_pdata, bl_level);
			else
			mdss_dsi_panel_bklt_dcs(ctrl_pdata, bl_level);
			break;
		}
		/*
		 * DCS commands to update backlight are usually sent at
		 * the same time to both the controllers. However, if
		 * sync_wait is enabled, we need to ensure that the
		 * dcs commands are first sent to the non-trigger
		 * controller so that when the commands are triggered,
		 * both controllers receive it at the same time.
		 */
		sctrl = mdss_dsi_get_other_ctrl(ctrl_pdata);
		if (mdss_dsi_sync_wait_trigger(ctrl_pdata)) {
			if (sctrl)
				{
				if (ctrl_pdata->hw_product_pad)
				mdss_dsi_panel_bklt_dcs_pad(sctrl, bl_level);
				else
				mdss_dsi_panel_bklt_dcs(sctrl, bl_level);
				}
			if (ctrl_pdata->hw_product_pad)
			mdss_dsi_panel_bklt_dcs_pad(ctrl_pdata, bl_level);
			else
			mdss_dsi_panel_bklt_dcs(ctrl_pdata, bl_level);
		} else {
			if (ctrl_pdata->hw_product_pad)
			mdss_dsi_panel_bklt_dcs_pad(ctrl_pdata, bl_level);
			else
			mdss_dsi_panel_bklt_dcs(ctrl_pdata, bl_level);
			if (sctrl)
				{
				if (ctrl_pdata->hw_product_pad)
				mdss_dsi_panel_bklt_dcs_pad(sctrl, bl_level);
				else
				mdss_dsi_panel_bklt_dcs(sctrl, bl_level);
				}
		}
/*  DTS2015070607268 zhangming 20150716 end>*/
		break;
	default:
		pr_err("%s: Unknown bl_ctrl configuration\n",
			__func__);
		break;
	}
}

/*< DTS2014042905347 zhaoyuxia 20140429 begin */
static int mdss_dsi_panel_on(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mdss_panel_info *pinfo;
#ifdef CONFIG_HUAWEI_LCD
	struct dsi_panel_cmds cmds;
#endif
/*DTS2014101301850 zhoujian 20141013 begin */
	unsigned long timeout = jiffies;
/*DTS2014101301850 zhoujian 20141013 end */
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	pinfo = &pdata->panel_info;
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_debug("%s: ctrl=%p ndx=%d\n", __func__, ctrl, ctrl->ndx);

	if (pinfo->dcs_cmd_by_left) {
		if (ctrl->ndx != DSI_CTRL_LEFT)
			goto end;
	}

#ifndef CONFIG_HUAWEI_LCD
	if (ctrl->on_cmds.cmd_cnt)
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->on_cmds);
#else
	memset(&cmds, sizeof(cmds), 0);
	/* < DTS2014051503541 daiyuhong 20140515 begin */
	/* if inversion mode has been setted open, we open inversion function after reset*/
	if((INVERSION_ON == ctrl->inversion_state) && ctrl ->dsi_panel_inverse_on_cmds.cmd_cnt )
	{
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->dsi_panel_inverse_on_cmds);
		LCD_LOG_DBG("%s:display inversion open:inversion_state = [%d]\n",__func__,ctrl->inversion_state);
	}
	/* DTS2014051503541 daiyuhong 20140515 end > */
	if (enable_initcode_debugging && !hw_parse_dsi_cmds(&cmds))
	{
		LCD_LOG_INFO("read from debug file and write to LCD!\n");
		cmds.link_state = ctrl->on_cmds.link_state;
		if (cmds.cmd_cnt)
			mdss_dsi_panel_cmds_send(ctrl, &cmds);
		hw_free_dsi_cmds(&cmds);
	}
	else
	{
		if (ctrl->on_cmds.cmd_cnt)
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->on_cmds);
	}
/* < DTS2014080106240 renxigang 20140801 begin */
#ifdef CONFIG_HUAWEI_LCD
	lcd_pwr_status.lcd_dcm_pwr_status |= BIT(1);
	do_gettimeofday(&lcd_pwr_status.tvl_lcd_on);
	time_to_tm(lcd_pwr_status.tvl_lcd_on.tv_sec, 0, &lcd_pwr_status.tm_lcd_on);
#endif
/* DTS2014080106240 renxigang 20140801 end > */

	LCD_LOG_INFO("exit %s\n",__func__);
#endif
/* < DTS2014050808504 daiyuhong 20140508 begin */
/*set mipi status*/
#if defined(CONFIG_HUAWEI_KERNEL) && defined(CONFIG_DEBUG_FS)
	atomic_set(&mipi_path_status,MIPI_PATH_OPEN);
#endif
/* DTS2014050808504 daiyuhong 20140508 end > */
end:
	pinfo->blank_state = MDSS_PANEL_BLANK_UNBLANK;
	pr_debug("%s:-\n", __func__);
/*DTS2014101301850 zhoujian 20141013 begin > */
/* add for timeout print log */
/*DTS2015032801385 renxigang 20150328 begin */
/*delete cpuget() to avoid panic*/
	LCD_LOG_INFO("%s: panel_on_time = %u\n",
			__func__,jiffies_to_msecs(jiffies-timeout));
/*DTS2015032801385 renxigang 20150328 end > */
/*DTS2014101301850 zhoujian 20141013 end > */
/*<DTS2015010902582 lijiechun/l00101002 20150109 begin */
#ifdef CONFIG_LOG_JANK
    LOG_JANK_D(JLID_KERNEL_LCD_POWER_ON, "%s", "JL_KERNEL_LCD_POWER_ON");
#endif
/*<DTS2015010902582 lijiechun/l00101002 20150109 end */
	return 0;
}

static int mdss_dsi_panel_off(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mdss_panel_info *pinfo;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
/* < DTS2014050808504 daiyuhong 20140508 begin */
/*set mipi status*/
#if defined(CONFIG_HUAWEI_KERNEL) && defined(CONFIG_DEBUG_FS)
	atomic_set(&mipi_path_status,MIPI_PATH_CLOSE);
#endif
/* DTS2014050808504 daiyuhong 20140508 end > */

	pinfo = &pdata->panel_info;
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_debug("%s: ctrl=%p ndx=%d\n", __func__, ctrl, ctrl->ndx);

	if (pinfo->dcs_cmd_by_left) {
		if (ctrl->ndx != DSI_CTRL_LEFT)
			goto end;
	}

	if (ctrl->off_cmds.cmd_cnt)
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->off_cmds);

/* < DTS2015122303338  feishilin 20151229 begin */
	old_dimming_flag = 0xFF; // flag for first status
/*  DTS2015122303338  feishilin 20151229 end >*/

end:
	pinfo->blank_state = MDSS_PANEL_BLANK_BLANK;
	pr_debug("%s:-\n", __func__);
/*<DTS2015010902582 lijiechun/l00101002 20150109 begin */
#ifdef CONFIG_LOG_JANK
    LOG_JANK_D(JLID_KERNEL_LCD_POWER_OFF, "%s", "JL_KERNEL_LCD_POWER_OFF");
#endif
/*<DTS2015010902582 lijiechun/l00101002 20150109 end */
	return 0;
}

static int mdss_dsi_panel_low_power_config(struct mdss_panel_data *pdata,
	int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mdss_panel_info *pinfo;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	pinfo = &pdata->panel_info;
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_debug("%s: ctrl=%p ndx=%d enable=%d\n", __func__, ctrl, ctrl->ndx,
		enable);

	/* Any panel specific low power commands/config */
	if (enable)
		pinfo->blank_state = MDSS_PANEL_BLANK_LOW_POWER;
	else
		pinfo->blank_state = MDSS_PANEL_BLANK_UNBLANK;

	pr_debug("%s:-\n", __func__);
	return 0;
}
#ifdef CONFIG_HUAWEI_LCD
static int mdss_dsi_panel_inversion_ctrl(struct mdss_panel_data *pdata,u32 imode)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	int ret = 0;
	if (pdata == NULL) {
		LCD_LOG_ERR("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	
	switch(imode)
	{
		case COLUMN_INVERSION: //column inversion mode
			if (ctrl_pdata->column_inversion_cmds.cmd_cnt)
				mdss_dsi_panel_cmds_send(ctrl_pdata ,&ctrl_pdata->column_inversion_cmds);
			break;
		case DOT_INVERSION:// dot inversion mode
			if (ctrl_pdata->dot_inversion_cmds.cmd_cnt)
				mdss_dsi_panel_cmds_send(ctrl_pdata ,&ctrl_pdata->dot_inversion_cmds);
			break;
		default:
			ret = -EINVAL;
			LCD_LOG_ERR("%s: invalid inversion mode: %d\n", __func__, imode);
			break;
	}
	LCD_LOG_INFO("exit %s ,dot inversion enable= %d \n",__func__,imode);
	return ret;

}
/* <DTS2014051303765 daiyuhong 20140513 begin */
/*Add status reg check function of tianma ili9806c LCD*/
/* <DTS2014050904959 daiyuhong 20140509 begin */
/* skip read reg when check panel status*/
/* < DTS2014111001776 zhaoyuxia 20141114 begin */
static int mdss_dsi_check_panel_status(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	int ret = 0;
	int count = 5;
	/* < DTS2014060603272 zhengjinzeng 20140606 begin */  
	/* because the max read length is 3, so change returned value buffer size to char*3 */  
	char rdata[3] = {0}; 
	char expect_value = 0x9C;
	int read_length = 1;

	if (pdata == NULL) {
		LCD_LOG_ERR("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	if(ctrl_pdata->skip_reg_read)
	{
		LCD_LOG_INFO("exit %s ,skip_reg_read=%d, panel status is ok\n",__func__,ctrl_pdata->skip_reg_read);
		return ret;
	}
	else
	{
		if (ctrl_pdata->long_read_flag)
		{
			expect_value = ctrl_pdata->reg_expect_value;
			read_length = ctrl_pdata->reg_expect_count;
		}

		do{
			mdss_dsi_panel_cmd_read(ctrl_pdata,0x0A,0x00,NULL,rdata,read_length);
			LCD_LOG_INFO("exit %s ,0x0A = 0x%x \n",__func__,rdata[0]);
			count--;
		}while((expect_value != rdata[0])&&(count>0));

		/* < DTS2014051603610 zhaoyuxia 20140516 begin */
		if(0 == count)
		{
			ret = -EINVAL;
			lcd_report_dsm_err(DSM_LCD_STATUS_ERROR_NO, rdata[0], 0x0A);
		}
		/* DTS2014051603610 zhaoyuxia 20140516 end > */
		/* DTS2014060603272 zhengjinzeng 20140606 end >*/ 
	
		return ret;
	}
}
/* DTS2014050904959 daiyuhong 20140509 end > */
/*< DTS2015052207491 guoyongxiang 20150522 begin */
int mdss_dsi_check_panel_status_n(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	int ret = 0;
	char rdata[3] = {0}; 
	char expect_value = 0x9C;
	int read_length = 1;

	if (pdata == NULL) {
		LCD_LOG_ERR("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	if(ctrl_pdata->skip_reg_read)
	{
		LCD_LOG_INFO("exit %s ,skip_reg_read=%d, panel status is ok\n",__func__,ctrl_pdata->skip_reg_read);
		return ret;
	}
	else
	{
		if (ctrl_pdata->long_read_flag)
		{
			expect_value = ctrl_pdata->reg_expect_value;
			read_length = ctrl_pdata->reg_expect_count;
		}
		
		mdss_dsi_panel_cmd_read(ctrl_pdata,0x0A,0x00,NULL,rdata,read_length);
		LCD_LOG_INFO("exit %s ,0x0A = 0x%x \n",__func__,rdata[0]);
		
		if((expect_value != rdata[0]))
		{
			ret = -EINVAL;
			lcd_report_dsm_err(DSM_LCD_STATUS_ERROR_NO, rdata[0], 0x0A);
		}
	
		return ret;
	}
}
/* DTS2015052207491 guoyongxiang 20150522 end >*/
/*< DTS2014052207729 renxigang 20140520 begin */
/*if Panel IC works well, return 1, else return -1 */
int panel_check_live_status(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int count = 0;
	int j = 0;
	int i = 0;
	/* success on retrun 1,otherwise return 0 or -1 */
	int ret = 1;
	int compare_reg = 0;
	/* < DTS2015043000365 tianye 20150430 begin */
	/*add ESD to check 0A reg status*/
	/* < DTS2015050403227 tianye 20150505 begin */
	/* open esd check function,avoid esd running test fail*/
	/* DTS2015050403227 tianye 20150505 end > */
	/* DTS2015043000365 tianye 20150430 end > */
	#define MAX_RETURN_COUNT_ESD 4
	#define REPEAT_COUNT 5
	char esd_buf[MAX_RETURN_COUNT_ESD]={0};
/* < DTS2015051208660 tianye 20150512 begin */
/* avoid esd and checksum running test error */
	if(mutex_lock_interruptible(&ctrl->panel_data.LCD_checksum_lock))
	{
		pr_err("exit %s,esd get lock fail\n",__func__);
		return ret;
	}
/* DTS2015051208660 tianye 20150512 end > */
	/*< DTS2015061105286 liujunchao 20150618 begin */
	if(ctrl->esd_set_cabc_flag)
	{
		switch(g_cabc_cfg_foresd.mode)
		{
			case CABC_MODE_MOVING:
			case CABC_MODE_STILL:
				if (ctrl->dsi_panel_cabc_video_cmds.cmd_cnt)
				{
					mdss_dsi_panel_cmds_send(ctrl, &ctrl->dsi_panel_cabc_video_cmds);
				}
				break;
			default:
				if(ctrl->dsi_panel_cabc_ui_cmds.cmd_cnt)
				{
					mdss_dsi_panel_cmds_send(ctrl, &ctrl->dsi_panel_cabc_ui_cmds);
				}
				break;
		}
	}
	/* DTS2015061105286 liujunchao 20150618 end >*/
	for(j = 0;j < ctrl->esd_cmds.cmd_cnt;j++)
	{
		count = 0;
		do
		{
			compare_reg = 0;
			mdss_dsi_panel_cmd_read(ctrl,ctrl->esd_cmds.cmds[j].payload[0],0x00,NULL,esd_buf,ctrl->esd_cmds.cmds[j].dchdr.dlen - 1);
			count++;
			for(i=0;i<(ctrl->esd_cmds.cmds[j].dchdr.dlen-1);i++)
			{
				if(ctrl->esd_cmds.cmds[j].payload[0] == 0x0d && ctrl->esd_cmds.cmds[j].dchdr.dlen == 2)
				{
					/*if ctrl->inversion_state is 1,ctrl->inversion_state<<5 equal 0x20*/
					/*if ctrl->inversion_state is 0,ctrl->inversion_state<<5 equal 0x00*/
					/*ctrl->esd_cmds.cmds[j].payload[1] equal 0x00*/
					/*when we read the 0x0d register,0x20 means enter color inversion mode when inversion mode on,not esd issue*/
					ctrl->esd_cmds.cmds[j].payload[1] = ctrl->esd_cmds.cmds[j].payload[1] | (ctrl->inversion_state<<5);					
				}
				if(esd_buf[i] != ctrl->esd_cmds.cmds[j].payload[i+1])
				{
					compare_reg = -1;
					/* < DTS2014062405194 songliangliang 20140624 begin */
					/* add for esd error*/
					lcd_report_dsm_err(DSM_LCD_ESD_STATUS_ERROR_NO,esd_buf[i] , ctrl->esd_cmds.cmds[j].payload[0]);
					/* < DTS2014062405194 songliangliang 20140624 end */
					LCD_LOG_ERR("panel ic error: in %s,  reg 0x%02X %d byte should be 0x%02x, but read data =0x%02x \n",
						__func__,ctrl->esd_cmds.cmds[j].payload[0],i+1,ctrl->esd_cmds.cmds[j].payload[i+1],esd_buf[i]);
					break;
				}
			}
		}while((count<REPEAT_COUNT)&&(compare_reg == -1));
		if(count == REPEAT_COUNT)
		{
		/* < DTS2014062405194 songliangliang 20140624 begin */
			/* add for esd error*/
			lcd_report_dsm_err(DSM_LCD_ESD_REBOOT_ERROR_NO,esd_buf[i] , ctrl->esd_cmds.cmds[j].payload[0]);
			/* < DTS2014062405194 songliangliang 20140624 end */
			ret = -1;
			goto out;
		}
	}
	/* < DTS2015043000365 tianye 20150430 begin */
	/*add ESD to check 0A reg status*/
	/* < DTS2015050403227 tianye 20150505 begin */
	/* open esd check function,avoid esd running test fail*/
	/* DTS2015050403227 tianye 20150505 end > */
	/* DTS2015043000365 tianye 20150430 end > */
out:
/* < DTS2015051208660 tianye 20150512 begin */
/* avoid esd and checksum running test error */
	mutex_unlock(&ctrl->panel_data.LCD_checksum_lock);
/* DTS2015051208660 tianye 20150512 end > */
	return ret;
}
/* DTS2014111001776 zhaoyuxia 20141114 end > */
/* DTS2014052207729 renxigang 20140520 end >*/

/* < DTS2014051503541 daiyuhong 20140515 begin */
/*Add display color inversion function*/

/*<DTS2015090700926 zhangming 20150917 begin>*/
/*<add cabc node>*/
static int mdss_dsi_lcd_set_cabc_mode(struct mdss_panel_data *pdata,unsigned int cabc_mode)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	int ret = 0;
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	switch(cabc_mode)
	{

		case CABC_OFF:
			if (ctrl_pdata->dsi_panel_cabc_off_cmds.cmd_cnt)
				mdss_dsi_panel_cmds_send(ctrl_pdata, &ctrl_pdata->dsi_panel_cabc_off_cmds);
			break;
/* < DTS2015121009904  feishilin 20151215 begin */
		case CABC_UI:
			if (ctrl_pdata->dsi_panel_cabc_ui_commands.cmd_cnt)
				mdss_dsi_panel_cmds_send(ctrl_pdata, &ctrl_pdata->dsi_panel_cabc_ui_commands);
			else
				LCD_LOG_ERR("%s: Empty cabc mode: %d\n", __func__, cabc_mode);
			break;
/*  DTS2015121009904  feishilin 20151215 end >*/

		case CABC_MOVING:
			if (ctrl_pdata->dsi_panel_cabc_moving_cmds.cmd_cnt)
				mdss_dsi_panel_cmds_send(ctrl_pdata, &ctrl_pdata->dsi_panel_cabc_moving_cmds);
			break;

		case CABC_STILL:
			if (ctrl_pdata->dsi_panel_cabc_still_cmds.cmd_cnt)
				mdss_dsi_panel_cmds_send(ctrl_pdata, &ctrl_pdata->dsi_panel_cabc_still_cmds);
			break;

		default:
			ret = -EINVAL;
			LCD_LOG_ERR("%s: invalid cabc mode: %d\n", __func__, cabc_mode);
			break;
	}
	LCD_LOG_INFO("exit %s : cabc mode= %d\n",__func__,cabc_mode);
	return ret;
}
/*<DTS2015090700926 zhangming 20150917 end>*/
static int mdss_dsi_lcd_set_display_inversion(struct mdss_panel_data *pdata,unsigned int inversion_mode)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	switch(inversion_mode)
	{
		/* in each inversion mode,we send the corresponding commonds and reset inversion state */
		case INVERSION_OFF:
			if (ctrl_pdata->dsi_panel_inverse_off_cmds.cmd_cnt)
				mdss_dsi_panel_cmds_send(ctrl_pdata, &ctrl_pdata->dsi_panel_inverse_off_cmds);
			ctrl_pdata->inversion_state = INVERSION_OFF;
			break;
		case INVERSION_ON:
			if (ctrl_pdata->dsi_panel_inverse_on_cmds.cmd_cnt)
				mdss_dsi_panel_cmds_send(ctrl_pdata, &ctrl_pdata->dsi_panel_inverse_on_cmds);
			ctrl_pdata->inversion_state = INVERSION_ON;
			break;
		default:
			LCD_LOG_ERR("%s: invalid inversion mode: %d\n", __func__,inversion_mode);
			break;
	}
	LCD_LOG_INFO("exit %s : inversion mode= %d\n",__func__,inversion_mode);
	return 0;
}

/* DTS2014051503541 daiyuhong 20140515 end > */
/*< DTS2014042818102 zhengjinzeng 20140428 begin */
static int mdss_dsi_check_mipi_crc(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	int ret = 0;
	char err_num = 0;
	int read_length = 1;

	if (pdata == NULL){
		LCD_LOG_ERR("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	mdss_dsi_panel_cmd_read(ctrl_pdata,0x05,0x00,NULL,&err_num,read_length);
	LCD_LOG_INFO("exit %s,read 0x05 = %d.\n",__func__,err_num);

	if(err_num)
	{
		LCD_LOG_ERR("%s: mipi crc has %d errors.\n",__func__,err_num);
		ret = err_num;
	}
	return ret;
}
/* DTS2014042818102 zhengjinzeng 20140428 end >*/
/*<  DTS2015012604763 zhengjinzeng 20150127 begin */
static int mdss_lcd_frame_checksum(struct mdss_panel_data *pdata)
{
	int i = 0, offset = 0, ret = 0;
	char rdata = 0;
	int read_length = 1;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
/* < DTS2015072307084 dongyulong 20150725 begin */
	struct mdss_panel_info *pinfo;
/* DTS2015072307084 dongyulong 20150725 end > */

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

/* < DTS2015072307084 dongyulong 20150725 begin */
	pinfo = &ctrl->panel_data.panel_info;
/* DTS2015072307084 dongyulong 20150725 end > */

	/* < DTS2015061809153 renxigang 20150618 begin */
	return 0;
	/* DTS2015061809153 renxigang 20150618 end > */
	
	if(!ctrl->frame_checksum_support){
		LCD_LOG_INFO("%s:this panel frame checksum not supported!\n",__func__);
		return ret;//if not support, return 0 to RT test(0 == sucess)
	}

/* < DTS2015051208660 tianye 20150512 begin */
/* avoid esd and checksum running test error */
	if(mutex_lock_interruptible(&pdata->LCD_checksum_lock))
	{
		pr_err("exit %s,checksum get lock fail\n",__func__);
		return ret;
	}
/* DTS2015051208660 tianye 20150512 end > */

/* < DTS2015072307084 dongyulong 20150725 begin */
    /* BOE-OTM1901 panel doesn't support checksum enable cmd */
	if (strcmp(pinfo->panel_name,"BOE_OTM1901_5P5_1080P_VIDEO") != 0) {
		if (ctrl->dsi_frame_crc_enable_cmds.cmd_cnt){
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->dsi_frame_crc_enable_cmds);
		}
	}
/* DTS2015072307084 dongyulong 20150725 end > */

	mdelay(50);
	
	do{
		mdss_dsi_panel_cmd_read(ctrl,ctrl->frame_crc_read_cmds[i],0x00,NULL,&rdata,read_length);
		if(i == 0 && rdata != ctrl->frame_crc_read_cmds_value[offset+i]){
			offset += ctrl->panel_checksum_cmd_len;
			if(rdata != ctrl->frame_crc_read_cmds_value[offset+i])
			{
				offset += ctrl->panel_checksum_cmd_len;
				if(rdata != ctrl->frame_crc_read_cmds_value[offset+i])
				{
					ret = -1;
					LCD_LOG_ERR("%s:frame checksum fail! The %dth rdata = [0x%x]\n",__func__,i,rdata);
					break;
				}
			}
		}
		else
		{
			if(rdata != ctrl->frame_crc_read_cmds_value[offset+i])
			{
				ret = -1;
				LCD_LOG_ERR("%s:frame checksum fail! The %dth rdata = [0x%x]\n",__func__,i,rdata);
				break;
			}
		}
		i++;
	}while(i < ctrl->panel_checksum_cmd_len);
	
/* < DTS2015072307084 dongyulong 20150725 begin */
    /* BOE-OTM1901 panel doesn't support checksum disable cmd */
	if (strcmp(pinfo->panel_name,"BOE_OTM1901_5P5_1080P_VIDEO") != 0) {
		if (ctrl->dsi_frame_crc_disable_cmds.cmd_cnt){
				mdss_dsi_panel_cmds_send(ctrl, &ctrl->dsi_frame_crc_disable_cmds);
		}
	}
/* DTS2015072307084 dongyulong 20150725 end > */

/* < DTS2015051208660 tianye 20150512 begin */
/* avoid esd and checksum running test error */
	mutex_unlock(&pdata->LCD_checksum_lock);
/* DTS2015051208660 tianye 20150512 end > */
	return ret;
}
/* DTS2015012604763 zhengjinzeng 20150127 end >*/
#endif
/* DTS2014042905347 zhaoyuxia 20140429 end > */
/* DTS2014051303765 daiyuhong 20140513 end> */
static void mdss_dsi_parse_lane_swap(struct device_node *np, char *dlane_swap)
{
	const char *data;

	*dlane_swap = DSI_LANE_MAP_0123;
	data = of_get_property(np, "qcom,mdss-dsi-lane-map", NULL);
	if (data) {
		if (!strcmp(data, "lane_map_3012"))
			*dlane_swap = DSI_LANE_MAP_3012;
		else if (!strcmp(data, "lane_map_2301"))
			*dlane_swap = DSI_LANE_MAP_2301;
		else if (!strcmp(data, "lane_map_1230"))
			*dlane_swap = DSI_LANE_MAP_1230;
		else if (!strcmp(data, "lane_map_0321"))
			*dlane_swap = DSI_LANE_MAP_0321;
		else if (!strcmp(data, "lane_map_1032"))
			*dlane_swap = DSI_LANE_MAP_1032;
		else if (!strcmp(data, "lane_map_2103"))
			*dlane_swap = DSI_LANE_MAP_2103;
		else if (!strcmp(data, "lane_map_3210"))
			*dlane_swap = DSI_LANE_MAP_3210;
	}
}

static void mdss_dsi_parse_trigger(struct device_node *np, char *trigger,
		char *trigger_key)
{
	const char *data;

	*trigger = DSI_CMD_TRIGGER_SW;
	data = of_get_property(np, trigger_key, NULL);
	if (data) {
		if (!strcmp(data, "none"))
			*trigger = DSI_CMD_TRIGGER_NONE;
		else if (!strcmp(data, "trigger_te"))
			*trigger = DSI_CMD_TRIGGER_TE;
		else if (!strcmp(data, "trigger_sw_seof"))
			*trigger = DSI_CMD_TRIGGER_SW_SEOF;
		else if (!strcmp(data, "trigger_sw_te"))
			*trigger = DSI_CMD_TRIGGER_SW_TE;
	}
}


static int mdss_dsi_parse_dcs_cmds(struct device_node *np,
		struct dsi_panel_cmds *pcmds, char *cmd_key, char *link_key)
{
	const char *data;
	int blen = 0, len;
	char *buf, *bp;
	struct dsi_ctrl_hdr *dchdr;
	int i, cnt;

	data = of_get_property(np, cmd_key, &blen);
	if (!data) {
		pr_err("%s: failed, key=%s\n", __func__, cmd_key);
		return -ENOMEM;
	}

	buf = kzalloc(sizeof(char) * blen, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	memcpy(buf, data, blen);

	/* scan dcs commands */
	bp = buf;
	len = blen;
	cnt = 0;
	while (len >= sizeof(*dchdr)) {
		dchdr = (struct dsi_ctrl_hdr *)bp;
		dchdr->dlen = ntohs(dchdr->dlen);
		if (dchdr->dlen > len) {
			pr_err("%s: dtsi cmd=%x error, len=%d",
				__func__, dchdr->dtype, dchdr->dlen);
			goto exit_free;
		}
		bp += sizeof(*dchdr);
		len -= sizeof(*dchdr);
		bp += dchdr->dlen;
		len -= dchdr->dlen;
		cnt++;
	}

	if (len != 0) {
		pr_err("%s: dcs_cmd=%x len=%d error!",
				__func__, buf[0], blen);
		goto exit_free;
	}

	pcmds->cmds = kzalloc(cnt * sizeof(struct dsi_cmd_desc),
						GFP_KERNEL);
	if (!pcmds->cmds)
		goto exit_free;

	pcmds->cmd_cnt = cnt;
	pcmds->buf = buf;
	pcmds->blen = blen;

	bp = buf;
	len = blen;
	for (i = 0; i < cnt; i++) {
		dchdr = (struct dsi_ctrl_hdr *)bp;
		len -= sizeof(*dchdr);
		bp += sizeof(*dchdr);
		pcmds->cmds[i].dchdr = *dchdr;
		pcmds->cmds[i].payload = bp;
		bp += dchdr->dlen;
		len -= dchdr->dlen;
	}

	/*Set default link state to LP Mode*/
	pcmds->link_state = DSI_LP_MODE;

	if (link_key) {
		data = of_get_property(np, link_key, NULL);
		if (data && !strcmp(data, "dsi_hs_mode"))
			pcmds->link_state = DSI_HS_MODE;
		else
			pcmds->link_state = DSI_LP_MODE;
	}

	pr_debug("%s: dcs_cmd=%x len=%d, cmd_cnt=%d link_state=%d\n", __func__,
		pcmds->buf[0], pcmds->blen, pcmds->cmd_cnt, pcmds->link_state);

	return 0;

exit_free:
	kfree(buf);
	return -ENOMEM;
}


int mdss_panel_get_dst_fmt(u32 bpp, char mipi_mode, u32 pixel_packing,
				char *dst_format)
{
	int rc = 0;
	switch (bpp) {
	case 3:
		*dst_format = DSI_CMD_DST_FORMAT_RGB111;
		break;
	case 8:
		*dst_format = DSI_CMD_DST_FORMAT_RGB332;
		break;
	case 12:
		*dst_format = DSI_CMD_DST_FORMAT_RGB444;
		break;
	case 16:
		switch (mipi_mode) {
		case DSI_VIDEO_MODE:
			*dst_format = DSI_VIDEO_DST_FORMAT_RGB565;
			break;
		case DSI_CMD_MODE:
			*dst_format = DSI_CMD_DST_FORMAT_RGB565;
			break;
		default:
			*dst_format = DSI_VIDEO_DST_FORMAT_RGB565;
			break;
		}
		break;
	case 18:
		switch (mipi_mode) {
		case DSI_VIDEO_MODE:
			if (pixel_packing == 0)
				*dst_format = DSI_VIDEO_DST_FORMAT_RGB666;
			else
				*dst_format = DSI_VIDEO_DST_FORMAT_RGB666_LOOSE;
			break;
		case DSI_CMD_MODE:
			*dst_format = DSI_CMD_DST_FORMAT_RGB666;
			break;
		default:
			if (pixel_packing == 0)
				*dst_format = DSI_VIDEO_DST_FORMAT_RGB666;
			else
				*dst_format = DSI_VIDEO_DST_FORMAT_RGB666_LOOSE;
			break;
		}
		break;
	case 24:
		switch (mipi_mode) {
		case DSI_VIDEO_MODE:
			*dst_format = DSI_VIDEO_DST_FORMAT_RGB888;
			break;
		case DSI_CMD_MODE:
			*dst_format = DSI_CMD_DST_FORMAT_RGB888;
			break;
		default:
			*dst_format = DSI_VIDEO_DST_FORMAT_RGB888;
			break;
		}
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}


static int mdss_dsi_parse_fbc_params(struct device_node *np,
				struct mdss_panel_info *panel_info)
{
	int rc, fbc_enabled = 0;
	u32 tmp;

	fbc_enabled = of_property_read_bool(np,	"qcom,mdss-dsi-fbc-enable");
	if (fbc_enabled) {
		pr_debug("%s:%d FBC panel enabled.\n", __func__, __LINE__);
		panel_info->fbc.enabled = 1;
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-bpp", &tmp);
		panel_info->fbc.target_bpp =	(!rc ? tmp : panel_info->bpp);
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-packing",
				&tmp);
		panel_info->fbc.comp_mode = (!rc ? tmp : 0);
		panel_info->fbc.qerr_enable = of_property_read_bool(np,
			"qcom,mdss-dsi-fbc-quant-error");
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-bias", &tmp);
		panel_info->fbc.cd_bias = (!rc ? tmp : 0);
		panel_info->fbc.pat_enable = of_property_read_bool(np,
				"qcom,mdss-dsi-fbc-pat-mode");
		panel_info->fbc.vlc_enable = of_property_read_bool(np,
				"qcom,mdss-dsi-fbc-vlc-mode");
		panel_info->fbc.bflc_enable = of_property_read_bool(np,
				"qcom,mdss-dsi-fbc-bflc-mode");
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-h-line-budget",
				&tmp);
		panel_info->fbc.line_x_budget = (!rc ? tmp : 0);
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-budget-ctrl",
				&tmp);
		panel_info->fbc.block_x_budget = (!rc ? tmp : 0);
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-block-budget",
				&tmp);
		panel_info->fbc.block_budget = (!rc ? tmp : 0);
		rc = of_property_read_u32(np,
				"qcom,mdss-dsi-fbc-lossless-threshold", &tmp);
		panel_info->fbc.lossless_mode_thd = (!rc ? tmp : 0);
		rc = of_property_read_u32(np,
				"qcom,mdss-dsi-fbc-lossy-threshold", &tmp);
		panel_info->fbc.lossy_mode_thd = (!rc ? tmp : 0);
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-rgb-threshold",
				&tmp);
		panel_info->fbc.lossy_rgb_thd = (!rc ? tmp : 0);
		rc = of_property_read_u32(np,
				"qcom,mdss-dsi-fbc-lossy-mode-idx", &tmp);
		panel_info->fbc.lossy_mode_idx = (!rc ? tmp : 0);
		rc = of_property_read_u32(np,
				"qcom,mdss-dsi-fbc-slice-height", &tmp);
		panel_info->fbc.slice_height = (!rc ? tmp : 0);
		panel_info->fbc.pred_mode = of_property_read_bool(np,
				"qcom,mdss-dsi-fbc-2d-pred-mode");
		panel_info->fbc.enc_mode = of_property_read_bool(np,
				"qcom,mdss-dsi-fbc-ver2-mode");
		rc = of_property_read_u32(np,
				"qcom,mdss-dsi-fbc-max-pred-err", &tmp);
		panel_info->fbc.max_pred_err = (!rc ? tmp : 0);
	} else {
		pr_debug("%s:%d Panel does not support FBC.\n",
				__func__, __LINE__);
		panel_info->fbc.enabled = 0;
		panel_info->fbc.target_bpp =
			panel_info->bpp;
	}
	return 0;
}

static void mdss_panel_parse_te_params(struct device_node *np,
				       struct mdss_panel_info *panel_info)
{

	u32 tmp;
	int rc = 0;
	/*
	 * TE default: dsi byte clock calculated base on 70 fps;
	 * around 14 ms to complete a kickoff cycle if te disabled;
	 * vclk_line base on 60 fps; write is faster than read;
	 * init == start == rdptr;
	 */
	panel_info->te.tear_check_en =
		!of_property_read_bool(np, "qcom,mdss-tear-check-disable");
	rc = of_property_read_u32
		(np, "qcom,mdss-tear-check-sync-cfg-height", &tmp);
	panel_info->te.sync_cfg_height = (!rc ? tmp : 0xfff0);
	rc = of_property_read_u32
		(np, "qcom,mdss-tear-check-sync-init-val", &tmp);
	panel_info->te.vsync_init_val = (!rc ? tmp : panel_info->yres);
	rc = of_property_read_u32
		(np, "qcom,mdss-tear-check-sync-threshold-start", &tmp);
	panel_info->te.sync_threshold_start = (!rc ? tmp : 4);
	rc = of_property_read_u32
		(np, "qcom,mdss-tear-check-sync-threshold-continue", &tmp);
	panel_info->te.sync_threshold_continue = (!rc ? tmp : 4);
	rc = of_property_read_u32(np, "qcom,mdss-tear-check-start-pos", &tmp);
	panel_info->te.start_pos = (!rc ? tmp : panel_info->yres);
	rc = of_property_read_u32
		(np, "qcom,mdss-tear-check-rd-ptr-trigger-intr", &tmp);
	panel_info->te.rd_ptr_irq = (!rc ? tmp : panel_info->yres + 1);
	rc = of_property_read_u32(np, "qcom,mdss-tear-check-frame-rate", &tmp);
	panel_info->te.refx100 = (!rc ? tmp : 6000);
}


static int mdss_dsi_parse_reset_seq(struct device_node *np,
		u32 rst_seq[MDSS_DSI_RST_SEQ_LEN], u32 *rst_len,
		const char *name)
{
	int num = 0, i;
	int rc;
	struct property *data;
	u32 tmp[MDSS_DSI_RST_SEQ_LEN];
	*rst_len = 0;
	data = of_find_property(np, name, &num);
	num /= sizeof(u32);
	if (!data || !num || num > MDSS_DSI_RST_SEQ_LEN || num % 2) {
		pr_debug("%s:%d, error reading %s, length found = %d\n",
			__func__, __LINE__, name, num);
	} else {
		rc = of_property_read_u32_array(np, name, tmp, num);
		if (rc)
			pr_debug("%s:%d, error reading %s, rc = %d\n",
				__func__, __LINE__, name, rc);
		else {
			for (i = 0; i < num; ++i)
				rst_seq[i] = tmp[i];
			*rst_len = num;
		}
	}
	return 0;
}

static int mdss_dsi_gen_read_status(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	if (ctrl_pdata->status_buf.data[0] !=
					ctrl_pdata->status_value) {
		pr_err("%s: Read back value from panel is incorrect\n",
							__func__);
		return -EINVAL;
	} else {
		return 1;
	}
}

static int mdss_dsi_nt35596_read_status(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	if (ctrl_pdata->status_buf.data[0] !=
					ctrl_pdata->status_value) {
		ctrl_pdata->status_error_count = 0;
		pr_err("%s: Read back value from panel is incorrect\n",
							__func__);
		return -EINVAL;
	} else {
		if (ctrl_pdata->status_buf.data[3] != NT35596_BUF_3_STATUS) {
			ctrl_pdata->status_error_count = 0;
		} else {
			if ((ctrl_pdata->status_buf.data[4] ==
				NT35596_BUF_4_STATUS) ||
				(ctrl_pdata->status_buf.data[5] ==
				NT35596_BUF_5_STATUS))
				ctrl_pdata->status_error_count = 0;
			else
				ctrl_pdata->status_error_count++;
			if (ctrl_pdata->status_error_count >=
					NT35596_MAX_ERR_CNT) {
				ctrl_pdata->status_error_count = 0;
				pr_err("%s: Read value bad. Error_cnt = %i\n",
					 __func__,
					ctrl_pdata->status_error_count);
				return -EINVAL;
			}
		}
		return 1;
	}
}

static void mdss_dsi_parse_roi_alignment(struct device_node *np,
		struct mdss_panel_info *pinfo)
{
	int len = 0;
	u32 value[6];
	struct property *data;
	data = of_find_property(np, "qcom,panel-roi-alignment", &len);
	len /= sizeof(u32);
	if (!data || (len != 6)) {
		pr_debug("%s: Panel roi alignment not found", __func__);
	} else {
		int rc = of_property_read_u32_array(np,
				"qcom,panel-roi-alignment", value, len);
		if (rc)
			pr_debug("%s: Error reading panel roi alignment values",
					__func__);
		else {
			pinfo->xstart_pix_align = value[0];
			pinfo->width_pix_align = value[1];
			pinfo->ystart_pix_align = value[2];
			pinfo->height_pix_align = value[3];
			pinfo->min_width = value[4];
			pinfo->min_height = value[5];
		}

		pr_debug("%s: ROI alignment: [%d, %d, %d, %d, %d, %d]",
				__func__, pinfo->xstart_pix_align,
				pinfo->width_pix_align, pinfo->ystart_pix_align,
				pinfo->height_pix_align, pinfo->min_width,
				pinfo->min_height);
	}
}

static int mdss_dsi_parse_panel_features(struct device_node *np,
	struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct mdss_panel_info *pinfo;

	if (!np || !ctrl) {
		pr_err("%s: Invalid arguments\n", __func__);
		return -ENODEV;
	}

	pinfo = &ctrl->panel_data.panel_info;

	pinfo->cont_splash_enabled = of_property_read_bool(np,
		"qcom,cont-splash-enabled");

	if (pinfo->mipi.mode == DSI_CMD_MODE) {
		pinfo->partial_update_enabled = of_property_read_bool(np,
				"qcom,partial-update-enabled");
		pr_info("%s: partial_update_enabled=%d\n", __func__,
					pinfo->partial_update_enabled);
		if (pinfo->partial_update_enabled) {
			ctrl->set_col_page_addr = mdss_dsi_set_col_page_addr;
			pinfo->partial_update_roi_merge =
					of_property_read_bool(np,
					"qcom,partial-update-roi-merge");
		}

		pinfo->dcs_cmd_by_left = of_property_read_bool(np,
						"qcom,dcs-cmd-by-left");
	}

	pinfo->ulps_feature_enabled = of_property_read_bool(np,
		"qcom,ulps-enabled");
	pr_info("%s: ulps feature %s\n", __func__,
		(pinfo->ulps_feature_enabled ? "enabled" : "disabled"));
	pinfo->esd_check_enabled = of_property_read_bool(np,
		"qcom,esd-check-enabled");

	pinfo->ulps_suspend_enabled = of_property_read_bool(np,
		"qcom,suspend-ulps-enabled");
	pr_info("%s: ulps during suspend feature %s", __func__,
		(pinfo->ulps_suspend_enabled ? "enabled" : "disabled"));

	pinfo->mipi.dynamic_switch_enabled = of_property_read_bool(np,
		"qcom,dynamic-mode-switch-enabled");

	if (pinfo->mipi.dynamic_switch_enabled) {
		mdss_dsi_parse_dcs_cmds(np, &ctrl->video2cmd,
			"qcom,video-to-cmd-mode-switch-commands", NULL);

		mdss_dsi_parse_dcs_cmds(np, &ctrl->cmd2video,
			"qcom,cmd-to-video-mode-switch-commands", NULL);

		if (!ctrl->video2cmd.cmd_cnt || !ctrl->cmd2video.cmd_cnt) {
			pr_warn("No commands specified for dynamic switch\n");
			pinfo->mipi.dynamic_switch_enabled = 0;
		}
	}

	pr_info("%s: dynamic switch feature enabled: %d\n", __func__,
		pinfo->mipi.dynamic_switch_enabled);
	pinfo->panel_ack_disabled = of_property_read_bool(np,
				"qcom,panel-ack-disabled");

	if (pinfo->panel_ack_disabled && pinfo->esd_check_enabled) {
		pr_warn("ESD should not be enabled if panel ACK is disabled\n");
		pinfo->esd_check_enabled = false;
	}

	if (ctrl->disp_en_gpio <= 0) {
		ctrl->disp_en_gpio = of_get_named_gpio(
			np,
			"qcom,5v-boost-gpio", 0);

		if (!gpio_is_valid(ctrl->disp_en_gpio))
			pr_err("%s:%d, Disp_en gpio not specified\n",
					__func__, __LINE__);
	}

	return 0;
}

static void mdss_dsi_parse_panel_horizintal_line_idle(struct device_node *np,
	struct mdss_dsi_ctrl_pdata *ctrl)
{
	const u32 *src;
	int i, len, cnt;
	struct panel_horizontal_idle *kp;

	if (!np || !ctrl) {
		pr_err("%s: Invalid arguments\n", __func__);
		return;
	}

	src = of_get_property(np, "qcom,mdss-dsi-hor-line-idle", &len);
	if (!src || len == 0)
		return;

	cnt = len % 3; /* 3 fields per entry */
	if (cnt) {
		pr_err("%s: invalid horizontal idle len=%d\n", __func__, len);
		return;
	}

	cnt = len / sizeof(u32);

	kp = kzalloc(sizeof(*kp) * (cnt / 3), GFP_KERNEL);
	if (kp == NULL) {
		pr_err("%s: No memory\n", __func__);
		return;
	}

	ctrl->line_idle = kp;
	for (i = 0; i < cnt; i += 3) {
		kp->min = be32_to_cpu(src[i]);
		kp->max = be32_to_cpu(src[i+1]);
		kp->idle = be32_to_cpu(src[i+2]);
		kp++;
		ctrl->horizontal_idle_cnt++;
	}

	pr_debug("%s: horizontal_idle_cnt=%d\n", __func__,
				ctrl->horizontal_idle_cnt);
}

static int mdss_dsi_set_refresh_rate_range(struct device_node *pan_node,
		struct mdss_panel_info *pinfo)
{
	int rc = 0;
	rc = of_property_read_u32(pan_node,
			"qcom,mdss-dsi-min-refresh-rate",
			&pinfo->min_fps);
	if (rc) {
		pr_warn("%s:%d, Unable to read min refresh rate\n",
				__func__, __LINE__);

		/*
		 * Since min refresh rate is not specified when dynamic
		 * fps is enabled, using minimum as 30
		 */
		pinfo->min_fps = MIN_REFRESH_RATE;
		rc = 0;
	}

	rc = of_property_read_u32(pan_node,
			"qcom,mdss-dsi-max-refresh-rate",
			&pinfo->max_fps);
	if (rc) {
		pr_warn("%s:%d, Unable to read max refresh rate\n",
				__func__, __LINE__);

		/*
		 * Since max refresh rate was not specified when dynamic
		 * fps is enabled, using the default panel refresh rate
		 * as max refresh rate supported.
		 */
		pinfo->max_fps = pinfo->mipi.frame_rate;
		rc = 0;
	}

	pr_info("dyn_fps: min = %d, max = %d\n",
			pinfo->min_fps, pinfo->max_fps);
	return rc;
}

static void mdss_dsi_parse_dfps_config(struct device_node *pan_node,
			struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	const char *data;
	bool dynamic_fps;
	struct mdss_panel_info *pinfo = &(ctrl_pdata->panel_data.panel_info);

	dynamic_fps = of_property_read_bool(pan_node,
			"qcom,mdss-dsi-pan-enable-dynamic-fps");

	if (!dynamic_fps)
		return;

	pinfo->dynamic_fps = true;
	data = of_get_property(pan_node, "qcom,mdss-dsi-pan-fps-update", NULL);
	if (data) {
		if (!strcmp(data, "dfps_suspend_resume_mode")) {
			pinfo->dfps_update = DFPS_SUSPEND_RESUME_MODE;
			pr_debug("dfps mode: suspend/resume\n");
		} else if (!strcmp(data, "dfps_immediate_clk_mode")) {
			pinfo->dfps_update = DFPS_IMMEDIATE_CLK_UPDATE_MODE;
			pr_debug("dfps mode: Immediate clk\n");
		} else if (!strcmp(data, "dfps_immediate_porch_mode")) {
			pinfo->dfps_update = DFPS_IMMEDIATE_PORCH_UPDATE_MODE;
			pr_debug("dfps mode: Immediate porch\n");
		} else {
			pinfo->dfps_update = DFPS_SUSPEND_RESUME_MODE;
			pr_debug("default dfps mode: suspend/resume\n");
		}
		mdss_dsi_set_refresh_rate_range(pan_node, pinfo);
	} else {
		pinfo->dynamic_fps = false;
		pr_debug("dfps update mode not configured: disable\n");
	}
	pinfo->new_fps = pinfo->mipi.frame_rate;

	return;
}

/*< DTS2014052207729 renxigang 20140520 begin */
/***************************************************************
Function: mdss_paned_parse_esd_dt
Description: parse the esd concerned setting
Parameters:
	struct device_node *np: device node
	struct mdss_dsi_ctrl_pdata *ctrl_pdata: dsi control parameter struct
Return:0:success
Return:-ENOMEM:fail
***************************************************************/
#ifdef CONFIG_HUAWEI_LCD
/*< DTS2015061105286 liujunchao 20150618 begin */
static void mdss_panel_parse_esd_dt(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int rc;
	u32 tmp;
	ctrl_pdata->esd_check_enable = of_property_read_bool(np, "qcom,panel-esd-check-enabled");
	/*< DTS2014061007154 renxigang 20140610 begin */
	if(ctrl_pdata->esd_check_enable)
	{
		rc = mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->esd_cmds,
			"qcom,panel-esd-read-commands","qcom,esd-read-cmds-state");
		if(rc < 0)
		{
			pr_err("%s:%d, parse esd command fail,esd check disabled\n",
					__func__, __LINE__);
			ctrl_pdata->esd_check_enable = false;
		}
		else
		{
			pr_info("%s:, esd check enabled \n",
					__func__);
		}
		rc = of_property_read_u32(np, "huawei,panel-esd-set-cabc-flag", &tmp);
		ctrl_pdata->esd_set_cabc_flag = (!rc ? tmp : 0);
	}
	else
	{
		pr_info("%s:, esd check not enabled \n",
					__func__);
	}
	/* DTS2014061007154 renxigang 20140610 end >*/
}
/* DTS2015061105286 liujunchao 20150618 end >*/
/*< DTS2015012604763 zhengjinzeng 20150127 begin */
static void mdss_panel_parse_frame_checksum_dt(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int rc = 0, i = 0, len = 0;
	const char *data;
	
	ctrl_pdata->frame_checksum_support = of_property_read_bool(np, "qcom,panel-frame-checksum-support");
	LCD_LOG_INFO("%s: frame checksum %s by this panel!\n",__func__,
					ctrl_pdata->frame_checksum_support?"supported":"not supported");

	if(ctrl_pdata->frame_checksum_support)
	{
		rc = mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->dsi_frame_crc_enable_cmds,
			"qcom,panel-frame_crc_enable_cmds","qcom,frame_crc_enable-cmds-state");
		if(rc < 0)
		{
			LCD_LOG_ERR("%s: parse frame_crc_enable command fail, frame_checksum not supported!\n",
					__func__);
			goto exit;
		}

		rc = mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->dsi_frame_crc_disable_cmds,
			"qcom,panel-frame_crc_disable_cmds","qcom,frame_crc_disable-cmds-state");
		if(rc < 0)
		{
			LCD_LOG_ERR("%s: parse frame_crc_disable command fail, frame_checksum not supported!\n",
					__func__);
			goto exit;
		}

		data = of_get_property(np, "qcom,panel-frame_crc_read_cmds", &len);
		if ((!data) || (!len)) {
			LCD_LOG_ERR("%s: parse frame_crc_read command fail, frame_checksum not supported!\n",
						__func__);
			goto exit;
		}
		else
		{
			ctrl_pdata->panel_checksum_cmd_len = len;
			for (i = 0; i < len; i++){
				ctrl_pdata->frame_crc_read_cmds[i] = data[i];
			}
		}

		data = of_get_property(np, "qcom,panel-frame_crc_read_cmds_value", &len);
		if ((!data) || (!len)) {
			LCD_LOG_ERR("%s: parse frame_crc_read command value fail, frame_checksum not supported!\n",
				__func__);
			goto exit;
		}
		else{
			for (i = 0; i < len; i++){
				ctrl_pdata->frame_crc_read_cmds_value[i] = data[i];
			}
		}
		
	}

	return;
	exit:
		ctrl_pdata->frame_checksum_support = false;
		LCD_LOG_ERR("%s : parse frame_crc dts cmds unsuccess! disable checksum!\n",__func__);
	return;
}
/* DTS2015012604763 zhengjinzeng 20150127 end> */
#endif
/* DTS2014052207729 renxigang 20140520 end >*/

static int mdss_panel_parse_dt(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	u32 tmp;
	int rc, i, len;
	const char *data;
	static const char *pdest;
	struct mdss_panel_info *pinfo = &(ctrl_pdata->panel_data.panel_info);

	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-width", &tmp);
	if (rc) {
		pr_err("%s:%d, panel width not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	pinfo->xres = (!rc ? tmp : 640);

	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-height", &tmp);
	if (rc) {
		pr_err("%s:%d, panel height not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	pinfo->yres = (!rc ? tmp : 480);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-physical-width-dimension", &tmp);
	pinfo->physical_width = (!rc ? tmp : 0);
	rc = of_property_read_u32(np,
		"qcom,mdss-pan-physical-height-dimension", &tmp);
	pinfo->physical_height = (!rc ? tmp : 0);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-left-border", &tmp);
	pinfo->lcdc.xres_pad = (!rc ? tmp : 0);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-right-border", &tmp);
	if (!rc)
		pinfo->lcdc.xres_pad += tmp;
	rc = of_property_read_u32(np, "qcom,mdss-dsi-v-top-border", &tmp);
	pinfo->lcdc.yres_pad = (!rc ? tmp : 0);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-v-bottom-border", &tmp);
	if (!rc)
		pinfo->lcdc.yres_pad += tmp;
	rc = of_property_read_u32(np, "qcom,mdss-dsi-bpp", &tmp);
	if (rc) {
		pr_err("%s:%d, bpp not specified\n", __func__, __LINE__);
		return -EINVAL;
	}
	pinfo->bpp = (!rc ? tmp : 24);
	pinfo->mipi.mode = DSI_VIDEO_MODE;
	data = of_get_property(np, "qcom,mdss-dsi-panel-type", NULL);
	if (data && !strncmp(data, "dsi_cmd_mode", 12))
		pinfo->mipi.mode = DSI_CMD_MODE;
	tmp = 0;
	data = of_get_property(np, "qcom,mdss-dsi-pixel-packing", NULL);
	if (data && !strcmp(data, "loose"))
		pinfo->mipi.pixel_packing = 1;
	else
		pinfo->mipi.pixel_packing = 0;
	rc = mdss_panel_get_dst_fmt(pinfo->bpp,
		pinfo->mipi.mode, pinfo->mipi.pixel_packing,
		&(pinfo->mipi.dst_format));
	if (rc) {
		pr_debug("%s: problem determining dst format. Set Default\n",
			__func__);
		pinfo->mipi.dst_format =
			DSI_VIDEO_DST_FORMAT_RGB888;
	}
	pdest = of_get_property(np,
		"qcom,mdss-dsi-panel-destination", NULL);

	if (pdest) {
		if (strlen(pdest) != 9) {
			pr_err("%s: Unknown pdest specified\n", __func__);
			return -EINVAL;
		}
		if (!strcmp(pdest, "display_1"))
			pinfo->pdest = DISPLAY_1;
		else if (!strcmp(pdest, "display_2"))
			pinfo->pdest = DISPLAY_2;
		else {
			pr_debug("%s: incorrect pdest. Set Default\n",
				__func__);
			pinfo->pdest = DISPLAY_1;
		}
	} else {
		pr_debug("%s: pdest not specified. Set Default\n",
				__func__);
		pinfo->pdest = DISPLAY_1;
	}
	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-front-porch", &tmp);
	pinfo->lcdc.h_front_porch = (!rc ? tmp : 6);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-back-porch", &tmp);
	pinfo->lcdc.h_back_porch = (!rc ? tmp : 6);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-pulse-width", &tmp);
	pinfo->lcdc.h_pulse_width = (!rc ? tmp : 2);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-sync-skew", &tmp);
	pinfo->lcdc.hsync_skew = (!rc ? tmp : 0);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-v-back-porch", &tmp);
	pinfo->lcdc.v_back_porch = (!rc ? tmp : 6);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-v-front-porch", &tmp);
	pinfo->lcdc.v_front_porch = (!rc ? tmp : 6);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-v-pulse-width", &tmp);
	pinfo->lcdc.v_pulse_width = (!rc ? tmp : 2);
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-underflow-color", &tmp);
	pinfo->lcdc.underflow_clr = (!rc ? tmp : 0xff);
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-border-color", &tmp);
	pinfo->lcdc.border_clr = (!rc ? tmp : 0);
	data = of_get_property(np, "qcom,mdss-dsi-panel-orientation", NULL);
	if (data) {
		pr_debug("panel orientation is %s\n", data);
		if (!strcmp(data, "180"))
			pinfo->panel_orientation = MDP_ROT_180;
		else if (!strcmp(data, "hflip"))
			pinfo->panel_orientation = MDP_FLIP_LR;
		else if (!strcmp(data, "vflip"))
			pinfo->panel_orientation = MDP_FLIP_UD;
	}

	ctrl_pdata->bklt_ctrl = UNKNOWN_CTRL;
	data = of_get_property(np, "qcom,mdss-dsi-bl-pmic-control-type", NULL);
	if (data) {
		if (!strncmp(data, "bl_ctrl_wled", 12)) {
			led_trigger_register_simple("bkl-trigger",
				&bl_led_trigger);
			pr_debug("%s: SUCCESS-> WLED TRIGGER register\n",
				__func__);
			ctrl_pdata->bklt_ctrl = BL_WLED;
		} else if (!strncmp(data, "bl_ctrl_pwm", 11)) {
			ctrl_pdata->bklt_ctrl = BL_PWM;
			ctrl_pdata->pwm_pmi = of_property_read_bool(np,
					"qcom,mdss-dsi-bl-pwm-pmi");
			rc = of_property_read_u32(np,
				"qcom,mdss-dsi-bl-pmic-pwm-frequency", &tmp);
			if (rc) {
				pr_err("%s:%d, Error, panel pwm_period\n",
						__func__, __LINE__);
				return -EINVAL;
			}
			ctrl_pdata->pwm_period = tmp;
			if (ctrl_pdata->pwm_pmi) {
				ctrl_pdata->pwm_bl = of_pwm_get(np, NULL);
				if (IS_ERR(ctrl_pdata->pwm_bl)) {
					pr_err("%s: Error, pwm device\n",
								__func__);
					ctrl_pdata->pwm_bl = NULL;
					return -EINVAL;
				}
			} else {
				rc = of_property_read_u32(np,
					"qcom,mdss-dsi-bl-pmic-bank-select",
								 &tmp);
				if (rc) {
					pr_err("%s:%d, Error, lpg channel\n",
							__func__, __LINE__);
					return -EINVAL;
				}
				ctrl_pdata->pwm_lpg_chan = tmp;
				tmp = of_get_named_gpio(np,
					"qcom,mdss-dsi-pwm-gpio", 0);
				ctrl_pdata->pwm_pmic_gpio = tmp;
				pr_debug("%s: Configured PWM bklt ctrl\n",
								 __func__);
			}
		} else if (!strncmp(data, "bl_ctrl_dcs", 11)) {
			ctrl_pdata->bklt_ctrl = BL_DCS_CMD;
			pr_debug("%s: Configured DCS_CMD bklt ctrl\n",
								__func__);
		}
	}
	rc = of_property_read_u32(np, "qcom,mdss-brightness-max-level", &tmp);
	pinfo->brightness_max = (!rc ? tmp : MDSS_MAX_BL_BRIGHTNESS);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-bl-min-level", &tmp);
	pinfo->bl_min = (!rc ? tmp : 0);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-bl-max-level", &tmp);
	pinfo->bl_max = (!rc ? tmp : 255);
	ctrl_pdata->bklt_max = pinfo->bl_max;

	rc = of_property_read_u32(np, "qcom,mdss-dsi-interleave-mode", &tmp);
	pinfo->mipi.interleave_mode = (!rc ? tmp : 0);

	pinfo->mipi.vsync_enable = of_property_read_bool(np,
		"qcom,mdss-dsi-te-check-enable");
	pinfo->mipi.hw_vsync_mode = of_property_read_bool(np,
		"qcom,mdss-dsi-te-using-te-pin");

	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-h-sync-pulse", &tmp);
	pinfo->mipi.pulse_mode_hsa_he = (!rc ? tmp : false);

	pinfo->mipi.hfp_power_stop = of_property_read_bool(np,
		"qcom,mdss-dsi-hfp-power-mode");
	pinfo->mipi.hsa_power_stop = of_property_read_bool(np,
		"qcom,mdss-dsi-hsa-power-mode");
	pinfo->mipi.hbp_power_stop = of_property_read_bool(np,
		"qcom,mdss-dsi-hbp-power-mode");
	pinfo->mipi.last_line_interleave_en = of_property_read_bool(np,
		"qcom,mdss-dsi-last-line-interleave");
	pinfo->mipi.bllp_power_stop = of_property_read_bool(np,
		"qcom,mdss-dsi-bllp-power-mode");
	pinfo->mipi.eof_bllp_power_stop = of_property_read_bool(
		np, "qcom,mdss-dsi-bllp-eof-power-mode");
	pinfo->mipi.traffic_mode = DSI_NON_BURST_SYNCH_PULSE;
	data = of_get_property(np, "qcom,mdss-dsi-traffic-mode", NULL);
	if (data) {
		if (!strcmp(data, "non_burst_sync_event"))
			pinfo->mipi.traffic_mode = DSI_NON_BURST_SYNCH_EVENT;
		else if (!strcmp(data, "burst_mode"))
			pinfo->mipi.traffic_mode = DSI_BURST_MODE;
	}
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-te-dcs-command", &tmp);
	pinfo->mipi.insert_dcs_cmd =
			(!rc ? tmp : 1);
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-wr-mem-continue", &tmp);
	pinfo->mipi.wr_mem_continue =
			(!rc ? tmp : 0x3c);
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-wr-mem-start", &tmp);
	pinfo->mipi.wr_mem_start =
			(!rc ? tmp : 0x2c);
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-te-pin-select", &tmp);
	pinfo->mipi.te_sel =
			(!rc ? tmp : 1);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-virtual-channel-id", &tmp);
	pinfo->mipi.vc = (!rc ? tmp : 0);
	pinfo->mipi.rgb_swap = DSI_RGB_SWAP_RGB;
	data = of_get_property(np, "qcom,mdss-dsi-color-order", NULL);
	if (data) {
		if (!strcmp(data, "rgb_swap_rbg"))
			pinfo->mipi.rgb_swap = DSI_RGB_SWAP_RBG;
		else if (!strcmp(data, "rgb_swap_bgr"))
			pinfo->mipi.rgb_swap = DSI_RGB_SWAP_BGR;
		else if (!strcmp(data, "rgb_swap_brg"))
			pinfo->mipi.rgb_swap = DSI_RGB_SWAP_BRG;
		else if (!strcmp(data, "rgb_swap_grb"))
			pinfo->mipi.rgb_swap = DSI_RGB_SWAP_GRB;
		else if (!strcmp(data, "rgb_swap_gbr"))
			pinfo->mipi.rgb_swap = DSI_RGB_SWAP_GBR;
	}
	pinfo->mipi.data_lane0 = of_property_read_bool(np,
		"qcom,mdss-dsi-lane-0-state");
	pinfo->mipi.data_lane1 = of_property_read_bool(np,
		"qcom,mdss-dsi-lane-1-state");
	pinfo->mipi.data_lane2 = of_property_read_bool(np,
		"qcom,mdss-dsi-lane-2-state");
	pinfo->mipi.data_lane3 = of_property_read_bool(np,
		"qcom,mdss-dsi-lane-3-state");

	rc = of_property_read_u32(np, "qcom,mdss-dsi-t-clk-pre", &tmp);
	pinfo->mipi.t_clk_pre = (!rc ? tmp : 0x24);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-t-clk-post", &tmp);
	pinfo->mipi.t_clk_post = (!rc ? tmp : 0x03);

	pinfo->mipi.rx_eot_ignore = of_property_read_bool(np,
		"qcom,mdss-dsi-rx-eot-ignore");
	pinfo->mipi.tx_eot_append = of_property_read_bool(np,
		"qcom,mdss-dsi-tx-eot-append");

	/* < DTS2015070607268 zhangming 20150716 begin */
	pinfo->mipi.force_clk_lane_hs = of_property_read_bool(np,
		"qcom,mdss-dsi-set-high-speed");
	/*  DTS2015070607268 zhangming 20150716 end>*/
	rc = of_property_read_u32(np, "qcom,mdss-dsi-stream", &tmp);
	pinfo->mipi.stream = (!rc ? tmp : 0);

	data = of_get_property(np, "qcom,mdss-dsi-panel-mode-gpio-state", NULL);
	if (data) {
		if (!strcmp(data, "high"))
			pinfo->mode_gpio_state = MODE_GPIO_HIGH;
		else if (!strcmp(data, "low"))
			pinfo->mode_gpio_state = MODE_GPIO_LOW;
	} else {
		pinfo->mode_gpio_state = MODE_GPIO_NOT_VALID;
	}

	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-framerate", &tmp);
	pinfo->mipi.frame_rate = (!rc ? tmp : 60);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-clockrate", &tmp);
	pinfo->clk_rate = (!rc ? tmp : 0);
	data = of_get_property(np, "qcom,mdss-dsi-panel-timings", &len);
	if ((!data) || (len != 12)) {
		pr_err("%s:%d, Unable to read Phy timing settings",
		       __func__, __LINE__);
		goto error;
	}
	for (i = 0; i < len; i++)
		pinfo->mipi.dsi_phy_db.timing[i] = data[i];
	/* < DTS2015072108592 tianye 20150731 begin */
	/* FPC unlock can't light lcd backlight */
	rc = of_property_read_u32(np, "qcom,mdss-dsi-post-delay-time", &tmp);
	LcdPow_DelayTime =  (!rc ? tmp : 0);
	set_lcd_power_delay_time(LcdPow_DelayTime);
	/* DTS2015072108592 tianye 20150731 end > */
	/* < DTS2015102906691 tianye 20151109 begin */
	/*open tp gesture can't wake up screen probability*/
	get_tp_reset_status = of_property_read_bool(np,"qcom,mdss-dsi-tp-reset_enable");
	set_tp_reset_status(get_tp_reset_status);
	/* DTS2015102906691 tianye 20151109 end > */
	pinfo->mipi.lp11_init = of_property_read_bool(np,
					"qcom,mdss-dsi-lp11-init");
	rc = of_property_read_u32(np, "qcom,mdss-dsi-init-delay-us", &tmp);
	pinfo->mipi.init_delay = (!rc ? tmp : 0);
/*< DTS2014103106441 huangli/hwx207935 20141031 begin*/
	#ifdef CONFIG_HUAWEI_LCD
	rc = of_property_read_u32(np, "qcom,mdss-dsi-mipi-rst-delay", &tmp);
	pinfo->mipi_rest_delay = (!rc ? tmp : 0);
	#endif
/*DTS2014103106441 huangli/hwx207935 20141031 end >*/
	mdss_dsi_parse_roi_alignment(np, pinfo);

	mdss_dsi_parse_trigger(np, &(pinfo->mipi.mdp_trigger),
		"qcom,mdss-dsi-mdp-trigger");

	mdss_dsi_parse_trigger(np, &(pinfo->mipi.dma_trigger),
		"qcom,mdss-dsi-dma-trigger");

	mdss_dsi_parse_lane_swap(np, &(pinfo->mipi.dlane_swap));

	mdss_dsi_parse_fbc_params(np, pinfo);

	mdss_panel_parse_te_params(np, pinfo);

	mdss_dsi_parse_reset_seq(np, pinfo->rst_seq, &(pinfo->rst_seq_len),
		"qcom,mdss-dsi-reset-sequence");

	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->on_cmds,
		"qcom,mdss-dsi-on-command", "qcom,mdss-dsi-on-command-state");

	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->off_cmds,
		"qcom,mdss-dsi-off-command", "qcom,mdss-dsi-off-command-state");

/* <DTS2014042409252 d00238048 20140505 begin */
#ifdef CONFIG_FB_AUTO_CABC
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->dsi_panel_cabc_ui_cmds,
		"qcom,panel-cabc-ui-cmds", "qcom,cabc-ui-cmds-dsi-state"); 
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->dsi_panel_cabc_video_cmds,
		"qcom,panel-cabc-video-cmds", "qcom,cabc-video-cmds-dsi-state");
#endif
/* DTS2014042409252 d00238048 20140505 end> */
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->status_cmds,
			"qcom,mdss-dsi-panel-status-command",
				"qcom,mdss-dsi-panel-status-command-state");
	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-status-value", &tmp);
	ctrl_pdata->status_value = (!rc ? tmp : 0);


	ctrl_pdata->status_mode = ESD_MAX;
	rc = of_property_read_string(np,
				"qcom,mdss-dsi-panel-status-check-mode", &data);
	if (!rc) {
		if (!strcmp(data, "bta_check")) {
			ctrl_pdata->status_mode = ESD_BTA;
		} else if (!strcmp(data, "reg_read")) {
			ctrl_pdata->status_mode = ESD_REG;
			ctrl_pdata->status_cmds_rlen = 1;
			ctrl_pdata->check_read_status =
						mdss_dsi_gen_read_status;
		} else if (!strcmp(data, "reg_read_nt35596")) {
			ctrl_pdata->status_mode = ESD_REG_NT35596;
			ctrl_pdata->status_error_count = 0;
			ctrl_pdata->status_cmds_rlen = 8;
			ctrl_pdata->check_read_status =
						mdss_dsi_nt35596_read_status;
		} else if (!strcmp(data, "te_signal_check")) {
			if (pinfo->mipi.mode == DSI_CMD_MODE)
				ctrl_pdata->status_mode = ESD_TE;
			else
				pr_err("TE-ESD not valid for video mode\n");
		}
	}

	rc = mdss_dsi_parse_panel_features(np, ctrl_pdata);
	if (rc) {
		pr_err("%s: failed to parse panel features\n", __func__);
		goto error;
	}
/* < DTS2015071309027 zhaihaipeng 20150713 begin */
	rc = of_property_read_u32(np, "huawei,panel-power-off-reset-flag", &tmp);
	ctrl_pdata->reset_for_pt_flag = (!rc ? tmp : 0);
/* DTS2015071309027 zhaihaipeng 20150713 end >*/
	mdss_dsi_parse_panel_horizintal_line_idle(np, ctrl_pdata);

	mdss_dsi_parse_dfps_config(np, ctrl_pdata);
/*< DTS2014052207729 renxigang 20140520 begin */
/* < DTS2014051503541 daiyuhong 20140515 begin */
/* <DTS2014051303765 daiyuhong 20140513 begin */
/* <DTS2014050904959 daiyuhong 20140509 begin */
/*< DTS2014042905347 zhaoyuxia 20140429 begin */
/* < DTS2014051403007 zhaoyuxia 20140515 begin */
/* add delaytine-before-bl flag */
/* <DTS2015010801501 sunlibin 20150108 begin */
/* Modify JDI tp/lcd power on/off to reduce power consumption */
/* < DTS2015012604763 zhengjinzeng 20150127 begin */
#ifdef CONFIG_HUAWEI_LCD
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->dot_inversion_cmds,
		"qcom,panel-dot-inversion-mode-cmds", "qcom,dot-inversion-cmds-dsi-state");
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->column_inversion_cmds,
		"qcom,panel-column-inversion-mode-cmds", "qcom,column-inversion-cmds-dsi-state");
	rc = of_property_read_u32(np, "huawei,long-read-flag", &tmp);
	ctrl_pdata->long_read_flag= (!rc ? tmp : 0);
	rc = of_property_read_u32(np, "huawei,skip-reg-read-flag", &tmp);
	ctrl_pdata->skip_reg_read= (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "huawei,reg-read-expect-value", &tmp);
	ctrl_pdata->reg_expect_value = (!rc ? tmp : 0x1c);
	/* < DTS2015070607268 zhangming 20150716 begin */
	rc = of_property_read_u32(np, "huawei,product-pad-flag", &tmp);
	ctrl_pdata->hw_product_pad = (!rc ? tmp : 0);
	/*  DTS2015070607268 zhangming 20150716 end>*/
	/* < DTS2015081903221 luozhiming 20150814 begin */
	rc = of_property_read_u32(np, "huawei,which-product-pad", &tmp);
	ctrl_pdata->which_product_pad = (!rc ? tmp : 0);
	/* < DTS2015081903221 luozhiming 20150814 end */

	rc = of_property_read_u32(np, "huawei,reg-long-read-count", &tmp);
	ctrl_pdata->reg_expect_count = (!rc ? tmp : 3);
	rc = of_property_read_u32(np, "huawei,delaytime-before-bl", &tmp);
	pinfo->delaytime_before_bl = (!rc ? tmp : 0);
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->dsi_panel_inverse_on_cmds,
		"qcom,panel-inverse-on-cmds", "qcom,inverse-on-cmds-dsi-state");
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->dsi_panel_inverse_off_cmds,
		"qcom,panel-inverse-off-cmds", "qcom,inverse-on-cmds-dsi-state");
	/*<DTS2015090700926 zhangming 20150917 begin>*/
	/*<add cabc node>*/
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->dsi_panel_cabc_off_cmds,
		"qcom,panel-cabc-off-cmds", "qcom,cabc-off-cmds-dsi-state");
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->dsi_panel_cabc_moving_cmds,
		"qcom,panel-cabc-moving-cmds", "qcom,cabc-moving-cmds-dsi-state");
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->dsi_panel_cabc_still_cmds,
		"qcom,panel-cabc-still-cmds", "qcom,cabc-still-cmds-dsi-state");
	/* < DTS2015121009904  feishilin 20151215 begin */
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->dsi_panel_cabc_ui_commands,
		"qcom,panel-cabc-ui-commands", "qcom,cabc-ui-commands-dsi-state");
	rc = of_property_read_u32(np, "huawei,cabc-default-mode", &tmp);
	pinfo->cabc_mode = (!rc ? tmp : 3); //default: moving mode;set cabc_mode=3, CABC_MOVING
	/*  DTS2015121009904  feishilin 20151215 end >*/

	/* < DTS2015122303338  feishilin 20151229 begin */
	rc = of_property_read_u32(np, "huawei,low-brightness-no-dimming", &tmp);
	ctrl_pdata->nodimming_brightnessval =  (!rc ? tmp : 0);
	/*  DTS2015122303338  feishilin 20151229 end >*/

	/*<DTS2015090700926 zhangming 20150917 end>*/
	mdss_panel_parse_esd_dt(np,ctrl_pdata);
	rc = of_property_read_u32(np, "huawei,mdss-dsi-lens-type", &tmp);
	pinfo->lens_type= (!rc ? tmp : 0);
	mdss_panel_parse_frame_checksum_dt(np,ctrl_pdata);

/* < DTS2015090808787 fwx295956 20150908 begin */
	if(ctrl_pdata->hw_product_pad){
		pinfo->support_mode |= 0x01; /*support lcd_comform_mode */
	}
/* DTS2015090808787 fwx295956 20150908 end > */

/*<  DTS2016021403416  feishilin 20160217 begin */
	rc = of_property_read_u32(np, "huawei,attenuate_blue_coff", &tmp);
	pinfo->attenuate_blue_coff =  (!rc ? tmp : 0);
/*  DTS2016021403416  feishilin 20160217 end >*/
#endif
/* DTS2015012604763 zhengjinzeng 20150127 end> */
/* DTS2015010801501 sunlibin 20150108 end> */
/* DTS2014051403007 zhaoyuxia 20140515 end > */
/* DTS2014042905347 zhaoyuxia 20140429 end > */
/* DTS2014050904959 daiyuhong 20140509 end > */
/* DTS2014051303765 daiyuhong 20140513 end> */
/* DTS2014051503541 daiyuhong 20140515 end > */
/* DTS2014052207729 renxigang 20140520 end >*/
	return 0;

error:
	return -EINVAL;
}

int mdss_dsi_panel_init(struct device_node *node,
	struct mdss_dsi_ctrl_pdata *ctrl_pdata,
	bool cmd_cfg_cont_splash)
{
	int rc = 0;
	static const char *panel_name;
	struct mdss_panel_info *pinfo;
/* <DTS2014042405897 d00238048 20140425 begin */
	static const char *info_node = "lcd type";
/* DTS2014042405897 d00238048 20140425 end> */

	if (!node || !ctrl_pdata) {
		pr_err("%s: Invalid arguments\n", __func__);
		return -ENODEV;
	}

	pinfo = &ctrl_pdata->panel_data.panel_info;

	pr_debug("%s:%d\n", __func__, __LINE__);
	pinfo->panel_name[0] = '\0';
	panel_name = of_get_property(node, "qcom,mdss-dsi-panel-name", NULL);
	if (!panel_name) {
		pr_info("%s:%d, Panel name not specified\n",
						__func__, __LINE__);
	} else {
		pr_info("%s: Panel Name = %s\n", __func__, panel_name);
		strlcpy(&pinfo->panel_name[0], panel_name, MDSS_MAX_PANEL_LEN);
	}
/* <DTS2014042409252 d00238048 20140505 begin */
#ifdef CONFIG_HUAWEI_LCD
/* <DTS2014042405897 d00238048 20140425 begin */
	rc = app_info_set(info_node, panel_name);
/* DTS2014042405897 d00238048 20140425 end> */
#endif
/* DTS2014042409252 d00238048 20140505 end> */
	rc = mdss_panel_parse_dt(node, ctrl_pdata);
	if (rc) {
		pr_err("%s:%d panel dt parse failed\n", __func__, __LINE__);
		return rc;
	}

	if (!cmd_cfg_cont_splash)
		pinfo->cont_splash_enabled = false;
	pr_info("%s: Continuous splash %s\n", __func__,
		pinfo->cont_splash_enabled ? "enabled" : "disabled");

	pinfo->dynamic_switch_pending = false;
	pinfo->is_lpm_mode = false;
	pinfo->esd_rdy = false;

	ctrl_pdata->on = mdss_dsi_panel_on;
	ctrl_pdata->off = mdss_dsi_panel_off;
	ctrl_pdata->low_power_config = mdss_dsi_panel_low_power_config;
	ctrl_pdata->panel_data.set_backlight = mdss_dsi_panel_bl_ctrl;
/* < DTS2014051503541 daiyuhong 20140515 begin */
/*< DTS2014042905347 zhaoyuxia 20140429 begin */
/*< DTS2014042818102 zhengjinzeng 20140428 begin */
/* < DTS2015012604763 zhengjinzeng 20150127 begin */
/*<DTS2015090700926 zhangming 20150917 begin>*/
#ifdef CONFIG_HUAWEI_LCD
	ctrl_pdata->panel_data.set_inversion_mode = mdss_dsi_panel_inversion_ctrl;
	ctrl_pdata->panel_data.check_panel_status= mdss_dsi_check_panel_status;
	ctrl_pdata->panel_data.lcd_set_display_inversion = mdss_dsi_lcd_set_display_inversion;
	ctrl_pdata->panel_data.lcd_set_cabc_mode = mdss_dsi_lcd_set_cabc_mode;
	ctrl_pdata->panel_data.check_panel_mipi_crc = mdss_dsi_check_mipi_crc;
	ctrl_pdata->panel_data.panel_frame_checksum = mdss_lcd_frame_checksum;
#endif
/*<DTS2015090700926 zhangming 20150917 end>*/
/* DTS2015012604763 zhengjinzeng 20150127 end> */
/* DTS2014042818102 zhengjinzeng 20140428 end >*/
/* DTS2014042905347 zhaoyuxia 20140429 end >*/
/* DTS2014051503541 daiyuhong 20140515 end > */
/* <DTS2014042409252 d00238048 20140505 begin */
#ifdef CONFIG_FB_AUTO_CABC
	ctrl_pdata->panel_data.config_cabc = mdss_dsi_panel_cabc_ctrl;
#endif
/* DTS2014042409252 d00238048 20140505 end> */

/* < DTS2014050808504 daiyuhong 20140508 begin */
/*save ctrl_pdata */
#if defined(CONFIG_HUAWEI_KERNEL) && defined(CONFIG_DEBUG_FS)
	lcd_dbg_set_dsi_ctrl_pdata(ctrl_pdata);
#endif
/* DTS2014050808504 daiyuhong 20140508 end > */
	ctrl_pdata->switch_mode = mdss_dsi_panel_switch_mode;

	return 0;
}
/* DTS2015082807971  liyunlong 20150829 end > */
