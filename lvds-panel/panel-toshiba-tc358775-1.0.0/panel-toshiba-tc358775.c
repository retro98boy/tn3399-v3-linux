#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct tc358775 {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;

	bool prepared;

	struct regulator *panel_supply;
};

static inline struct tc358775 *to_tc358775(struct drm_panel *panel)
{
	return container_of(panel, struct tc358775, panel);
}

static uint8_t tc358775_init_seq[27][6] = {
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

static int tc358775_send_init_seq(struct tc358775 *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &ctx->dsi->dev;
	int i;
	int ret;

	for (i = 0; i < 27; i++) {
		ret = mipi_dsi_generic_write(dsi, tc358775_init_seq[i], 6);
		if (ret < 1) {
			dev_err(dev, "send init seq error\n");
			return -1;
		}

		msleep(2);
	}

	return 0;
}

static int tc358775_prepare(struct drm_panel *panel)
{
	struct tc358775 *ctx = to_tc358775(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (ctx->prepared)
		return 0;

	ret = tc358775_send_init_seq(ctx);
	if (ret < 0) {
		dev_err(dev, "failed to initialize panel: %d\n", ret);
		return ret;
	}

	// 将打开LVDS供电的操作放在初始化TC358775后面
	// TC358775的初始化是不需要接有屏幕的，相反，如果屏幕先工作（打开LVDS供电并开启屏幕背光），TC358775初始化过程中屏幕会白色闪屏
	// 针对一些背光不可单独控的屏幕（打开屏幕供电后背光也就打开了），交换顺序可避免这个问题
	ret = regulator_enable(ctx->panel_supply);
	if (ret < 0)
		return ret;

	ctx->prepared = true;

	return 0;
}

static int tc358775_unprepare(struct drm_panel *panel)
{
	struct tc358775 *ctx = to_tc358775(panel);

	if (!ctx->prepared)
		return 0;

	regulator_disable(ctx->panel_supply);

	ctx->prepared = false;

	return 0;
}

static const struct drm_display_mode tc358775_mode = {
	.clock = 51450,

	.hdisplay = 1024,
	.hsync_start = 1024 + 156,
	.hsync_end = 1024 + 156 + 8,
	.htotal = 1024 + 156 + 8 + 156,

	.vdisplay = 600,
	.vsync_start = 600 + 16,
	.vsync_end = 600 + 16 + 6,
	.vtotal = 600 + 16 + 6 + 16,

	.type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
};

static int tc358775_get_modes(struct drm_panel *panel,
			      struct drm_connector *connector)
{
	struct tc358775 *ctx = to_tc358775(panel);
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &tc358775_mode);

	if (!mode) {
		dev_err(&ctx->dsi->dev, "failed to add mode %ux%u@%u\n",
			tc358775_mode.hdisplay, tc358775_mode.vdisplay,
			drm_mode_vrefresh(&tc358775_mode));

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

static int tc358775_probe(struct mipi_dsi_device *dsi)
{
	struct tc358775 *ctx;
	struct device *dev = &dsi->dev;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->panel_supply = devm_regulator_get(dev, "panel");
	if (IS_ERR(ctx->panel_supply))
		return PTR_ERR(ctx->panel_supply);

	ctx->dsi = dsi;

	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET;

	drm_panel_init(&ctx->panel, dev, &tc358775_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);

	ret = drm_panel_of_backlight(&ctx->panel);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to get backlight\n");

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		drm_panel_remove(&ctx->panel);

		return ret;
	}

	return 0;
}

static void tc358775_remove(struct mipi_dsi_device *dsi)
{
	struct tc358775 *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id tc358775_of_match[] = {
	{ .compatible = "toshiba,tc358775-panel" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, tc358775_of_match);

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
