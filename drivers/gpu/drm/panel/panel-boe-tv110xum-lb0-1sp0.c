// SPDX-License-Identifier: GPL-2.0-only
/*
 * BOE TV110XUM-LB0-1SP0 panels driver
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/regulator/consumer.h>
#include <video/mipi_display.h>
#include <drm/drm_connector.h>
#include <drm/drm_crtc.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct panel_info {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi[2];
	const struct panel_desc *desc;

	struct backlight_device *backlight;
	struct regulator *vddio;
};

struct panel_desc {
	unsigned int width_mm;
	unsigned int height_mm;

	unsigned int bpc;
	unsigned int lanes;
	unsigned long mode_flags;
	enum mipi_dsi_pixel_format format;

	const struct drm_display_mode *modes;
	unsigned int num_modes;
	const struct mipi_dsi_device_info dsi_info;
	int (*init_sequence)(struct panel_info *pinfo);
};

static inline struct panel_info *to_panel_info(struct drm_panel *panel)
{
	return container_of(panel, struct panel_info, panel);
}

static int h7000_boe_init_sequence(struct panel_info *pinfo)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = pinfo->dsi[0] };
	/* No datasheet, so write magic init sequence directly */

	mipi_dsi_dcs_exit_sleep_mode_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 120);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x35, 0x5a, 0x0e, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x83, 0xac, 0xb6, 0x6d);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x50, 0x5a, 0x19);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x80,
					     0x92, 0x8e, 0x8c, 0x8a, 0x88, 0x87, 0x86,
					     0x84, 0x83, 0x82, 0x81, 0x81, 0x00, 0x50,
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

static const struct drm_display_mode h7000_boe_modes[] = {
	{
		.clock = (1600 + 25 + 14 + 25) * (2176 + 250 + 8 + 73) * 60 / 1000,
		.hdisplay = 1600,
		.hsync_start = 1600 + 25,
		.hsync_end = 1600 + 25 + 14,
		.htotal = 1600 + 25 + 14 + 25,
		.vdisplay = 2176,
		.vsync_start = 2176 + 250,
		.vsync_end = 2176 + 250 + 8,
		.vtotal = 2176 + 250 + 8 + 73,
	},
};

static const struct panel_desc h7000_boe_desc = {
	.modes = h7000_boe_modes,
	.num_modes = ARRAY_SIZE(h7000_boe_modes),
	.dsi_info = {
		.type = "BOE-h7000",
		.channel = 0,
		.node = NULL,
	},
	.width_mm = 147,
	.height_mm = 197,
	.bpc = 8,
	.lanes = 4,
	.format = MIPI_DSI_FMT_RGB888,
	.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_CLOCK_NON_CONTINUOUS | MIPI_DSI_MODE_LPM,
	.init_sequence = h7000_boe_init_sequence,
};

static int boe_tv110xum_lb0_1sp0_prepare(struct drm_panel *panel)
{
	struct panel_info *pinfo = to_panel_info(panel);
	int ret;

	ret = regulator_enable(pinfo->vddio);
	if (ret) {
		dev_err(panel->dev, "failed to enable vddio regulator: %d\n", ret);
		return ret;
	}

	ret = pinfo->desc->init_sequence(pinfo);
	if (ret < 0) {
		regulator_disable(pinfo->vddio);
		dev_err(panel->dev, "failed to initialize panel: %d\n", ret);
		return ret;
	}

	return 0;
}

static int boe_tv110xum_lb0_1sp0_disable(struct drm_panel *panel)
{
	struct panel_info *pinfo = to_panel_info(panel);
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = pinfo->dsi[0]};

	mipi_dsi_dcs_set_display_off_multi(&dsi_ctx);
	mipi_dsi_dcs_enter_sleep_mode_multi(&dsi_ctx);
	msleep(70);

	return 0;
}

static int boe_tv110xum_lb0_1sp0_unprepare(struct drm_panel *panel)
{
	struct panel_info *pinfo = to_panel_info(panel);

	regulator_disable(pinfo->vddio);
	return 0;
}

static void boe_tv110xum_lb0_1sp0_remove(struct mipi_dsi_device *dsi)
{
	struct panel_info *pinfo = mipi_dsi_get_drvdata(dsi);

	drm_panel_remove(&pinfo->panel);
}

static int boe_tv110xum_lb0_1sp0_get_modes(struct drm_panel *panel,
			       struct drm_connector *connector)
{
	struct panel_info *pinfo = to_panel_info(panel);
	int i;

