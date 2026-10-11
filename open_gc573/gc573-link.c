// SPDX-License-Identifier: GPL-2.0-only

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>

#include "gc573.h"
#include "gc573-it6664.h"

#define GC555_LINK_REG_VIDEO_SOURCE	0x01000

#define GC555_LINK_SPLITTER_READY0_GPIO	4
#define GC555_LINK_SPLITTER_READY1_GPIO	6
#define GC555_LINK_SPLITTER_ENABLE_GPIO	1
#define GC555_LINK_SPLITTER_RESET_GPIO	5
#define GC555_LINK_INPUT_HPD_GPIO	2
#define GC555_LINK_SPLITTER_SCDT_GPIO	3
#define GC555_LINK_RECEIVER_RESET_GPIO	8
#define GC555_LINK_TX1_HPD_GATE_GPIO	7
#define GC555_LINK_SPLITTER_QUIESCE_MS	200


static int gc555_link_prepare_splitter(struct gc555_dev *gc555)
{
	/* GC573 is a PCIe card with IT6805 + IT6664; no GC555 splitter hardware exists.
	 * GPIO 4 is the Green LED; do not toggle it or wait for readiness here.
	 */
	return 0;
}

static int gc555_link_reset_receiver(struct gc555_dev *gc555)
{
	int ret;

	ret = gc555_bridge_set_gpio(gc555, GC555_LINK_RECEIVER_RESET_GPIO,
				    true, 2);
	if (ret)
		return ret;
	ret = gc555_bridge_set_gpio(gc555, GC555_LINK_RECEIVER_RESET_GPIO,
				    false, 2);
	if (ret)
		return ret;

	return gc555_bridge_set_gpio(gc555, GC555_LINK_RECEIVER_RESET_GPIO,
				      true, 2);
}

static int gc555_link_set_default_source(struct gc555_dev *gc555)
{
	u32 value;
	int ret;

	ret = gc555_bridge_read(gc555, GC555_LINK_REG_VIDEO_SOURCE, &value);
	if (ret)
		return ret;

	/* Select source 0 and disable the alternate source. */
	value = (value & ~BIT(6)) | BIT(0);
	return gc555_bridge_write(gc555, GC555_LINK_REG_VIDEO_SOURCE, value);
}

int gc555_link_get_video_signal(struct gc555_dev *gc555,
				struct gc555_video_signal *signal)
{
	if (!gc555)
		return -EINVAL;

	return gc555_it6805_get_video_signal(gc555->it6805, signal);
}

int gc555_link_get_input_power(struct gc555_dev *gc555, bool *present)
{
	if (!gc555)
		return -EINVAL;

	return gc555_it6805_get_input_power(gc555->it6805, present);
}

int gc555_link_get_input_edid(struct gc555_dev *gc555, u8 *edid, size_t size)
{
	if (!gc555)
		return -EINVAL;

	return gc555_it6664_get_input_edid(gc555->it6664, edid, size);
}

int gc555_link_get_source_hdcp(struct gc555_dev *gc555,
			      enum gc555_hdcp_level *level)
{
	if (!gc555)
		return -EINVAL;

	return gc555_it6664_get_source_hdcp(gc555->it6664, level);
}

int gc555_link_get_input_hpd(struct gc555_dev *gc555, bool *high)
{
	if (!gc555_bridge_is_ready(gc555))
		return -ENODEV;

	return gc555_bridge_get_gpio(gc555, GC555_LINK_INPUT_HPD_GPIO, high);
}

int gc555_link_set_input_hpd(struct gc555_dev *gc555, bool high)
{
	if (!gc555_bridge_is_ready(gc555))
		return -ENODEV;

	return gc555_bridge_set_gpio(gc555, GC555_LINK_INPUT_HPD_GPIO,
				     high, 2);
}

int gc555_link_set_splitter_scdt(struct gc555_dev *gc555, bool high)
{
	/* GC573: GPIO 3 is the Red LED, NOT splitter SCDT! Do not toggle it here. */
	return 0;
}

int gc555_link_set_tx_hpd_gate(struct gc555_dev *gc555, unsigned int port,
			       bool high)
{
	unsigned int gpio;

	if (!gc555_bridge_is_ready(gc555))
		return -ENODEV;

	switch (port) {
	case 1:
		gpio = GC555_LINK_TX1_HPD_GATE_GPIO;
		break;
	case 2:
		gpio = GC555_LINK_SPLITTER_READY1_GPIO;
		break;
	default:
		return -EINVAL;
	}

	return gc555_bridge_set_gpio(gc555, gpio, high, 0);
}

int gc555_link_tx_is_hdmi(struct gc555_dev *gc555, unsigned int port,
			  bool *is_hdmi)
{
	u8 status;
	u8 mode;
	int ret;

	if (!gc555 || !is_hdmi)
		return -EINVAL;
	if (!gc555->it6664)
		return -ENODEV;

	ret = gc555_it6664_tx_read_mode(gc555->it6664, port, &status, &mode);
	if (ret)
		return ret;

	*is_hdmi = mode & BIT(0);

	return 0;
}

int gc555_link_init(struct gc555_dev *gc555)
{
	int ret;

	if (!gc555_bridge_is_ready(gc555))
		return -ENODEV;

	/* Allow splitter control to quiesce before reset sequencing. */
	msleep(GC555_LINK_SPLITTER_QUIESCE_MS);
	ret = gc555_link_prepare_splitter(gc555);
	if (ret)
		return ret;
	ret = gc555_link_reset_receiver(gc555);
	if (ret)
		return ret;
	ret = gc555_link_set_default_source(gc555);
	if (ret)
		return ret;

	return 0;
}
