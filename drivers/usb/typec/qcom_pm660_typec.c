// SPDX-License-Identifier: GPL-2.0-only

#include <linux/bits.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/role.h>

#define TYPE_C_STATUS_3_REG			(0x0d)
#define ENABLE_BANDGAP_BIT			BIT(7)
#define U_USB_GND_NOVBUS_BIT			BIT(6)
#define U_USB_FLOAT_NOVBUS_BIT			BIT(5)
#define U_USB_GND_BIT				BIT(4)
#define U_USB_FMB1_BIT				BIT(3)
#define U_USB_FLOAT1_BIT			BIT(2)
#define U_USB_FMB2_BIT				BIT(1)
#define U_USB_FLOAT2_BIT			BIT(0)

struct pm660_typec {
	struct device		*dev;
	struct regmap		*regmap;
	u32			base;
	int			irq;
	struct usb_role_switch	*role_sw;
	struct regulator	*vbus;
	bool			vbus_enabled;
	enum usb_role		role;
};

static enum usb_role pm660_typec_decode(unsigned int status)
{
    if (!!(status & (U_USB_GND_NOVBUS_BIT | U_USB_GND_BIT)))
        return USB_ROLE_HOST;
    return USB_ROLE_NONE;
}

static int pm660_typec_apply(struct pm660_typec *typec)
{
	unsigned int status;
	enum usb_role role;
	int ret;
    printk("### irq\n");
	ret = regmap_read(typec->regmap,
			  typec->base + TYPE_C_STATUS_3_REG, &status);
	if (ret)
		return ret;

	role = pm660_typec_decode(status);
    switch (role) {
        case USB_ROLE_NONE:
            printk("### none 0x%02x %d\n", status, status);
            break;
        case USB_ROLE_DEVICE:
            printk("### device 0x%02x %d\n", status, status);
            break;
        case USB_ROLE_HOST:
            printk("### host 0x%02x %d\n", status, status);
    }

	if (role == typec->role)
		return 0;

	if (typec->vbus) {
		bool want_vbus = (role == USB_ROLE_HOST);

		if (want_vbus && !typec->vbus_enabled) {
			ret = regulator_enable(typec->vbus);
			if (ret)
				return ret;
			typec->vbus_enabled = true;
		} else if (!want_vbus && typec->vbus_enabled) {
			regulator_disable(typec->vbus);
			typec->vbus_enabled = false;
		}
	}

	ret = usb_role_switch_set_role(typec->role_sw, role);
	if (ret)
		return ret;

	typec->role = role;
	return 0;
}

static irqreturn_t pm660_typec_isr(int irq, void *data)
{
	pm660_typec_apply(data);
	return IRQ_HANDLED;
}

static int pm660_typec_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct fwnode_handle *connector;
	struct pm660_typec *typec;
	int ret;

	typec = devm_kzalloc(dev, sizeof(*typec), GFP_KERNEL);
	if (!typec)
		return -ENOMEM;

	typec->dev = dev;
	typec->role = USB_ROLE_NONE;

	typec->regmap = dev_get_regmap(dev->parent, NULL);
	if (!typec->regmap)
		return -ENODEV;

	ret = of_property_read_u32_index(dev->of_node, "reg", 0, &typec->base);
	if (ret)
		return ret;

	typec->irq = platform_get_irq_byname(pdev, "type-c-change");
	if (typec->irq < 0)
		return typec->irq;

	connector = device_get_named_child_node(dev, "connector");
	if (!connector)
		return -EINVAL;

	typec->role_sw = fwnode_usb_role_switch_get(connector);
	fwnode_handle_put(connector);
	if (IS_ERR(typec->role_sw))
		return PTR_ERR(typec->role_sw);

	typec->vbus = devm_regulator_get_optional(dev, "vdd-vbus");
	if (IS_ERR(typec->vbus)) {
		if (PTR_ERR(typec->vbus) != -ENODEV) {
			ret = PTR_ERR(typec->vbus);
			goto err_role_put;
		}
		typec->vbus = NULL;
	}

	platform_set_drvdata(pdev, typec);

	pm660_typec_apply(typec);

	ret = devm_request_threaded_irq(dev, typec->irq, NULL,
					pm660_typec_isr, IRQF_ONESHOT,
					dev_name(dev), typec);
	if (ret)
		goto err_role_put;

	return 0;

err_role_put:
	usb_role_switch_put(typec->role_sw);
	return ret;
}

static void pm660_typec_remove(struct platform_device *pdev)
{
	struct pm660_typec *typec = platform_get_drvdata(pdev);

	if (typec->vbus_enabled)
		regulator_disable(typec->vbus);
	usb_role_switch_put(typec->role_sw);
}

static const struct of_device_id pm660_typec_of_match[] = {
	{ .compatible = "qcom,pm660-typec" },
	{ }
};
MODULE_DEVICE_TABLE(of, pm660_typec_of_match);

static struct platform_driver pm660_typec_driver = {
	.probe		= pm660_typec_probe,
	.remove		= pm660_typec_remove,
	.driver		= {
		.name		= "qcom-pm660-typec",
		.of_match_table	= pm660_typec_of_match,
	},
};
module_platform_driver(pm660_typec_driver);

MODULE_AUTHOR("");
MODULE_DESCRIPTION("Qualcomm pm660 USB Type-C role-switch driver");
MODULE_LICENSE("GPL");