// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 Stephan Gerhold
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2021 GOODIX
 * Copyright (C) 2026, XiKoTaSu <3329989998@qq.com>
 *
 * Minimal ASoC driver for NXP TFA9891 audio amplifier (WIP).
 * Register definitions adapted from TFA9891_I2C_list_V11.xls
 * (Tfa98xx_genregs.h). This driver is minimal – only basic audio playback.
 * vddd-supply is optional.
 */

#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <sound/soc.h>

/* ---------- Register definitions from Tfa98xx_genregs.h ---------- */
#define TFA98XX_STATUSREG              0x00
#define TFA98XX_BATTERYVOLTAGE         0x01
#define TFA9891_TEMPERATURE            0x02
#define TFA98XX_REVISIONNUMBER         0x03
#define TFA98XX_I2SREG                 0x04
#define TFA98XX_BAT_PROT               0x05
#define TFA98XX_AUDIO_CTR              0x06
#define TFA98XX_DCDCBOOST              0x07
#define TFA98XX_SPKR_CALIBRATION       0x08
#define TFA98XX_SYS_CTRL               0x09
#define TFA98XX_I2S_SEL_REG            0x0a
#define TFA98XX_INTERRUPT_REG          0x0f
#define TFA98XX_PDM_CTRL               0x10
#define TFA98XX_PDM_OUT_CTRL           0x11
#define TFA98XX_CTRL_SAAM_PGA          0x22
#define TFA98XX_MISC_CTRL              0x25
#define TFA98XX_CURRENTSENSE1          0x46
#define TFA98XX_CURRENTSENSE4          0x49
#define TFA9891_CF_CONTROLS            0x70
#define TFA9891_CF_MAD                 0x71
#define TFA9891_CF_MEM                 0x72
#define TFA9891_CF_STATUS              0x73

/* Status register bits */
#define TFA98XX_STATUSREG_VDDS         BIT(0)
#define TFA98XX_STATUSREG_PLLS         BIT(1)
#define TFA98XX_STATUSREG_OTDS         BIT(2)
#define TFA98XX_STATUSREG_OVDS         BIT(3)
#define TFA98XX_STATUSREG_UVDS         BIT(4)
#define TFA98XX_STATUSREG_OCDS         BIT(5)
#define TFA98XX_STATUSREG_CLKS         BIT(6)
#define TFA98XX_STATUSREG_CLIPS        BIT(7)
#define TFA98XX_STATUSREG_MTPB         BIT(8)
#define TFA98XX_STATUSREG_DCCS         BIT(9)
#define TFA98XX_STATUSREG_SPKS         BIT(10)
#define TFA98XX_STATUSREG_ACS          BIT(11)
#define TFA98XX_STATUSREG_SWS          BIT(12)
#define TFA98XX_STATUSREG_WDS          BIT(13)
#define TFA98XX_STATUSREG_AMPS         BIT(14)
#define TFA98XX_STATUSREG_AREFS        BIT(15)

/* I2SReg bits */
#define TFA98XX_I2SREG_I2SF            GENMASK(2,0)
#define TFA98XX_I2SREG_CHS12           GENMASK(4,3)
#define TFA98XX_I2SREG_CHS3            BIT(5)
#define TFA98XX_I2SREG_CHSA            GENMASK(7,6)
#define TFA98XX_I2SREG_I2SDOC          GENMASK(9,8)
#define TFA98XX_I2SREG_DISP            BIT(10)
#define TFA98XX_I2SREG_I2SDOE          BIT(11)
#define TFA98XX_I2SREG_I2SSR           GENMASK(15,12)

/* SysCtrl bits */
#define TFA98XX_SYS_CTRL_PWDN          BIT(0)
#define TFA98XX_SYS_CTRL_I2CR          BIT(1)
#define TFA98XX_SYS_CTRL_CFE           BIT(2)
#define TFA98XX_SYS_CTRL_AMPE          BIT(3)
#define TFA98XX_SYS_CTRL_DCA           BIT(4)
#define TFA98XX_SYS_CTRL_SBSL          BIT(5)
#define TFA98XX_SYS_CTRL_AMPC          BIT(6)
#define TFA98XX_SYS_CTRL_DCDIS         BIT(7)
#define TFA98XX_SYS_CTRL_PSDR          BIT(8)
#define TFA98XX_SYS_CTRL_DCCV          GENMASK(10,9)
#define TFA98XX_SYS_CTRL_CCFD          GENMASK(12,11)
#define TFA98XX_SYS_CTRL_ISEL          BIT(13)
#define TFA98XX_SYS_CTRL_IPLL          BIT(14)

/* I2S_sel_reg bits */
#define TFA98XX_I2S_SEL_REG_DOLS       GENMASK(2,0)
#define TFA98XX_I2S_SEL_REG_DORS       GENMASK(5,3)
#define TFA98XX_I2S_SEL_REG_SPKL       GENMASK(8,6)
#define TFA98XX_I2S_SEL_REG_SPKR       GENMASK(10,9)
#define TFA98XX_I2S_SEL_REG_DCFG       GENMASK(14,11)

