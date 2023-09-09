#include "linux/of_device.h"
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct tc358775_setting {
	unsigned long dsi_flags;
	enum mipi_dsi_pixel_format dsi_pix_fmt;
	unsigned int dsi_lanes;
	const uint8_t (*dsi_init_seq)[27][6];
	const struct drm_display_mode *display_mode;
};

struct tc358775 {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	const struct tc358775_setting *setting;

	bool prepared;

	// LVDS供电
	struct regulator *power;
};

static inline struct tc358775 *to_tc358775(struct drm_panel *panel)
{
	return container_of(panel, struct tc358775, panel);
}

static const struct drm_display_mode common_1024_600_display_mode = {
	.clock = 51450,

	.hdisplay = 1024,
	.hsync_start = 1024 + 156,
	.hsync_end = 1024 + 156 + 8,
	.htotal = 1024 + 156 + 8 + 156,

	.vdisplay = 600,
	.vsync_start = 600 + 16,
	.vsync_end = 600 + 16 + 6,
	.vtotal = 600 + 16 + 6 + 16,
};

static const uint8_t tc358775_1024_600_single_8bit_init_seq[27][6] = {
	{ 0x3C, 0x01, 0x05, 0x00, 0x03, 0x00 },
	{ 0x14, 0x01, 0x03, 0x00, 0x00, 0x00 },
	{ 0x64, 0x01, 0x04, 0x00, 0x00, 0x00 },
	{ 0x68, 0x01, 0x04, 0x00, 0x00, 0x00 },
	{ 0x6C, 0x01, 0x04, 0x00, 0x00, 0x00 },
	{ 0x70, 0x01, 0x04, 0x00, 0x00, 0x00 },
	{ 0x34, 0x01, 0x1F, 0x00, 0x00, 0x00 },
	{ 0x10, 0x02, 0x1F, 0x00, 0x00, 0x00 },
	{ 0x04, 0x01, 0x01, 0x00, 0x00, 0x00 },
	{ 0x04, 0x02, 0x01, 0x00, 0x00, 0x00 },
	{ 0x50, 0x04, 0x20, 0x01, 0xF0, 0x03 },
	{ 0x54, 0x04, 0x08, 0x00, 0x9C, 0x00 },
	{ 0x58, 0x04, 0x00, 0x04, 0x9C, 0x00 },
	{ 0x5C, 0x04, 0x06, 0x00, 0x10, 0x00 },
	{ 0x60, 0x04, 0x58, 0x02, 0x10, 0x00 },
	{ 0x64, 0x04, 0x01, 0x00, 0x00, 0x00 },
	{ 0xA0, 0x04, 0x2D, 0x80, 0x44, 0x00 },
	{ 0xA0, 0x04, 0x2D, 0x80, 0x04, 0x00 },
	{ 0x04, 0x05, 0x04, 0x00, 0x00, 0x00 },
	{ 0x80, 0x04, 0x00, 0x01, 0x02, 0x03 },
	{ 0x84, 0x04, 0x04, 0x07, 0x05, 0x08 },
	{ 0x88, 0x04, 0x09, 0x0A, 0x0E, 0x0F },
	{ 0x8C, 0x04, 0x0B, 0x0C, 0x0D, 0x10 },
	{ 0x90, 0x04, 0x16, 0x17, 0x11, 0x12 },
	{ 0x94, 0x04, 0x13, 0x14, 0x15, 0x1B },
	{ 0x98, 0x04, 0x18, 0x19, 0x1A, 0x06 },
	{ 0x9C, 0x04, 0x41, 0x01, 0x00, 0x00 },
};

static const struct tc358775_setting tc358775_1024_600_single_8bit_setting = {
	.dsi_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
		     MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET,
	.dsi_pix_fmt = MIPI_DSI_FMT_RGB888,
	.dsi_lanes = 4,
	.dsi_init_seq = &tc358775_1024_600_single_8bit_init_seq,
	.display_mode = &common_1024_600_display_mode,
};

