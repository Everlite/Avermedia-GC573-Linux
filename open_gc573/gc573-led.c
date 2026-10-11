// SPDX-License-Identifier: GPL-2.0-only

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/led-class-multicolor.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include "gc573.h"

static int led_mode = 0;
module_param(led_mode, int, 0644);
MODULE_PARM_DESC(led_mode, "LED lighting mode: 0=off (dark), 1=solid blue, 2=solid green, 3=solid red, 4=cyan, 5=magenta, 6=yellow, 7=white");

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
	int mode;
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

static int gc555_led_write_chunked(struct gc555_led *led, u8 reg,
				   const u8 *data, size_t len)
{
	/* Write in max 18-byte chunks so hardware I2C FIFO never overflows */
	size_t offset = 0;
	int ret;

	while (offset < len) {
		size_t chunk = min_t(size_t, len - offset, 18);
		ret = gc555_led_write(led, reg + offset, data + offset, chunk);
		if (ret)
			return ret;
		offset += chunk;
	}
	return 0;
}

static int gc555_led_write_pwm(struct gc555_led *led)
{
	int ret;

	ret = gc555_led_select_page(led, GC555_LED_PWM_PAGE);
	if (ret)
		return ret;

	return gc555_led_write_chunked(led, GC555_LED_PWM_FIRST_REG, led->pwm,
				       sizeof(led->pwm));
}

