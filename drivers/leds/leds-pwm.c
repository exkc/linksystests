/*
 * linux/drivers/leds-pwm.c
 *
 * simple PWM based LED control
 *
 * Copyright 2009 Luotao Fu @ Pengutronix (l.fu@pengutronix.de)
 *
 * based on leds-gpio.c by Raphael Assenat <raph@8d.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/fb.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/leds_pwm.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

struct led_pwm_priv;
extern int led_custom_set;

struct led_pwm_data {
	struct led_classdev	cdev;
	struct pwm_device	*pwm;
	struct work_struct	work;
	struct led_pwm_priv *priv;
	unsigned int		active_low;
	unsigned int		period;
	int			duty;
	bool			can_sleep;
};

struct led_pwm_priv {
	int num_leds;
	enum led_brightness brightness;
	struct led_pwm_data leds[0];
};

static void __led_pwm_set(struct led_pwm_data *led_dat)
{
	int new_duty = led_dat->duty;

	pwm_config(led_dat->pwm, new_duty, led_dat->period);

	if (new_duty == 0)
		pwm_disable(led_dat->pwm);
	else
		pwm_enable(led_dat->pwm);
}

static void led_pwm_work(struct work_struct *work)
{
	struct led_pwm_data *led_dat =
		container_of(work, struct led_pwm_data, work);

	__led_pwm_set(led_dat);
}

static void led_pwm_set(struct led_classdev *led_cdev,
	enum led_brightness brightness)
{
	struct led_pwm_data *led_dat =
		container_of(led_cdev, struct led_pwm_data, cdev);
	unsigned int max = led_dat->cdev.max_brightness;
	unsigned long long duty =  led_dat->period;

	duty *= brightness;
	do_div(duty, max);

	if (led_dat->active_low)
		duty = led_dat->period - duty;

	led_dat->duty = duty;

	if (led_dat->can_sleep)
		schedule_work(&led_dat->work);
	else
		__led_pwm_set(led_dat);
}

static inline size_t sizeof_pwm_leds_priv(int num_leds)
{
	return sizeof(struct led_pwm_priv) +
		      (sizeof(struct led_pwm_data) * num_leds);
}

static void led_pwm_cleanup(struct led_pwm_priv *priv)
{
	while (priv->num_leds--) {
		led_classdev_unregister(&priv->leds[priv->num_leds].cdev);
		if (priv->leds[priv->num_leds].can_sleep)
			cancel_work_sync(&priv->leds[priv->num_leds].work);
	}
}

static int led_pwm_add(struct device *dev, struct led_pwm_priv *priv,
		       struct led_pwm *led, struct device_node *child)
{
	struct led_pwm_data *led_data = &priv->leds[priv->num_leds];
	int ret;

	led_data->priv = priv;
	led_data->active_low = led->active_low;
	led_data->cdev.name = led->name;
	led_data->cdev.default_trigger = led->default_trigger;
	led_data->cdev.brightness_set = led_pwm_set;
	led_data->cdev.brightness = LED_OFF;
	led_data->cdev.max_brightness = led->max_brightness;
	led_data->cdev.flags = LED_CORE_SUSPENDRESUME;

	if (child)
		led_data->pwm = devm_of_pwm_get(dev, child, NULL);
	else
		led_data->pwm = devm_pwm_get(dev, led->name);
	if (IS_ERR(led_data->pwm)) {
		ret = PTR_ERR(led_data->pwm);
		dev_err(dev, "unable to request PWM for %s: %d\n",
			led->name, ret);
		return ret;
	}

	led_data->can_sleep = pwm_can_sleep(led_data->pwm);
	if (led_data->can_sleep)
		INIT_WORK(&led_data->work, led_pwm_work);

	led_data->period = pwm_get_period(led_data->pwm);
	if (!led_data->period && (led->pwm_period_ns > 0))
		led_data->period = led->pwm_period_ns;

	ret = led_classdev_register(dev, &led_data->cdev);
	if (ret == 0) {
		priv->num_leds++;
	} else {
		dev_err(dev, "failed to register PWM led for %s: %d\n",
			led->name, ret);
	}

	return ret;
}

static int led_pwm_create_of(struct device *dev, struct led_pwm_priv *priv)
{
	struct device_node *child;
	struct led_pwm led;
	int ret = 0;

	memset(&led, 0, sizeof(led));

	for_each_child_of_node(dev->of_node, child) {
		led.name = of_get_property(child, "label", NULL) ? :
			   child->name;

		led.default_trigger = of_get_property(child,
						"linux,default-trigger", NULL);
		led.active_low = of_property_read_bool(child, "active-low");
		of_property_read_u32(child, "max-brightness",
				     &led.max_brightness);

		ret = led_pwm_add(dev, priv, &led, child);
		if (ret) {
			of_node_put(child);
			break;
		}
	}

	return ret;
}

#define RED	0
#define GREEN	1
#define BLUE	2


static void rgb_bright_set(struct led_classdev *led_cdev, enum led_brightness value)
{
	struct led_pwm_data *led_data = container_of(led_cdev, struct led_pwm_data, cdev);
	struct led_pwm_priv *priv = led_data->priv;
	enum led_brightness red, green, blue;
	
	priv->brightness = value;

	blue = 0x0000ff & value;
	green = (0x00ff00 & value) >> 8;
	red = (0xff0000 & value) >> 16;

	priv->leds[RED].cdev.brightness_set(&priv->leds[RED].cdev, red);
	priv->leds[GREEN].cdev.brightness_set(&priv->leds[GREEN].cdev, green);
	priv->leds[BLUE].cdev.brightness_set(&priv->leds[BLUE].cdev, blue);

	return;
}

static void purple_bright_set(struct led_classdev *led_cdev, enum led_brightness value)
{
	enum led_brightness rgb;

	rgb = value << 16 | LED_OFF << 8 | value; 
	rgb_bright_set(led_cdev, rgb); 
	return;
}

static void white_bright_set(struct led_classdev *led_cdev, enum led_brightness value)
{
	enum led_brightness rgb;

	rgb = value << 16 | value << 8 | value; 
	rgb_bright_set(led_cdev, rgb); 
	return;
}

static void cyan_bright_set(struct led_classdev *led_cdev, enum led_brightness value)
{
	enum led_brightness rgb;

	rgb = LED_OFF << 16 | value << 8 | value; 
	rgb_bright_set(led_cdev, rgb); 
	return;
}

static void yellow_bright_set(struct led_classdev *led_cdev, enum led_brightness value)
{
	enum led_brightness rgb;

	rgb = value << 16 | value << 8 | LED_OFF; 
	rgb_bright_set(led_cdev, rgb); 
	return;
}

static enum led_brightness rgb_bright_get(struct led_classdev *led_cdev)
{
	struct led_pwm_data *led_data = container_of(led_cdev, struct led_pwm_data, cdev);
	struct led_pwm_priv *priv = led_data->priv;

	return priv->brightness;
}	

static void led_dummy_work(struct work_struct *work)
{
	printk(KERN_EMERG "dummy_work called\n");
	return;
}

void rgb_custom_set(struct led_classdev *led_cdev, int value)
{
	if (value == 0)
		dev_info(led_cdev->dev, "RGB custom stop\n");
	else 
		dev_info(led_cdev->dev, "RGB custom start\n");

	led_custom_set = value ?1 :0;
	return;
}
EXPORT_SYMBOL_GPL(rgb_custom_set);

struct fake_led {
	char *name;
	char *default_trigger;
	enum led_brightness max_brightness; 
	
	void (*leds_work)(struct work_struct *work);
	void (*bright_set)(struct led_classdev *led_cdev, enum led_brightness value);
	enum led_brightness (*bright_get)(struct led_classdev *led_cdev);
} fake_leds[] = {
	{
		.name = "pwm:purple",
		.default_trigger = "none",
		.leds_work = led_dummy_work,
		.bright_set = purple_bright_set,
	},
	{
		.name = "pwm:white",
		.default_trigger = "none",
		.leds_work = led_dummy_work,
		.bright_set = white_bright_set,
	},
	{
		.name = "pwm:cyan",
		.default_trigger = "none",
		.leds_work = NULL,
		.bright_set = cyan_bright_set,
	},
	{
		.name = "pwm:yellow",
		.default_trigger = "none",
		.leds_work = led_dummy_work,
		.bright_set = yellow_bright_set,
	}, 
	{
		.name = "pwm:rgb",
		.default_trigger = "none",
		.leds_work = led_dummy_work,
		.bright_set = rgb_bright_set,
		.bright_get = rgb_bright_get,
		.max_brightness = 0xFFFFFF,
	},
};

static int probe_fake_leds(struct device *dev, struct led_pwm_priv *priv)
{
	int i, ret;
	int fake_leds_num = sizeof(fake_leds)/sizeof(struct fake_led);
	
	for (i = 0; i < fake_leds_num; i++) {
		priv->leds[priv->num_leds+i].priv = priv;
		priv->leds[priv->num_leds+i].cdev.name =fake_leds[i].name;
		priv->leds[priv->num_leds+i].cdev.default_trigger = fake_leds[i].default_trigger;	
		priv->leds[priv->num_leds+i].cdev.brightness_set = fake_leds[i].bright_set;

		if (fake_leds[i].bright_get)
			priv->leds[priv->num_leds+i].cdev.brightness_get = fake_leds[i].bright_get;

		if (fake_leds[i].max_brightness)
			priv->leds[priv->num_leds+i].cdev.max_brightness = fake_leds[i].max_brightness;

		//if (fake_leds[i].max_brightness)
		//	priv->leds[priv->num_leds+i].cdev.max_brightness = fake_leds[i].max_brightness;
	printk(KERN_EMERG "register  %s\n", priv->leds[priv->num_leds+i].cdev.name);

		ret = led_classdev_register(dev, &priv->leds[priv->num_leds+i].cdev);
		if (ret != 0) {
			dev_err(dev, "failed to register PWM led for %s: %d\n",
				priv->leds[priv->num_leds+i].cdev.name, ret);
				goto exit;
		}
	}

	priv->num_leds +=  fake_leds_num;
	return 0;
	
exit:
	while (i--) {
		led_classdev_unregister(&priv->leds[priv->num_leds+i].cdev);
	}

	return ret;
}

static int led_pwm_probe(struct platform_device *pdev)
{
	struct led_pwm_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct led_pwm_priv *priv;
	int count, i;
	int ret = 0;
	int fake_leds_num = sizeof(fake_leds)/sizeof(struct fake_led);

	if (pdata)
		count = pdata->num_leds;
	else
		count = of_get_child_count(pdev->dev.of_node);

	if (!count)
		return -EINVAL;

	priv = devm_kzalloc(&pdev->dev, sizeof_pwm_leds_priv(count + fake_leds_num),
			    GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	if (pdata) {
		for (i = 0; i < count; i++) {
			ret = led_pwm_add(&pdev->dev, priv, &pdata->leds[i],
					  NULL);
			if (ret)
				break;
		}
	} else {
		ret = led_pwm_create_of(&pdev->dev, priv);
	}

	if (ret) {
		led_pwm_cleanup(priv);
		return ret;
	}

	probe_fake_leds(&pdev->dev, priv);
	printk(KERN_EMERG "register  %d leds\n", count + fake_leds_num);

	platform_set_drvdata(pdev, priv);

	return 0;
}

static int led_pwm_remove(struct platform_device *pdev)
{
	struct led_pwm_priv *priv = platform_get_drvdata(pdev);

	led_pwm_cleanup(priv);

	return 0;
}

static const struct of_device_id of_pwm_leds_match[] = {
	{ .compatible = "pwm-leds", },
	{},
};
MODULE_DEVICE_TABLE(of, of_pwm_leds_match);

static struct platform_driver led_pwm_driver = {
	.probe		= led_pwm_probe,
	.remove		= led_pwm_remove,
	.driver		= {
		.name	= "leds_pwm",
		.of_match_table = of_pwm_leds_match,
	},
};

module_platform_driver(led_pwm_driver);

MODULE_AUTHOR("Luotao Fu <l.fu@pengutronix.de>");
MODULE_DESCRIPTION("PWM LED driver for PXA");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:leds-pwm");
