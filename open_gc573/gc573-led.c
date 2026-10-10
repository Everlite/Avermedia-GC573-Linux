// SPDX-License-Identifier: GPL-2.0-only

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/led-class-multicolor.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include "gc573.h"

#define GC555_LED_COUNT			26
#define GC555_LED_COLORS		3
#define GC555_LED_I2C_ADDRESS		0x74
#define GC555_LED_PAGE_REG		0xfd
#define GC555_LED_FUNCTION_PAGE		0x0b
#define GC555_LED_PWM_PAGE		0x00
#define GC555_LED_PWM_FIRST_REG		0x24
#define GC555_LED_PWM_REG_COUNT		144
#define GC555_LED_ENABLE_REG_COUNT	18
#define GC555_LED_MAX_WRITE		180
#define GC555_LED_UPDATE_DELAY_MS	2

struct gc555_led;

struct gc555_led_pixel {
	struct led_classdev_mc mc;
	struct mc_subled subled[GC555_LED_COLORS];
	struct gc555_led *controller;
	char name[24];
	unsigned int index;
};

struct gc555_led {
	struct gc555_dev *gc555;
	struct i2c_client *client;
	struct delayed_work update_work;
	struct mutex lock;
	struct gc555_led_pixel pixels[GC555_LED_COUNT];
	u8 pwm[GC555_LED_PWM_REG_COUNT];
	unsigned int registered;
	bool running;
};

static const u8 gc555_led_positions[GC555_LED_COUNT] = {
	/* Logical LEDs 1-20 follow the skirt; 21-26 cover the front panel. */
	7, 20, 18, 19, 15, 16, 17, 13, 14, 9, 8, 12, 11,
	10, 2, 1, 3, 4, 5, 6, 25, 26, 27, 28, 29, 30,
};

static int gc555_led_write(struct gc555_led *led, u8 reg,
			   const u8 *data, size_t len)
{
	u8 transfer[GC555_LED_MAX_WRITE + 1];
	int ret;

	if (!len || len > GC555_LED_MAX_WRITE)
		return -EINVAL;

	transfer[0] = reg;
	memcpy(transfer + 1, data, len);
	ret = i2c_master_send(led->client, transfer, len + 1);

	return ret < 0 ? ret : ret == len + 1 ? 0 : -EIO;
}

static int gc555_led_write_byte(struct gc555_led *led, u8 reg, u8 value)
{
	return gc555_led_write(led, reg, &value, 1);
}

static int gc555_led_select_page(struct gc555_led *led, u8 page)
{
	return gc555_led_write_byte(led, GC555_LED_PAGE_REG, page);
}

static int gc555_led_write_pwm(struct gc555_led *led)
{
	int ret;

	ret = gc555_led_select_page(led, GC555_LED_PWM_PAGE);
	if (ret)
		return ret;

	return gc555_led_write(led, GC555_LED_PWM_FIRST_REG, led->pwm,
			       sizeof(led->pwm));
}

static int gc555_led_hw_init(struct gc555_led *led)
{
	static const struct {
		u8 reg;
		u8 value;
	} function_setup[] = {
		{ 0x0a, 0x00 },
		{ 0x01, 0x18 },
		{ 0x0d, 0xe4 },
		{ 0x0e, 0x01 },
		{ 0x14, 0x55 },
		{ 0x15, 0x00 },
		{ 0x18, 0xaa },
		{ 0x19, 0xaa },
		{ 0x1a, 0xaa },
		{ 0x0f, 0xbf },
	};
	u8 clear_pwm_and_blink[GC555_LED_MAX_WRITE] = {};
	u8 breath_table[36];
	u8 enable[GC555_LED_ENABLE_REG_COUNT];
	unsigned int i;
	int ret;

	ret = gc555_bridge_set_gpio(led->gc555, 9, true, 10);
	if (ret)
		return ret;

	ret = gc555_led_select_page(led, GC555_LED_FUNCTION_PAGE);
	if (ret)
		return ret;
	for (i = 0; i < ARRAY_SIZE(function_setup); i++) {
		ret = gc555_led_write_byte(led, function_setup[i].reg,
					   function_setup[i].value);
		if (ret)
			return ret;
	}

	ret = gc555_led_select_page(led, GC555_LED_PWM_PAGE);
	if (ret)
		return ret;
	ret = gc555_led_write(led, 0x00, clear_pwm_and_blink,
			       sizeof(clear_pwm_and_blink));
	if (ret)
		return ret;

	memset(breath_table, 0x55, sizeof(breath_table));
	ret = gc555_led_select_page(led, 0x0d);
	if (ret)
		return ret;
	ret = gc555_led_write(led, 0x00, breath_table, sizeof(breath_table));
	if (ret)
		return ret;

	ret = gc555_led_select_page(led, GC555_LED_FUNCTION_PAGE);
	if (ret)
		return ret;
	ret = gc555_led_write_byte(led, 0x0a, 0x01);
	if (ret)
		return ret;

	memset(enable, 0xff, sizeof(enable));
	ret = gc555_led_select_page(led, GC555_LED_PWM_PAGE);
	if (ret)
		return ret;
	ret = gc555_led_write(led, 0x00, enable, sizeof(enable));
	if (ret)
		return ret;

	return gc555_led_write_pwm(led);
}