/* Other useful defines */
#define TFA98XX_REVISIONNUMBER_REV     GENMASK(7,0)
#define TFA9891_REVISION               0x92

struct tfa989x_rev {
	unsigned int rev;
	int (*init)(struct regmap *regmap);
};

struct tfa989x {
	const struct tfa989x_rev *rev;
	struct regulator *vddd_supply;
};

static bool tfa989x_writeable_reg(struct device *dev, unsigned int reg)
{
	return reg > TFA98XX_REVISIONNUMBER;
}

static bool tfa989x_volatile_reg(struct device *dev, unsigned int reg)
{
	return reg <= TFA98XX_REVISIONNUMBER;
}

static const struct regmap_config tfa989x_regmap = {
	.reg_bits = 8,
	.val_bits = 16,
	.writeable_reg = tfa989x_writeable_reg,
	.volatile_reg = tfa989x_volatile_reg,
	.cache_type = REGCACHE_RBTREE,
};

/* DAPM widgets – minimal path from AIFIN to OUT */
static const struct snd_soc_dapm_widget tfa989x_dapm_widgets[] = {
	SND_SOC_DAPM_OUTPUT("OUT"),
	SND_SOC_DAPM_SUPPLY("POWER", TFA98XX_SYS_CTRL,
			    __builtin_ctz(TFA98XX_SYS_CTRL_PWDN), 1, NULL, 0),
	SND_SOC_DAPM_OUT_DRV("AMPE", TFA98XX_SYS_CTRL,
			     __builtin_ctz(TFA98XX_SYS_CTRL_AMPE), 0, NULL, 0),
	SND_SOC_DAPM_AIF_IN("AIFIN", "HiFi Playback", 0, SND_SOC_NOPM, 0, 0),
};

static const struct snd_soc_dapm_route tfa989x_dapm_routes[] = {
	{"OUT", NULL, "AMPE"},
	{"AMPE", NULL, "POWER"},
	{"AMPE", NULL, "AIFIN"},
};

static const struct snd_soc_component_driver tfa989x_component = {
	.dapm_widgets = tfa989x_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tfa989x_dapm_widgets),
	.dapm_routes = tfa989x_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(tfa989x_dapm_routes),
	.use_pmdown_time = 1,
	.endianness = 1,
};

static const unsigned int tfa989x_rates[] = {
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000
};

static int tfa989x_find_sample_rate(unsigned int rate)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(tfa989x_rates); i++)
		if (tfa989x_rates[i] == rate)
			return i;
	return -EINVAL;
}

static int tfa989x_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	int sr;

	sr = tfa989x_find_sample_rate(params_rate(params));
	if (sr < 0)
		return sr;

	return snd_soc_component_update_bits(component, TFA98XX_I2SREG,
					     TFA98XX_I2SREG_I2SSR,
					     (unsigned int)sr << 12);
}

static const struct snd_soc_dai_ops tfa989x_dai_ops = {
	.hw_params = tfa989x_hw_params,
};

static struct snd_soc_dai_driver tfa989x_dai = {
	.name = "tfa989x-hifi",
	.playback = {
		.stream_name = "HiFi Playback",
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.rate_min = 8000,
		.rate_max = 48000,
		.channels_min = 1,
		.channels_max = 2,
	},
	.ops = &tfa989x_dai_ops,
};

/* Minimal init sequence for TFA9891 – only essential registers */
static const struct reg_sequence tfa9891_reg_init[] = {
	/* Battery protection threshold (recommended default) */
	{ TFA98XX_BAT_PROT, 0x13AB },
	/* Audio control: enable all blocks, no mute */
	{ TFA98XX_AUDIO_CTR, 0x001F },
	/* Speaker calibration / peak voltage protection */
	{ TFA98XX_SPKR_CALIBRATION, 0x3C4E },
	/* System control: power up, enable amp, disable DSP initially */
	{ TFA98XX_SYS_CTRL, 0x024D },
	/* PWM control */
	{ TFA98XX_PDM_CTRL, 0x0000 },  /* I2S mode, not PDM */
	{ TFA98XX_PDM_OUT_CTRL, 0x0000 },
	/* Current sense configuration */
	{ TFA98XX_CURRENTSENSE4, 0x0E82 },
};

static int tfa9891_init(struct regmap *regmap)
{
	return regmap_multi_reg_write(regmap, tfa9891_reg_init,
				      ARRAY_SIZE(tfa9891_reg_init));
}

static const struct tfa989x_rev tfa9891_rev = {
	.rev = TFA9891_REVISION,
	.init = tfa9891_init,
};