static int tc358775_send_init_seq(struct tc358775 *priv)
{
	struct mipi_dsi_device *dsi = priv->dsi;
	struct device *dev = &priv->dsi->dev;
	int i;
	int ret;

	for (i = 0; i < 27; i++) {
		ret = mipi_dsi_generic_write(
			dsi, &(*priv->setting->dsi_init_seq)[i][0], 6);
		if (ret < 1) {
			dev_err(dev, "send dsi init seq error\n");
			return -1;
		}

		msleep(2);
	}

	return 0;
}

static int tc358775_prepare(struct drm_panel *panel)
{
	struct tc358775 *priv = to_tc358775(panel);
	struct device *dev = &priv->dsi->dev;
	int ret;

	if (priv->prepared)
		return 0;

	ret = tc358775_send_init_seq(priv);
	if (ret < 0) {
		dev_err(dev, "failed to initialize panel: %d\n", ret);
		return ret;
	}

	// 将打开LVDS供电的操作放在初始化TC358775后面
	// TC358775的初始化是不需要接有屏幕的，相反，如果屏幕先工作（打开LVDS供电并开启屏幕背光），TC358775初始化过程中屏幕会白色闪屏
	// 针对一些背光不可单独控的屏幕（打开屏幕供电后背光也就打开了），交换顺序可避免这个问题
	ret = regulator_enable(priv->power);
	if (ret < 0)
		return ret;

	priv->prepared = true;

	return 0;
}

static int tc358775_unprepare(struct drm_panel *panel)
{
	struct tc358775 *priv = to_tc358775(panel);

	if (!priv->prepared)
		return 0;

	regulator_disable(priv->power);

	priv->prepared = false;

	return 0;
}

static int tc358775_get_modes(struct drm_panel *panel,
			      struct drm_connector *connector)
{
	struct tc358775 *priv = to_tc358775(panel);
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, priv->setting->display_mode);

	if (!mode) {
		dev_err(&priv->dsi->dev, "failed to add mode %ux%u@%u\n",
			priv->setting->display_mode->hdisplay,
			priv->setting->display_mode->vdisplay,
			drm_mode_vrefresh(priv->setting->display_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);

	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs tc358775_panel_funcs = {
	.prepare = tc358775_prepare,
	.unprepare = tc358775_unprepare,
	.get_modes = tc358775_get_modes,
};

static const struct of_device_id tc358775_of_match[] = {
	{
		.compatible = "tc358775,1024x600-single-8bit",
		.data = &tc358775_1024_600_single_8bit_setting,
	},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, tc358775_of_match);

static int tc358775_probe(struct mipi_dsi_device *dsi)
{
	struct tc358775 *priv;
	struct device *dev = &dsi->dev;
	const struct of_device_id *id;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	id = of_match_device(tc358775_of_match, dev);
	if (!id)
		return -ENODEV;
	priv->setting = id->data;

	priv->power = devm_regulator_get(dev, "power");
	if (IS_ERR(priv->power))
		return PTR_ERR(priv->power);

	priv->dsi = dsi;

	dsi->lanes = priv->setting->dsi_lanes;
	dsi->format = priv->setting->dsi_pix_fmt;
	dsi->mode_flags = priv->setting->dsi_flags;

	mipi_dsi_set_drvdata(dsi, priv);

	drm_panel_init(&priv->panel, dev, &tc358775_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);

	ret = drm_panel_of_backlight(&priv->panel);
	if (ret)
		return dev_err_probe(dev, ret, "failed to get backlight\n");

	drm_panel_add(&priv->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "failed to attach to DSI host: %d\n", ret);
		drm_panel_remove(&priv->panel);
		return ret;
	}

	return 0;
}

static void tc358775_remove(struct mipi_dsi_device *dsi)
{
	struct tc358775 *priv = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&priv->panel);
}

static struct mipi_dsi_driver tc358775_driver = {
	.probe = tc358775_probe,
	.remove = tc358775_remove,
	.driver = {
		.name = "panel-toshiba-tc358775",
		.of_match_table = tc358775_of_match,
	},
};
module_mipi_dsi_driver(tc358775_driver);

MODULE_AUTHOR("retro98boy <retro98boy@qq.com>");
MODULE_DESCRIPTION("LVDS panel driver for TN3399_V3 with TC358775");
MODULE_LICENSE("GPL v2");