static void gc555_led_update_work(struct work_struct *work)
{
	struct gc555_led *led =
		container_of(to_delayed_work(work), struct gc555_led,
			     update_work);
	int ret;

	mutex_lock(&led->lock);
	if (!led->running) {
		mutex_unlock(&led->lock);
		return;
	}
	ret = gc555_led_write_pwm(led);
	mutex_unlock(&led->lock);

	if (ret)
		dev_warn_ratelimited(led->gc555->dev,
				     "LED update failed: %d\n", ret);
}

static int gc555_led_set_brightness(struct led_classdev *cdev,
				    enum led_brightness brightness)
{
	struct led_classdev_mc *mc = lcdev_to_mccdev(cdev);
	struct gc555_led_pixel *pixel =
		container_of(mc, struct gc555_led_pixel, mc);
	struct gc555_led *led = pixel->controller;
	unsigned int position = gc555_led_positions[pixel->index] - 1;
	unsigned int block = position / 12;
	unsigned int channel = position % 12;
	unsigned int red = block * 36 + channel;
	bool running;

	led_mc_calc_color_components(mc, brightness);

	mutex_lock(&led->lock);
	led->pwm[red] = pixel->subled[0].brightness;
	led->pwm[red + 12] = pixel->subled[1].brightness;
	led->pwm[red + 24] = pixel->subled[2].brightness;
	running = led->running;
	mutex_unlock(&led->lock);

	if (running)
		mod_delayed_work(system_wq, &led->update_work,
				 msecs_to_jiffies(GC555_LED_UPDATE_DELAY_MS));
	return 0;
}

static int gc555_led_register_pixels(struct gc555_led *led)
{
	unsigned int i;
	int ret;

	for (i = 0; i < GC555_LED_COUNT; i++) {
		struct gc555_led_pixel *pixel = &led->pixels[i];

		pixel->controller = led;
		pixel->index = i;
		pixel->subled[0].color_index = LED_COLOR_ID_RED;
		pixel->subled[1].color_index = LED_COLOR_ID_GREEN;
		pixel->subled[2].color_index = LED_COLOR_ID_BLUE;
		pixel->mc.num_colors = GC555_LED_COLORS;
		pixel->mc.subled_info = pixel->subled;
		snprintf(pixel->name, sizeof(pixel->name),
			 "gc555:rgb:%02u", i + 1);
		pixel->mc.led_cdev.name = pixel->name;
		pixel->mc.led_cdev.max_brightness = 255;
		pixel->mc.led_cdev.brightness_set_blocking =
			gc555_led_set_brightness;

		ret = led_classdev_multicolor_register(led->gc555->dev,
						       &pixel->mc);
		if (ret)
			return ret;
		led->registered++;
	}

	return 0;
}

static void gc555_led_unregister_pixels(struct gc555_led *led)
{
	while (led->registered)
		led_classdev_multicolor_unregister(
			&led->pixels[--led->registered].mc);
}

int gc555_led_init(struct gc555_dev *gc555)
{
	struct i2c_adapter *adapter;
	struct gc555_led *led;
	int ret;

	adapter = gc555_i2c_get_adapter(gc555, GC555_I2C_BUS_SECONDARY);
	if (!adapter)
		return -ENODEV;

	led = devm_kzalloc(gc555->dev, sizeof(*led), GFP_KERNEL);
	if (!led)
		return -ENOMEM;

	led->gc555 = gc555;
	mutex_init(&led->lock);
	INIT_DELAYED_WORK(&led->update_work, gc555_led_update_work);
	gc555->led = led;

	led->client = i2c_new_dummy_device(adapter, GC555_LED_I2C_ADDRESS);
	if (IS_ERR(led->client)) {
		ret = PTR_ERR(led->client);
		led->client = NULL;
		goto clear_led;
	}

	ret = gc555_led_hw_init(led);
	if (ret)
		goto unregister_client;
	led->running = true;

	ret = gc555_led_register_pixels(led);
	if (ret)
		goto stop_led;

	dev_info(gc555->dev, "registered %u RGB LEDs\n", GC555_LED_COUNT);
	return 0;

stop_led:
	led->running = false;
	gc555_led_unregister_pixels(led);
unregister_client:
	i2c_unregister_device(led->client);
	led->client = NULL;
clear_led:
	gc555->led = NULL;
	return dev_err_probe(gc555->dev, ret,
			     "failed to initialize RGB lighting\n");
}

void gc555_led_cleanup(struct gc555_dev *gc555)
{
	struct gc555_led *led;

	if (!gc555 || !gc555->led)
		return;

	led = gc555->led;
	mutex_lock(&led->lock);
	led->running = false;
	mutex_unlock(&led->lock);
	cancel_delayed_work_sync(&led->update_work);
	gc555_led_unregister_pixels(led);
	i2c_unregister_device(led->client);
	led->client = NULL;
	gc555->led = NULL;
}

void gc555_led_suspend(struct gc555_dev *gc555)
{
	struct gc555_led *led;

	if (!gc555 || !gc555->led)
		return;

	led = gc555->led;
	mutex_lock(&led->lock);
	led->running = false;
	mutex_unlock(&led->lock);
	cancel_delayed_work_sync(&led->update_work);
}

int gc555_led_resume(struct gc555_dev *gc555)
{
	struct gc555_led *led;
	int ret;

	if (!gc555 || !gc555->led)
		return 0;

	led = gc555->led;
	mutex_lock(&led->lock);
	ret = gc555_led_hw_init(led);
	if (!ret)
		led->running = true;
	mutex_unlock(&led->lock);

	return ret;
}