	for (i = 0; i < pinfo->desc->num_modes; i++) {
		const struct drm_display_mode *m = &pinfo->desc->modes[i];
		struct drm_display_mode *mode;

		mode = drm_mode_duplicate(connector->dev, m);
		if (!mode) {
			dev_err(panel->dev, "failed to add mode %ux%u@%u\n",
				m->hdisplay, m->vdisplay, drm_mode_vrefresh(m));
			return -ENOMEM;
		}

		mode->type = DRM_MODE_TYPE_DRIVER;
		if (i == 0)
			mode->type |= DRM_MODE_TYPE_PREFERRED;

		drm_mode_set_name(mode);
		drm_mode_probed_add(connector, mode);
	}

	connector->display_info.width_mm = pinfo->desc->width_mm;
	connector->display_info.height_mm = pinfo->desc->height_mm;
	connector->display_info.bpc = pinfo->desc->bpc;

	return pinfo->desc->num_modes;
}

static const struct drm_panel_funcs boe_tv110xum_lb0_1sp0_panel_funcs = {
	.disable = boe_tv110xum_lb0_1sp0_disable,
	.prepare = boe_tv110xum_lb0_1sp0_prepare,
	.unprepare = boe_tv110xum_lb0_1sp0_unprepare,
	.get_modes = boe_tv110xum_lb0_1sp0_get_modes,
};

static int boe_tv110xum_lb0_1sp0_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi1;
	struct mipi_dsi_host *dsi1_host;
	struct panel_info *pinfo;
	const struct mipi_dsi_device_info *info;
	int i, ret;

	pinfo = devm_drm_panel_alloc(dev, struct panel_info, panel,
				     &boe_tv110xum_lb0_1sp0_panel_funcs,
				     DRM_MODE_CONNECTOR_DSI);
	if (IS_ERR(pinfo))
		return PTR_ERR(pinfo);

	pinfo->vddio = devm_regulator_get(dev, "vddio");
	if (IS_ERR(pinfo->vddio))
		return dev_err_probe(dev, PTR_ERR(pinfo->vddio), "failed to get vddio regulator\n");

	pinfo->desc = of_device_get_match_data(dev);
	if (!pinfo->desc)
		return -ENODEV;

	info = &pinfo->desc->dsi_info;

	dsi1 = of_graph_get_remote_node(dsi->dev.of_node, 1, -1);
	if (!dsi1) {
		dev_err(dev, "cannot get secondary DSI node.\n");
		return -ENODEV;
	}

	dsi1_host = of_find_mipi_dsi_host_by_node(dsi1);
	of_node_put(dsi1);
	if (!dsi1_host)
		return dev_err_probe(dev, -EPROBE_DEFER, "cannot get secondary DSI host\n");

	pinfo->dsi[1] = devm_mipi_dsi_device_register_full(dev, dsi1_host, info);
	if (IS_ERR(pinfo->dsi[1])) {
		dev_err(dev, "cannot get secondary DSI device\n");
		return PTR_ERR(pinfo->dsi[1]);
	}

	pinfo->dsi[0] = dsi;
	mipi_dsi_set_drvdata(dsi, pinfo);

	pinfo->panel.prepare_prev_first = true;

	ret = drm_panel_of_backlight(&pinfo->panel);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to get backlight\n");

	drm_panel_add(&pinfo->panel);

	for (i = 0; i < 2; i++) {
		pinfo->dsi[i]->lanes = pinfo->desc->lanes;
		pinfo->dsi[i]->format = pinfo->desc->format;
		pinfo->dsi[i]->mode_flags = pinfo->desc->mode_flags;

		ret = devm_mipi_dsi_attach(dev, pinfo->dsi[i]);
		if (ret < 0)
			return dev_err_probe(dev, ret, "cannot attach to DSI%d host.\n", i);
	}

	return 0;
}

static const struct of_device_id boe_tv110xum_lb0_1sp0_of_match[] = {
	{
		.compatible = "eebbk,h7000-boe-tv110xum-lb0-1sp0",
		.data = &h7000_boe_desc,
	},
	{},
};
MODULE_DEVICE_TABLE(of, boe_tv110xum_lb0_1sp0_of_match);

static struct mipi_dsi_driver boe_tv110xum_lb0_1sp0_driver = {
	.probe = boe_tv110xum_lb0_1sp0_probe,
	.remove = boe_tv110xum_lb0_1sp0_remove,
	.driver = {
		.name = "panel-boe-tv110xum-lb0-1sp0",
		.of_match_table = boe_tv110xum_lb0_1sp0_of_match,
	},
};
module_mipi_dsi_driver(boe_tv110xum_lb0_1sp0_driver);

MODULE_AUTHOR("");
MODULE_DESCRIPTION("DRM driver for BOE TV110XUM-LB0-1SP0 based MIPI DSI panels");
MODULE_LICENSE("GPL");
