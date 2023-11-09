/* Modifications were made by Linksys on or before Thu Dec 15 19:44:28 PST 2016 */
/* LED Kernel pulse Trigger
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

extern void rgb_custom_set(struct led_classdev *led_cdev, int value);

struct pulse_trig_data {
	int first_down;
	int start;
	int min_red, min_green, min_blue;
	int max_red, max_green, max_blue;
	int stop_on_min, stop_on_max;

	int step_up;
	int step_down;
	long unsigned int delay_up, delay_down;

	int active_red, active_green, active_blue;
	int isdown;

	struct timer_list timer;
};

static inline void pulse_set_brightness(struct led_classdev *led_cdev,
					enum led_brightness value)
{
	led_cdev->brightness = value;
	if (!(led_cdev->flags & LED_SUSPENDED))
		led_cdev->brightness_set(led_cdev, value);
}

static void pulse_timer_function(unsigned long data)
{
	struct led_classdev *led_cdev = (struct led_classdev *) data;
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;
	unsigned long duration = 0;
	int active;

	if (!pulse_data->start) {
		return;
	}

	if (pulse_data->isdown) {
		if (pulse_data->active_red != pulse_data->min_red) {

			if (pulse_data->active_red >= pulse_data->active_green &&
					pulse_data->active_red >= pulse_data->active_blue ) {
				pulse_data->active_red -= pulse_data->step_down;
			}

			if (pulse_data->active_red <= pulse_data->min_red) {
				pulse_data->active_red = pulse_data->min_red;
			}
		}

		if (pulse_data->active_green != pulse_data->min_green) {

			if (pulse_data->active_green >= pulse_data->active_red &&
					pulse_data->active_green >= pulse_data->active_blue ) {
				pulse_data->active_green -= pulse_data->step_down;
			}

			if (pulse_data->active_green <= pulse_data->min_green) {
				pulse_data->active_green = pulse_data->min_green;
			}
		}

		if (pulse_data->active_blue != pulse_data->min_blue) {

			if (pulse_data->active_blue >= pulse_data->active_red &&
					pulse_data->active_blue >= pulse_data->active_green ) {
				pulse_data->active_blue -= pulse_data->step_down;
			}

			if (pulse_data->active_blue <= pulse_data->min_blue) {
				pulse_data->active_blue = pulse_data->min_blue;
			}
		}

		duration = pulse_data->delay_down;

		if ((pulse_data->active_red == pulse_data->min_red) &&
				(pulse_data->active_green == pulse_data->min_green) &&
				(pulse_data->active_blue == pulse_data->min_blue)) {
			pulse_data->isdown = 0;

			if (pulse_data->stop_on_min) {
				duration = 0;
			}
		}

	}
	else {
		if (pulse_data->active_red != pulse_data->max_red) {
			pulse_data->active_red += pulse_data->step_up;

			if (pulse_data->active_red >= pulse_data->max_red) {
				pulse_data->active_red = pulse_data->max_red;
			}
		}

		if (pulse_data->active_green != pulse_data->max_green) {
			pulse_data->active_green += pulse_data->step_up;

			if (pulse_data->active_green >= pulse_data->max_green) {
				pulse_data->active_green = pulse_data->max_green;
			}
		}

		if (pulse_data->active_blue != pulse_data->max_blue) {
			pulse_data->active_blue += pulse_data->step_up;

			if (pulse_data->active_blue >= pulse_data->max_blue) {
				pulse_data->active_blue = pulse_data->max_blue;
			}
		}

		duration = pulse_data->delay_up;
		if ((pulse_data->active_red == pulse_data->max_red) &&
				(pulse_data->active_green == pulse_data->max_green) &&
				(pulse_data->active_blue == pulse_data->max_blue)) {
			pulse_data->isdown = 1;

			if (pulse_data->stop_on_max) {
				duration = 0;
			}
		}
	}

	active = (((pulse_data->active_red << 8) + pulse_data->active_green) << 8) +
		pulse_data->active_blue;

	pulse_set_brightness(led_cdev, active);

	dev_dbg(led_cdev->dev, "%s: isdown[%d] active[0x%x] duration[%lu]\n", __func__,
			pulse_data->isdown,
			active, duration);

	if (duration)
		mod_timer(&pulse_data->timer, jiffies + duration);

	return;
}

static ssize_t pulse_min_show(int min, char *buf)
{
	return sprintf(buf, "%d\n", min);
}

static ssize_t pulse_min_store(int *min_pos, const char *buf)
{
	unsigned long min;
	ssize_t ret;

	ret = kstrtoul(buf, 10, &min);
	if (ret)
		return ret;

	if ((min < LED_OFF) || (min >LED_FULL))
		return -EINVAL;

	*min_pos = min;
	return ret;
}

static void restart_timer(struct pulse_trig_data *pulse_data)
{
	unsigned long duration = pulse_data->isdown ?
		pulse_data->delay_down : pulse_data->delay_up;

	if (pulse_data->start) {
		mod_timer(&pulse_data->timer, jiffies + duration);
	}
	return;
}

static ssize_t pulse_min_red_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;

	return pulse_min_show(pulse_data->min_red, buf);
}

static ssize_t pulse_min_red_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;

	pulse_min_store(&pulse_data->min_red, buf);
	restart_timer(pulse_data);
	return size;
}

static ssize_t pulse_min_green_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;

	return pulse_min_show(pulse_data->min_green, buf);
}

static ssize_t pulse_min_green_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;

	pulse_min_store(&pulse_data->min_green, buf);
	restart_timer(pulse_data);
	return size;
}

static ssize_t pulse_min_blue_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;

	return pulse_min_show(pulse_data->min_blue, buf);
}

static ssize_t pulse_min_blue_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;

	pulse_min_store(&pulse_data->min_blue, buf);
	restart_timer(pulse_data);
	return size;
}

static ssize_t pulse_max_red_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;

	return pulse_min_show(pulse_data->max_red, buf);
}

static ssize_t pulse_max_red_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;

	pulse_min_store(&pulse_data->max_red, buf);
	restart_timer(pulse_data);
	return size;
}

static ssize_t pulse_max_green_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;

	return pulse_min_show(pulse_data->max_green, buf);
}

static ssize_t pulse_max_green_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;

	pulse_min_store(&pulse_data->max_green, buf);
	restart_timer(pulse_data);
	return size;
}

static ssize_t pulse_max_blue_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;

	return pulse_min_show(pulse_data->max_blue, buf);
}

static ssize_t pulse_max_blue_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;

	pulse_min_store(&pulse_data->max_blue, buf);
	restart_timer(pulse_data);
	return size;
}

static ssize_t pulse_stop_on_min_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;

	return pulse_min_show(pulse_data->stop_on_min, buf);
}

static ssize_t pulse_stop_on_min_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;

	pulse_min_store(&pulse_data->stop_on_min, buf);
	restart_timer(pulse_data);
	return size;
}

static ssize_t pulse_stop_on_max_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;

	return pulse_min_show(pulse_data->stop_on_max, buf);
}

static ssize_t pulse_stop_on_max_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;

	pulse_min_store(&pulse_data->stop_on_max, buf);
	restart_timer(pulse_data);
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
	restart_timer(pulse_data);
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

	pulse_data->step_up = step_up;
	restart_timer(pulse_data);
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

	pulse_data->step_down = step_down;
	restart_timer(pulse_data);
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
	restart_timer(pulse_data);
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
	restart_timer(pulse_data);
	return size;
}

static ssize_t pulse_start_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;
	return sprintf(buf, "%d\n", pulse_data->start);
}

static inline int pulse_get_brightness(struct led_classdev *led_cdev)
{
	return led_cdev->brightness_get(led_cdev);
}

static ssize_t pulse_start_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;
	unsigned long start;
	ssize_t ret;

	ret = kstrtoul(buf, 10, &start);
	if (ret)
		return ret;
	pulse_data->start = start;
	if (pulse_data->start) {
		unsigned long duration = pulse_data->isdown ?
			pulse_data->delay_down : pulse_data->delay_up;
		int active = pulse_get_brightness(led_cdev);
		pulse_data->active_blue = 0x0000ff & active;
		pulse_data->active_green = (0x00ff00 & active) >> 8;
		pulse_data->active_red = (0xff0000 & active) >> 16;
		dev_dbg(dev, "active red(%d) green(%d) blue(%d) %x\n",
				pulse_data->active_red,
				pulse_data->active_green,
				pulse_data->active_blue,
				active);
	
		mod_timer(&pulse_data->timer, jiffies + duration);
		rgb_custom_set(led_cdev, 1);
	}
	else {
		del_timer_sync(&pulse_data->timer);
		rgb_custom_set(led_cdev, 0);
	}

	return size;
}

static DEVICE_ATTR(first_down, 0644, pulse_first_down_show, pulse_first_down_store);
static DEVICE_ATTR(start, 0644, pulse_start_show, pulse_start_store);
static DEVICE_ATTR(min_red, 0644, pulse_min_red_show, pulse_min_red_store);
static DEVICE_ATTR(min_green, 0644, pulse_min_green_show, pulse_min_green_store);
static DEVICE_ATTR(min_blue, 0644, pulse_min_blue_show, pulse_min_blue_store);
static DEVICE_ATTR(max_red, 0644, pulse_max_red_show, pulse_max_red_store);
static DEVICE_ATTR(max_green, 0644, pulse_max_green_show, pulse_max_green_store);
static DEVICE_ATTR(max_blue, 0644, pulse_max_blue_show, pulse_max_blue_store);
static DEVICE_ATTR(stop_on_min, 0644, pulse_stop_on_min_show, pulse_stop_on_min_store);
static DEVICE_ATTR(stop_on_max, 0644, pulse_stop_on_max_show, pulse_stop_on_max_store);
static DEVICE_ATTR(step_up, 0644, pulse_step_up_show, pulse_step_up_store);
static DEVICE_ATTR(step_down, 0644, pulse_step_down_show, pulse_step_down_store);
static DEVICE_ATTR(delay_up, 0644, pulse_delay_up_show, pulse_delay_up_store);
static DEVICE_ATTR(delay_down, 0644, pulse_delay_down_show, pulse_delay_down_store);

static void init_pulse_trig_data(struct pulse_trig_data *tdata)
{
	tdata->isdown = tdata->first_down = 1;
	tdata->start = 0;
	tdata->min_red = tdata->min_green = tdata->min_blue = LED_OFF;
	tdata->max_red = tdata->max_green = tdata->max_blue = LED_FULL;

	tdata->step_up = 5;
	tdata->delay_up = 3;
	tdata->step_down = 5;
	tdata->delay_down = 3;

	return;
}

static void custompulse_trig_activate(struct led_classdev *led_cdev)
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

	rc = device_create_file(led_cdev->dev, &dev_attr_start);
	if (rc)
		goto err_out_start;

	rc = device_create_file(led_cdev->dev, &dev_attr_min_red);
	if (rc)
		goto err_out_min_red;

	rc = device_create_file(led_cdev->dev, &dev_attr_min_green);
	if (rc)
		goto err_out_min_green;

	rc = device_create_file(led_cdev->dev, &dev_attr_min_blue);
	if (rc)
		goto err_out_min_blue;

	rc = device_create_file(led_cdev->dev, &dev_attr_max_red);
	if (rc)
		goto err_out_max_red;

	rc = device_create_file(led_cdev->dev, &dev_attr_max_green);
	if (rc)
		goto err_out_max_green;

	rc = device_create_file(led_cdev->dev, &dev_attr_max_blue);
	if (rc)
		goto err_out_max_blue;

	rc = device_create_file(led_cdev->dev, &dev_attr_stop_on_min);
	if (rc)
		goto err_out_stop_on_min;

	rc = device_create_file(led_cdev->dev, &dev_attr_stop_on_max);
	if (rc)
		goto err_out_stop_on_max;

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

	return;

err_out_delay_down:
	device_remove_file(led_cdev->dev, &dev_attr_step_down);
err_out_step_down:
	device_remove_file(led_cdev->dev, &dev_attr_delay_up);
err_out_delay_up:
	device_remove_file(led_cdev->dev, &dev_attr_step_up);
err_out_step_up:
	device_remove_file(led_cdev->dev, &dev_attr_stop_on_max);
err_out_stop_on_max:
	device_remove_file(led_cdev->dev, &dev_attr_stop_on_min);
err_out_stop_on_min:
	device_remove_file(led_cdev->dev, &dev_attr_max_blue);
err_out_max_blue:
	device_remove_file(led_cdev->dev, &dev_attr_max_green);
err_out_max_green:
	device_remove_file(led_cdev->dev, &dev_attr_max_red);
err_out_max_red:
	device_remove_file(led_cdev->dev, &dev_attr_min_blue);
err_out_min_blue:
	device_remove_file(led_cdev->dev, &dev_attr_min_green);
err_out_min_green:
	device_remove_file(led_cdev->dev, &dev_attr_min_red);
err_out_min_red:
	device_remove_file(led_cdev->dev, &dev_attr_start);
err_out_start:
	device_remove_file(led_cdev->dev, &dev_attr_first_down);
err_out:
	dev_err(led_cdev->dev, "unable to register custompulse trigger\n");
	led_cdev->trigger_data = NULL;
	kfree(tdata);
}

static void custompulse_trig_deactivate(struct led_classdev *led_cdev)
{
	struct pulse_trig_data *pulse_data = led_cdev->trigger_data;

	pulse_data->start = 0;
	del_timer_sync(&pulse_data->timer);
	rgb_custom_set(led_cdev, 0);

	device_remove_file(led_cdev->dev, &dev_attr_first_down);
	device_remove_file(led_cdev->dev, &dev_attr_start);

	device_remove_file(led_cdev->dev, &dev_attr_min_red);
	device_remove_file(led_cdev->dev, &dev_attr_min_green);
	device_remove_file(led_cdev->dev, &dev_attr_min_blue);
	device_remove_file(led_cdev->dev, &dev_attr_max_red);
	device_remove_file(led_cdev->dev, &dev_attr_max_green);
	device_remove_file(led_cdev->dev, &dev_attr_max_blue);

	device_remove_file(led_cdev->dev, &dev_attr_stop_on_min);
	device_remove_file(led_cdev->dev, &dev_attr_stop_on_max);

	device_remove_file(led_cdev->dev, &dev_attr_step_up);
	device_remove_file(led_cdev->dev, &dev_attr_delay_up);
	device_remove_file(led_cdev->dev, &dev_attr_step_down);
	device_remove_file(led_cdev->dev, &dev_attr_delay_down);

	led_cdev->trigger_data = NULL;
	kfree(pulse_data);
}

static struct led_trigger custompulse_trigger = {
	.name     = "custompulse",
	.activate = custompulse_trig_activate,
	.deactivate = custompulse_trig_deactivate,
};

static int __init custompulse_trig_init(void)
{
	return led_trigger_register(&custompulse_trigger);
}

static void __exit custompulse_trig_exit(void)
{
	led_trigger_unregister(&custompulse_trigger);
}

module_init(custompulse_trig_init);
module_exit(custompulse_trig_exit);

MODULE_AUTHOR("Yellow Huang <yellow.huang@cybertan.com.tw>");
MODULE_DESCRIPTION("custompulse LED trigger");
MODULE_LICENSE("GPL");