static int gc555_led_hw_init(struct gc555_led *led)
{
	static const struct {
		u8 reg;
		u8 value;
	} function_setup[] = {
		{ 0x0a, 0x00 }, /* Shutdown during setup */
		{ 0x00, 0x00 }, /* Picture Display Mode (disables auto-breath!) */
		{ 0x01, 0x00 }, /* Select Frame 1 */
		{ 0x08, 0x00 }, /* Disable breath control 1 */
		{ 0x09, 0x00 }, /* Disable breath control 2 */
		{ 0x0f, 0x00 }, /* Disable breath enable */
	};
	u8 zeros[18] = {};
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

	/* Select PWM page (Frame 1 = Page 0) and clear blink & enable */
	ret = gc555_led_select_page(led, GC555_LED_PWM_PAGE);
	if (ret)
		return ret;

	/* Clear blink registers (0x12..0x23) */
	ret = gc555_led_write(led, 0x12, zeros, sizeof(zeros));
	if (ret)
		return ret;

	/* Clear all 144 PWM registers */
	memset(led->pwm, 0, sizeof(led->pwm));
	return gc555_led_write_chunked(led, GC555_LED_PWM_FIRST_REG,
				       led->pwm, sizeof(led->pwm));
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

#define GC573_LED_REG_DIVIDER    0x0800
#define GC573_LED_REG_ENABLE     0x0804
#define GC573_LED_REG_BTN_BASE   0x0808
#define GC573_LED_REG_SCRIPT     0x2008

static void gc573_fpga_write_rgb(struct gc555_dev *gc555, bool r, bool g, bool b)
{
	int i;

	gc555_bridge_write(gc555, GC573_LED_REG_DIVIDER, 0x0003ffff);
	for (i = 0; i < 5; i++) {
		u32 btn_reg = GC573_LED_REG_BTN_BASE + i * 24;

		gc555_bridge_write(gc555, btn_reg + 0, r ? 3 : 2);
		gc555_bridge_write(gc555, btn_reg + 4, r ? 4 : 2);
		gc555_bridge_write(gc555, btn_reg + 8, g ? 3 : 2);
		gc555_bridge_write(gc555, btn_reg + 12, g ? 4 : 2);
		gc555_bridge_write(gc555, btn_reg + 16, b ? 3 : 2);
		gc555_bridge_write(gc555, btn_reg + 20, b ? 4 : 2);
	}
	gc555_bridge_write(gc555, GC573_LED_REG_SCRIPT + 0, 0x00000000);
	gc555_bridge_write(gc555, GC573_LED_REG_SCRIPT + 4, 0x40211010);
	gc555_bridge_write(gc555, GC573_LED_REG_ENABLE, (r || g || b) ? 0x1f : 0x00);
}

static void gc573_fpga_set_led(struct gc555_dev *gc555, int mode)
{
	if (!gc555 || !gc555->bridge.regs)
		return;

	/* Stop the FPGA hardware LED sequencer before reprogram */
	gc555_bridge_write(gc555, GC573_LED_REG_ENABLE, 0x00);

	switch (mode) {
	case 0: /* All LEDs OFF / Dark */
		gc573_fpga_write_rgb(gc555, false, false, false);
		break;
	case 1: /* Solid Blue (Profile 4 from official customer_LEDgc573.txt) */
		gc573_fpga_write_rgb(gc555, false, false, true);
		break;
	case 2: /* Solid Green */
		gc573_fpga_write_rgb(gc555, false, true, false);
		break;
	case 3: /* Solid Red (Profile 1) */
		gc573_fpga_write_rgb(gc555, true, false, false);
		break;
	case 4: /* Cyan / Türkis */
		gc573_fpga_write_rgb(gc555, false, true, true);
		break;
	case 5: /* Magenta / Lila */
		gc573_fpga_write_rgb(gc555, true, false, true);
		break;
	case 6: /* Gelb */
		gc573_fpga_write_rgb(gc555, true, true, false);
		break;
	case 7: /* Weiß */
		gc573_fpga_write_rgb(gc555, true, true, true);
		break;
	default:
		break;
	}
}

int gc555_led_get_mode(struct gc555_dev *gc555)
{
	if (!gc555 || !gc555->led)
		return -ENODEV;
	return gc555->led->mode;
}

int gc555_led_set_mode(struct gc555_dev *gc555, int mode)
{
	struct gc555_led *led;
	u8 enable[GC555_LED_ENABLE_REG_COUNT];
	int ret = 0;

	if (!gc555 || !gc555->led)
		return -ENODEV;

	if (mode < 0 || mode > 7)
		return -EINVAL;

	/* 1. Control GC573 FPGA hardware MMIO RGB LED engine (0x800-0x81c, 0x2008) */
	gc573_fpga_set_led(gc555, mode);

	led = gc555->led;
	mutex_lock(&led->lock);

	/* If GC555 IS31FL3731 I2C controller is present (GC555 Live Gamer BOLT only) */
	if (led->client && gc555->pdev->subsystem_device == GC555_PCI_SUBDEVICE_ID) {
		if (mode == 0) {
			gc555_bridge_set_gpio(gc555, 9, true, 5);
			gc555_led_select_page(led, GC555_LED_FUNCTION_PAGE);
			gc555_led_write_byte(led, 0x00, 0x00);
			gc555_led_write_byte(led, 0x08, 0x00);
			gc555_led_write_byte(led, 0x09, 0x00);
			gc555_led_write_byte(led, 0x0f, 0x00);
			gc555_led_write_byte(led, 0x0a, 0x00);
			gc555_led_select_page(led, GC555_LED_PWM_PAGE);
			memset(enable, 0, sizeof(enable));
			gc555_led_write(led, 0x00, enable, sizeof(enable));
			memset(led->pwm, 0, sizeof(led->pwm));
			gc555_led_write_chunked(led, GC555_LED_PWM_FIRST_REG,
					       led->pwm, sizeof(led->pwm));
			gc555_bridge_set_gpio(gc555, 9, false, 0);
		} else {
			gc555_bridge_set_gpio(gc555, 9, true, 10);
			gc555_led_select_page(led, GC555_LED_FUNCTION_PAGE);
			gc555_led_write_byte(led, 0x00, 0x00);
			gc555_led_write_byte(led, 0x01, 0x00);
			gc555_led_write_byte(led, 0x08, 0x00);
			gc555_led_write_byte(led, 0x09, 0x00);
			gc555_led_write_byte(led, 0x0f, 0x00);
			gc555_led_write_byte(led, 0x0a, 0x01);
			gc555_led_select_page(led, GC555_LED_PWM_PAGE);
			memset(enable, 0xff, sizeof(enable));
			gc555_led_write(led, 0x00, enable, sizeof(enable));
		}
	}

	led->mode = mode;
	dev_info(gc555->dev, "LED mode set to %d\n", mode);

	mutex_unlock(&led->lock);
	return ret;
}

static struct i2c_client *gc555_led_probe_client(struct gc555_dev *gc555)
{
	static const enum gc555_i2c_bus_id candidate_buses[] = {
		GC555_I2C_BUS_SECONDARY,
		GC555_I2C_BUS_TERTIARY,
	};
	struct i2c_adapter *adapter;
	struct i2c_client *client;
	unsigned int i;
	int ret;
	u8 test_cmd[2] = { GC555_LED_PAGE_REG, GC555_LED_FUNCTION_PAGE };

	/* Ensure hardware enable pin (GPIO 9) is high before probing I2C */
	ret = gc555_bridge_set_gpio(gc555, 9, true, 20);
	if (ret)
		dev_warn(gc555->dev, "failed to set LED GPIO 9: %d\n", ret);

	for (i = 0; i < ARRAY_SIZE(candidate_buses); i++) {
		adapter = gc555_i2c_get_adapter(gc555, candidate_buses[i]);
		if (!adapter)
			continue;

		client = i2c_new_dummy_device(adapter, GC555_LED_I2C_ADDRESS);
		if (IS_ERR(client))
			continue;

		/* Try sending page select command (0xfd, 0x0b) */
		ret = i2c_master_send(client, test_cmd, sizeof(test_cmd));
		if (ret == sizeof(test_cmd)) {
			dev_info(gc555->dev,
				 "IS31FL3731 LED controller detected on bridge I2C %u\n",
				 candidate_buses[i]);
			return client;
		}

		i2c_unregister_device(client);
	}

	dev_warn(gc555->dev, "IS31FL3731 LED controller not found on any I2C bus\n");
	return ERR_PTR(-ENODEV);
}

int gc555_led_init(struct gc555_dev *gc555)
{
	struct gc555_led *led;
	int ret;

	led = devm_kzalloc(gc555->dev, sizeof(*led), GFP_KERNEL);
	if (!led)
		return -ENOMEM;

	led->gc555 = gc555;
	mutex_init(&led->lock);
	INIT_DELAYED_WORK(&led->update_work, gc555_led_update_work);
	gc555->led = led;

	/*
	 * Hardware differentiation:
	 * - GC555 (Live Gamer BOLT, external TB3): has IS31FL3731 I2C LED controller on GPIO 9.
	 * - GC573 (Live Gamer 4K, PCIe): has FPGA MMIO LED registers (0x0800-0x086c).
	 *   Do NOT probe I2C or toggle GPIO 9 on GC573 as GPIO 9 is connected to IT6664 HDMI logic!
	 */
	if (gc555->pdev->subsystem_device == GC555_PCI_SUBDEVICE_ID) {
		led->client = gc555_led_probe_client(gc555);
		if (!IS_ERR(led->client)) {
			ret = gc555_led_hw_init(led);
			if (ret) {
				i2c_unregister_device(led->client);
				led->client = NULL;
			} else {
				led->running = true;
				gc555_led_register_pixels(led);
			}
		} else {
			led->client = NULL;
		}
	} else {
		led->client = NULL;
	}

	/* Apply configured LED mode at driver load (FPGA MMIO & I2C) */
	gc555_led_set_mode(gc555, led_mode);

	return 0;
}

void gc555_led_cleanup(struct gc555_dev *gc555)
{
	struct gc555_led *led;

	gc573_fpga_set_led(gc555, 0);

	if (!gc555 || !gc555->led)
		return;

	led = gc555->led;
	mutex_lock(&led->lock);
	led->running = false;
	mutex_unlock(&led->lock);
	cancel_delayed_work_sync(&led->update_work);
	if (led->client) {
		gc555_led_unregister_pixels(led);
		i2c_unregister_device(led->client);
		led->client = NULL;
	}
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
	if (!ret) {
		led->running = true;
		gc555_led_set_mode(gc555, led->mode);
	}
	mutex_unlock(&led->lock);

	return ret;
}
