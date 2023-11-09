/*
 * LED Kernel pulse Trigger
 *
 * Copyright (C) 2016 Yellow Huang <yelow.huang@cybertan.com.tw>
 *
 * Based on Shuah Khan's ledtrig-transient.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
/*
 * pulse trigger allows level changable activation. Please refer to
 * Documentation/leds/ledtrig-pulse.txt for details
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/leds.h>
#include "../leds.h"

#ifdef DEBUG
#define my_debug(fmt, ...) if (printk_ratelimit()) printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#else
#define my_debug(fmt, ...)
#endif

struct pulse_trig_data {
	int first_down;
	int active;
	int min;
	int max;
	int step_up;
	int step_down;
	unsigned long delay_up;
	unsigned long delay_down;

	int isdown;
	struct timer_list timer;
};

static void pulse_timer_function(unsigned long data)
{
	struct led_classdev *led_cdev = (struct led_classdev *) data;
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;
	unsigned long duration;

	__led_set_brightness(led_cdev, pulse_data->active);

	if (pulse_data->isdown) {
		if (pulse_data->active != pulse_data->min) {
			pulse_data->active -= pulse_data->step_down;
		}

		if (pulse_data->active <= pulse_data->min) {
			pulse_data->active = pulse_data->min;
			pulse_data->isdown = 0;
		}
		duration = pulse_data->delay_down;
	}
	else {
		if (pulse_data->active != pulse_data->max) {
			pulse_data->active += pulse_data->step_up;
		}

		if (pulse_data->active >= pulse_data->max) {
			pulse_data->active = pulse_data->max;
			pulse_data->isdown = 1;
		}
		duration = pulse_data->delay_up;
	}
	my_debug("%s: isdown[%d] active[%u] duration[%lu]\n", __func__,
			pulse_data->isdown,
			pulse_data->active, duration);

	if (duration)
		mod_timer(&pulse_data->timer, jiffies + duration);

	return;
}

static ssize_t pulse_max_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;

	return sprintf(buf, "%d\n", pulse_data->max);
}

static ssize_t pulse_max_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;
	unsigned long max;
	ssize_t ret;

	ret = kstrtoul(buf, 10, &max);
	if (ret)
		return ret;

	if ((max < LED_OFF) || (max >LED_FULL))
		return -EINVAL;

	if (max < pulse_data->min)
		return -EINVAL;

	pulse_data->max = max;
	return size;
}

static ssize_t pulse_min_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;

	return sprintf(buf, "%d\n", pulse_data->min);
}

static ssize_t pulse_min_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;
	unsigned long min;
	ssize_t ret;

	ret = kstrtoul(buf, 10, &min);
	if (ret)
		return ret;

	if ((min < LED_OFF) || (min >LED_FULL))
		return -EINVAL;

	if (min > pulse_data->max)
		return -EINVAL;

	pulse_data->min = min;
	return size;
}

static ssize_t pulse_step_up_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;

	return sprintf(buf, "%d\n", pulse_data->step_up);
}

static ssize_t pulse_step_up_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;
	unsigned long step_up;
	ssize_t ret;

	ret = kstrtoul(buf, 10, &step_up);
	if (ret)
		return ret;

	if (step_up > (pulse_data->max - pulse_data->min))
		return -EINVAL;

	pulse_data->step_up = step_up;
	mod_timer(&pulse_data->timer, jiffies + 10);
	return size;
}

static ssize_t pulse_delay_up_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;

	return sprintf(buf, "%lu\n", pulse_data->delay_up);
}

static ssize_t pulse_delay_up_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;
	unsigned long delay_up;
	ssize_t ret;

	ret = kstrtoul(buf, 10, &delay_up);
	if (ret)
		return ret;

	pulse_data->delay_up = delay_up;
	mod_timer(&pulse_data->timer, jiffies + 10);
	return size;
}

static ssize_t pulse_step_down_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;

	return sprintf(buf, "%d\n", pulse_data->step_down);
}

static ssize_t pulse_step_down_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;
	unsigned long step_down;
	ssize_t ret;

	ret = kstrtoul(buf, 10, &step_down);
	if (ret)
		return ret;

	if (step_down > (pulse_data->max - pulse_data->min))
		return -EINVAL;

	pulse_data->step_down = step_down;
	mod_timer(&pulse_data->timer, jiffies + 10);
	return size;
}

static ssize_t pulse_delay_down_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;

	return sprintf(buf, "%lu\n", pulse_data->delay_down);
}

static ssize_t pulse_delay_down_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;
	unsigned long delay_down;
	ssize_t ret;

	ret = kstrtoul(buf, 10, &delay_down);
	if (ret)
		return ret;

	pulse_data->delay_down = delay_down;
	mod_timer(&pulse_data->timer, jiffies + 10);
	return size;
}

static ssize_t pulse_first_down_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;

	return sprintf(buf, "%d\n", pulse_data->first_down);
}

static ssize_t pulse_first_down_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;
	unsigned long first_down;
	ssize_t ret;

	ret = kstrtoul(buf, 10, &first_down);
	if (ret)
		return ret;

	pulse_data->isdown = pulse_data->first_down = first_down;
	mod_timer(&pulse_data->timer, jiffies + 10);
	return size;
}

static ssize_t pulse_active_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;

	return sprintf(buf, "%d\n", pulse_data->active);
}

static ssize_t pulse_active_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;
	unsigned long active;
	ssize_t ret;

	ret = kstrtoul(buf, 10, &active);
	if (ret)
		return ret;

	pulse_data->active = active;
	mod_timer(&pulse_data->timer, jiffies + 10);
	return size;
}

static DEVICE_ATTR(first_down, 0644, pulse_first_down_show, pulse_first_down_store);
static DEVICE_ATTR(active, 0644, pulse_active_show, pulse_active_store);
static DEVICE_ATTR(min, 0644, pulse_min_show, pulse_min_store);
static DEVICE_ATTR(max, 0644, pulse_max_show, pulse_max_store);
static DEVICE_ATTR(step_up, 0644, pulse_step_up_show, pulse_step_up_store);
static DEVICE_ATTR(delay_up, 0644, pulse_delay_up_show, pulse_delay_up_store);
static DEVICE_ATTR(step_down, 0644, pulse_step_down_show, pulse_step_down_store);
static DEVICE_ATTR(delay_down, 0644, pulse_delay_down_show, pulse_delay_down_store);

static void init_pulse_trig_data(struct pulse_trig_data *tdata)
{
	tdata->first_down = 1;
	tdata->active = tdata->max = LED_FULL;
	tdata->min = LED_OFF;
	tdata->step_up = 5;
	tdata->delay_up = 3;
	tdata->step_down = 5;
	tdata->delay_down = 3;

	tdata->isdown = tdata->first_down;
	return;
}

static void pulse_trig_activate(struct led_classdev *led_cdev)
{
	int rc;
	struct pulse_trig_data *tdata;

	tdata = kzalloc(sizeof(struct pulse_trig_data), GFP_KERNEL);
	if (!tdata) {
		dev_err(led_cdev->dev,
			"unable to allocate pulse trigger\n");
		return;
	}
	led_cdev->trigger_data = tdata;
	init_pulse_trig_data(tdata);

	rc = device_create_file(led_cdev->dev, &dev_attr_first_down);
	if (rc)
		goto err_out;

	rc = device_create_file(led_cdev->dev, &dev_attr_active);
	if (rc)
		goto err_out_active;

	rc = device_create_file(led_cdev->dev, &dev_attr_min);
	if (rc)
		goto err_out_min;

	rc = device_create_file(led_cdev->dev, &dev_attr_max);
	if (rc)
		goto err_out_max;

	rc = device_create_file(led_cdev->dev, &dev_attr_step_up);
	if (rc)
		goto err_out_step_up;

	rc = device_create_file(led_cdev->dev, &dev_attr_delay_up);
	if (rc)
		goto err_out_delay_up;

	rc = device_create_file(led_cdev->dev, &dev_attr_step_down);
	if (rc)
		goto err_out_step_down;

	rc = device_create_file(led_cdev->dev, &dev_attr_delay_down);
	if (rc)
		goto err_out_delay_down;

	setup_timer(&tdata->timer, pulse_timer_function,
		    (unsigned long) led_cdev);
	led_cdev->activated = true;
	pulse_timer_function((unsigned long) led_cdev);

	return;

err_out_delay_down:
	device_remove_file(led_cdev->dev, &dev_attr_step_down);
err_out_step_down:
	device_remove_file(led_cdev->dev, &dev_attr_delay_up);
err_out_delay_up:
	device_remove_file(led_cdev->dev, &dev_attr_step_up);
err_out_step_up:
	device_remove_file(led_cdev->dev, &dev_attr_max);
err_out_max:
	device_remove_file(led_cdev->dev, &dev_attr_min);
err_out_min:
	device_remove_file(led_cdev->dev, &dev_attr_active);
err_out_active:
	device_remove_file(led_cdev->dev, &dev_attr_first_down);
err_out:
	dev_err(led_cdev->dev, "unable to register pulse trigger\n");
	led_cdev->trigger_data = NULL;
	kfree(tdata);
}

static void pulse_trig_deactivate(struct led_classdev *led_cdev)
{
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;

	if (led_cdev->activated) {
		del_timer_sync(&pulse_data->timer);
		device_remove_file(led_cdev->dev, &dev_attr_first_down);
		device_remove_file(led_cdev->dev, &dev_attr_active);
		device_remove_file(led_cdev->dev, &dev_attr_min);
		device_remove_file(led_cdev->dev, &dev_attr_max);
		device_remove_file(led_cdev->dev, &dev_attr_step_up);
		device_remove_file(led_cdev->dev, &dev_attr_delay_up);
		device_remove_file(led_cdev->dev, &dev_attr_step_down);
		device_remove_file(led_cdev->dev, &dev_attr_delay_down);

		led_cdev->trigger_data = NULL;
		led_cdev->activated = false;
		kfree(pulse_data);
	}
}

static struct led_trigger pulse_trigger = {
	.name     = "pulse",
	.activate = pulse_trig_activate,
	.deactivate = pulse_trig_deactivate,
};

static int __init pulse_trig_init(void)
{
	return led_trigger_register(&pulse_trigger);
}

static void __exit pulse_trig_exit(void)
{
	led_trigger_unregister(&pulse_trigger);
}

module_init(pulse_trig_init);
module_exit(pulse_trig_exit);

MODULE_AUTHOR("Yellow Huang <yellow.huang@cybertan.com.tw>");
MODULE_DESCRIPTION("pulse LED trigger");
MODULE_LICENSE("GPL");