/* DSP bypass: force direct I2S input, disable CoolFlux */
static int tfa989x_dsp_bypass(struct regmap *regmap)
{
	int ret;

	/* Select I2S1 left channel (CHSA = 0) */
	ret = regmap_update_bits(regmap, TFA98XX_I2SREG,
				 TFA98XX_I2SREG_CHSA, 0);
	if (ret)
		return ret;

	/* Set speaker impedance to 8 ohm, DCDC compensation off */
	ret = regmap_update_bits(regmap, TFA98XX_I2S_SEL_REG,
				 TFA98XX_I2S_SEL_REG_SPKR |
				 TFA98XX_I2S_SEL_REG_DCFG,
				 FIELD_PREP(TFA98XX_I2S_SEL_REG_SPKR, 3));
	if (ret)
		return ret;

	/* Disable CoolFlux DSP and DCDC boost (follower mode) */
	return regmap_clear_bits(regmap, TFA98XX_SYS_CTRL,
				 TFA98XX_SYS_CTRL_CFE |
				 TFA98XX_SYS_CTRL_DCA |
				 TFA98XX_SYS_CTRL_AMPC);
}

static void tfa989x_regulator_disable(void *data)
{
	struct tfa989x *tfa989x = data;
	if (tfa989x->vddd_supply)
		regulator_disable(tfa989x->vddd_supply);
}

static int tfa989x_i2c_probe(struct i2c_client *i2c)
{
	struct device *dev = &i2c->dev;
	const struct tfa989x_rev *rev;
	struct tfa989x *tfa989x;
	struct regmap *regmap;
	unsigned int val;
	int ret;

	rev = device_get_match_data(dev);
	if (!rev)
		return dev_err_probe(dev, -ENODEV, "missing match data\n");

	tfa989x = devm_kzalloc(dev, sizeof(*tfa989x), GFP_KERNEL);
	if (!tfa989x)
		return -ENOMEM;

	tfa989x->rev = rev;
	i2c_set_clientdata(i2c, tfa989x);

	/* Optional vddd-supply */
	tfa989x->vddd_supply = devm_regulator_get_optional(dev, "vddd");
	if (IS_ERR(tfa989x->vddd_supply)) {
		if (PTR_ERR(tfa989x->vddd_supply) == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		/* -ENODEV means no supply, that's fine */
		tfa989x->vddd_supply = NULL;
	}

	regmap = devm_regmap_init_i2c(i2c, &tfa989x_regmap);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	if (tfa989x->vddd_supply) {
		ret = regulator_enable(tfa989x->vddd_supply);
		if (ret)
			return dev_err_probe(dev, ret, "failed to enable vddd\n");
		ret = devm_add_action_or_reset(dev, tfa989x_regulator_disable, tfa989x);
		if (ret)
			return ret;
	}

	/* Bypass cache during initial setup */
	regcache_cache_bypass(regmap, true);

	/* Dummy read to generate I2C clocks (required on some platforms) */
	regmap_read(regmap, TFA98XX_REVISIONNUMBER, &val);

	ret = regmap_read(regmap, TFA98XX_REVISIONNUMBER, &val);
	if (ret)
		return dev_err_probe(dev, ret, "failed to read revision\n");

	val &= TFA98XX_REVISIONNUMBER_REV;
	if (val != rev->rev)
		return dev_err_probe(dev, -ENODEV,
				     "revision mismatch: got %#x, expected %#x\n",
				     val, rev->rev);

	/* Reset I2C registers */
	ret = regmap_write(regmap, TFA98XX_SYS_CTRL, TFA98XX_SYS_CTRL_I2CR);
	if (ret)
		return dev_err_probe(dev, ret, "I2C reset failed\n");

	/* Perform chip-specific init */
	ret = rev->init(regmap);
	if (ret)
		return dev_err_probe(dev, ret, "chip init failed\n");

	/* Bypass DSP for simple analog pass-through */
	ret = tfa989x_dsp_bypass(regmap);
	if (ret)
		return dev_err_probe(dev, ret, "DSP bypass failed\n");

	regcache_cache_bypass(regmap, false);

	return devm_snd_soc_register_component(dev, &tfa989x_component,
					       &tfa989x_dai, 1);
}

static const struct of_device_id tfa989x_of_match[] = {
	{ .compatible = "nxp,tfa9891", .data = &tfa9891_rev },
	{ }
};
MODULE_DEVICE_TABLE(of, tfa989x_of_match);

static struct i2c_driver tfa989x_i2c_driver = {
	.driver = {
		.name = "tfa9891",
		.of_match_table = tfa989x_of_match,
	},
	.probe = tfa989x_i2c_probe,
};
module_i2c_driver(tfa989x_i2c_driver);

MODULE_DESCRIPTION("Minimal ASoC driver for NXP TFA9891 audio amplifier");
MODULE_AUTHOR("XiKoTaSu <3329989998@qq.com>");
MODULE_LICENSE("GPL v2");
