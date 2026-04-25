// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2026 FIXME
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved. (FIXME)

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <drm/drm_probe_helper.h>

struct nt35597_wqxga_truly {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct regulator_bulk_data *supplies;
	struct gpio_desc *reset_gpio;
};

static const struct regulator_bulk_data nt35597_wqxga_truly_supplies[] = {
	{ .supply = "vddio" },
	{ .supply = "vddneg" },
	{ .supply = "vddpos" },
};

static inline
struct nt35597_wqxga_truly *to_nt35597_wqxga_truly(struct drm_panel *panel)
{
	return container_of_const(panel, struct nt35597_wqxga_truly, panel);
}

static void nt35597_wqxga_truly_reset(struct nt35597_wqxga_truly *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(5000, 6000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(25);
}

static int nt35597_wqxga_truly_on(struct nt35597_wqxga_truly *ctx)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	mipi_dsi_dcs_exit_sleep_mode_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 120);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x50, 0x5a, 0x0e);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x83, 0xac, 0xb6, 0x6d);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x50, 0x5a, 0x19);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x80,
				     0x92, 0x8e, 0x8c, 0x8a, 0x88, 0x87, 0x86,
				     0x84, 0x83, 0x82, 0x81, 0x81, 0x55, 0x55,
				     0xf6, 0x2f);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x90,
				     0xf3, 0xff, 0xff, 0xef, 0xbf, 0x7f, 0x0f);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_READ_DDB_START,
				     0x00, 0x48);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x50, 0x5a, 0x23);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x90, 0xff, 0x0f);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x94, 0x2c, 0x01);
	mipi_dsi_dcs_set_display_on_multi(&dsi_ctx);
	mipi_dsi_usleep_range(&dsi_ctx, 1000, 2000);

	return dsi_ctx.accum_err;
}

static int nt35597_wqxga_truly_off(struct nt35597_wqxga_truly *ctx)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	mipi_dsi_dcs_set_display_off_multi(&dsi_ctx);
	mipi_dsi_usleep_range(&dsi_ctx, 1000, 2000);
	mipi_dsi_dcs_enter_sleep_mode_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 120);

	return dsi_ctx.accum_err;
}

static int nt35597_wqxga_truly_prepare(struct drm_panel *panel)
{
	struct nt35597_wqxga_truly *ctx = to_nt35597_wqxga_truly(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(nt35597_wqxga_truly_supplies), ctx->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators: %d\n", ret);
		return ret;
	}

	nt35597_wqxga_truly_reset(ctx);

	ret = nt35597_wqxga_truly_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		regulator_bulk_disable(ARRAY_SIZE(nt35597_wqxga_truly_supplies), ctx->supplies);
		return ret;
	}

	return 0;
}

static int nt35597_wqxga_truly_unprepare(struct drm_panel *panel)
{
	struct nt35597_wqxga_truly *ctx = to_nt35597_wqxga_truly(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	ret = nt35597_wqxga_truly_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	regulator_bulk_disable(ARRAY_SIZE(nt35597_wqxga_truly_supplies), ctx->supplies);

	return 0;
}

static const struct drm_display_mode nt35597_wqxga_truly_mode = {
	.clock = (800 + 25 + 14 + 25) * (2176 + 250 + 8 + 73) * 60 / 1000,
	.hdisplay = 800,
	.hsync_start = 800 + 25,
	.hsync_end = 800 + 25 + 14,
	.htotal = 800 + 25 + 14 + 25,
	.vdisplay = 2176,
	.vsync_start = 2176 + 250,
	.vsync_end = 2176 + 250 + 8,
	.vtotal = 2176 + 250 + 8 + 73,
	.width_mm = 147,
	.height_mm = 197,
	.type = DRM_MODE_TYPE_DRIVER,
};

static int nt35597_wqxga_truly_get_modes(struct drm_panel *panel,
					 struct drm_connector *connector)
{
	return drm_connector_helper_get_modes_fixed(connector, &nt35597_wqxga_truly_mode);
}

static const struct drm_panel_funcs nt35597_wqxga_truly_panel_funcs = {
	.prepare = nt35597_wqxga_truly_prepare,
	.unprepare = nt35597_wqxga_truly_unprepare,
	.get_modes = nt35597_wqxga_truly_get_modes,
};

static int nt35597_wqxga_truly_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct nt35597_wqxga_truly *ctx;
	int ret;

	ctx = devm_drm_panel_alloc(dev, struct nt35597_wqxga_truly, panel,
				   &nt35597_wqxga_truly_panel_funcs,
				   DRM_MODE_CONNECTOR_DSI);
	if (IS_ERR(ctx))
		return PTR_ERR(ctx);

	ret = devm_regulator_bulk_get_const(dev,
					    ARRAY_SIZE(nt35597_wqxga_truly_supplies),
					    nt35597_wqxga_truly_supplies,
					    &ctx->supplies);
	if (ret < 0)
		return ret;

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS | MIPI_DSI_MODE_LPM;

	ctx->panel.prepare_prev_first = true;

	ret = drm_panel_of_backlight(&ctx->panel);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to get backlight\n");

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		drm_panel_remove(&ctx->panel);
		return dev_err_probe(dev, ret, "Failed to attach to DSI host\n");
	}

	return 0;
}

static void nt35597_wqxga_truly_remove(struct mipi_dsi_device *dsi)
{
	struct nt35597_wqxga_truly *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id nt35597_wqxga_truly_of_match[] = {
	{ .compatible = "mdss,nt35597-wqxga-truly" }, // FIXME
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, nt35597_wqxga_truly_of_match);

static struct mipi_dsi_driver nt35597_wqxga_truly_driver = {
	.probe = nt35597_wqxga_truly_probe,
	.remove = nt35597_wqxga_truly_remove,
	.driver = {
		.name = "panel-nt35597-wqxga-truly",
		.of_match_table = nt35597_wqxga_truly_of_match,
	},
};
module_mipi_dsi_driver(nt35597_wqxga_truly_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>"); // FIXME
MODULE_DESCRIPTION("DRM driver for Dual nt35597 video mode dsi truly panel without DSC");
MODULE_LICENSE("GPL");
