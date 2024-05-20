// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2010-2011 Canonical Ltd <jeremy.kerr@canonical.com>
 * Copyright (C) 2011-2012 Mike Turquette, Linaro Ltd <mturquette@linaro.org>
 *
 * Fixed rate clock implementation
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/iio/iio.h>

struct clk_iioproxy_struct {
	struct clk_hw 	hw;
	unsigned long	current_rate;
	unsigned long	fixed_accuracy;
	struct iio_dev	*indio_dev;
};
#define to_clk_iioproxy_struct(_hw) container_of(_hw, struct clk_iioproxy_struct, hw)

struct iioproxy_devpair {
	struct device_node *of_iio;
	struct device *dev_iio;
};

static unsigned long clk_iioproxy_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	
	return to_clk_iioproxy_struct(hw)->current_rate;
}

static int clk_iioproxy_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate)
{
	
	struct clk_iioproxy_struct *st;
	struct iio_dev *my_indio_dev;
	struct iio_chan_spec *indio_channels;
	struct iio_chan_spec indio_command;
	struct iio_info *indio_info;
	int ret;

	//printk("in clk_iioproxy_set_rate, requested: %ld\n",rate);
	st = to_clk_iioproxy_struct(hw);
	my_indio_dev = st->indio_dev;
	
	if (rate == st->current_rate)
		return 0;


	indio_channels = my_indio_dev->channels;
	if (indio_channels==NULL){
		dev_err(&my_indio_dev->dev, "IIO device has no channels\n");
		return -EINVAL;
	}
	indio_info=my_indio_dev->info;
	if (indio_info==NULL){
		dev_err(&my_indio_dev->dev, "IIO channel %d has no info attached\n", 0);
		return -EINVAL;
	}
	if (indio_info->write_raw==NULL){
		dev_err(&my_indio_dev->dev, "IIO channel %d has no write function attached\n", 0);
		return -EINVAL;
	}

	indio_command.channel=0;
	ret=(indio_info->write_raw)(my_indio_dev, &indio_command, rate, 0, IIO_CHAN_INFO_FREQUENCY);
	if (ret)
		return -EINVAL;

	st->current_rate = rate;

	return 0;
}

static long clk_iioproxy_round_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long *parent_rate)
{
	//printk("in clk_iioproxy_round_rate, requested: %ld\n",rate);
	return rate;
}

static unsigned long clk_iioproxy_recalc_accuracy(struct clk_hw *hw,
		unsigned long parent_accuracy)
{
	return to_clk_iioproxy_struct(hw)->fixed_accuracy;
}

const struct clk_ops clk_iioproxy_ops = {
	.set_rate = clk_iioproxy_set_rate,
	.round_rate = clk_iioproxy_round_rate,
	.recalc_rate = clk_iioproxy_recalc_rate,
	.recalc_accuracy = clk_iioproxy_recalc_accuracy,
};
EXPORT_SYMBOL_GPL(clk_iioproxy_ops);

struct clk_hw *clk_hw_register_iioproxy_with_accuracy(struct device *dev,
		const char *name, const char *parent_name, unsigned long flags,
		unsigned long current_rate, unsigned long fixed_accuracy)
{
	struct clk_iioproxy_struct *st;
	struct clk_hw *hw;
	struct clk_init_data init;
	int ret;

	st = kzalloc(sizeof(*st), GFP_KERNEL);
	if (!st)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.ops = &clk_iioproxy_ops;
	init.flags = flags;
	init.parent_names = (parent_name ? &parent_name: NULL);
	init.num_parents = (parent_name ? 1 : 0);

	/* struct clk_iioproxy assignments */
	st->current_rate = current_rate;
	st->fixed_accuracy = fixed_accuracy;
	st->hw.init = &init;

	/* register the clock */
	hw = &st->hw;
	ret = clk_hw_register(dev, hw);
	if (ret) {
		kfree(st);
		hw = ERR_PTR(ret);
	}

	return hw;
}
EXPORT_SYMBOL_GPL(clk_hw_register_iioproxy_with_accuracy);

struct clk *clk_register_iioproxy_with_accuracy(struct device *dev,
		const char *name, const char *parent_name, unsigned long flags,
		unsigned long current_rate, unsigned long fixed_accuracy)
{
	struct clk_hw *hw;

	hw = clk_hw_register_iioproxy_with_accuracy(dev, name, parent_name,
			flags, current_rate, fixed_accuracy);
	if (IS_ERR(hw))
		return ERR_CAST(hw);
	return hw->clk;
}
EXPORT_SYMBOL_GPL(clk_register_iioproxy_with_accuracy);

struct clk_hw *clk_hw_register_iioproxy(struct device *dev, const char *name,
		const char *parent_name, unsigned long flags,
		unsigned long current_rate)
{
	return clk_hw_register_iioproxy_with_accuracy(dev, name, parent_name,
						     flags, current_rate, 0);
}
EXPORT_SYMBOL_GPL(clk_hw_register_iioproxy);

struct clk *clk_register_iioproxy(struct device *dev, const char *name,
		const char *parent_name, unsigned long flags,
		unsigned long current_rate)
{
	return clk_register_iioproxy_with_accuracy(dev, name, parent_name,
						     flags, current_rate, 0);
}
EXPORT_SYMBOL_GPL(clk_register_iioproxy);

void clk_unregister_iioproxy(struct clk *clk)
{
	struct clk_hw *hw;

	hw = __clk_get_hw(clk);
	if (!hw)
		return;

	clk_unregister(clk);
	kfree(to_clk_iioproxy_struct(hw));
}
EXPORT_SYMBOL_GPL(clk_unregister_iioproxy);

void clk_hw_unregister_iioproxy(struct clk_hw *hw)
{
	struct clk_iioproxy_struct *st;

	st = to_clk_iioproxy_struct(hw);

	clk_hw_unregister(hw);
	kfree(st);
}
EXPORT_SYMBOL_GPL(clk_hw_unregister_iioproxy);

static int find_iio_from_platform_dev(struct device *dev, void *data)
{
	struct iioproxy_devpair *idp = data;
	int ret = 0;

	device_lock(dev);
	if ((idp->of_iio == dev->of_node) && dev->driver) {
		idp->dev_iio = dev;
		ret = 1;
	}
	device_unlock(dev);

	return ret;
}

#ifdef CONFIG_OF
static struct clk *_of_iioproxy_clk_setup(struct device_node *node)
{
	struct clk *clk;
	struct clk_hw *clk_hw;
	const char *clk_name = node->name;
	u32 initial_rate;
	u32 accuracy = 0;
	struct iioproxy_devpair idp;
	struct iio_dev *my_indio_dev;
	struct clk_iioproxy_struct *st;
	int ret;

	if (of_property_read_u32(node, "clock-frequency", &initial_rate))
		return ERR_PTR(-EIO);

	of_property_read_u32(node, "clock-accuracy", &accuracy);

	of_property_read_string(node, "clock-output-names", &clk_name);

	idp.of_iio = of_parse_phandle(node, "iiohwdev", 0);
	if (idp.of_iio)
		printk("clk-iioproxy: found iiohwdev entry\n");
	ret = bus_for_each_dev(&platform_bus_type, NULL, &idp, find_iio_from_platform_dev);
	if (ret)
		printk("clk-iioproxy: found platform device for entry\n");
	my_indio_dev = dev_get_drvdata(idp.dev_iio);
	if (my_indio_dev){
		printk("clk-iioproxy: found IIO device for platform device with name %s\n",my_indio_dev->name);
	}


	clk = clk_register_iioproxy_with_accuracy(NULL, clk_name, NULL, 0, initial_rate, accuracy);
	if (IS_ERR(clk))
		return clk;

	clk_hw = __clk_get_hw(clk);

	st = to_clk_iioproxy_struct(clk_hw);
	st->indio_dev = my_indio_dev;

	ret = of_clk_add_provider(node, of_clk_src_simple_get, clk);
	if (ret) {
		clk_unregister(clk);
		return ERR_PTR(ret);
	}

	st->current_rate = 0;
	clk_iioproxy_set_rate(clk_hw,initial_rate, 0);

	return clk;
}

/**
 * of_iioproxy_clk_setup() - Setup function for simple fixed rate clock
 */
void __init of_iioproxy_clk_setup(struct device_node *node)
{
	_of_iioproxy_clk_setup(node);
}
CLK_OF_DECLARE(iioproxy_clk, "iioproxy-clock", of_iioproxy_clk_setup);

static int of_iioproxy_clk_remove(struct platform_device *pdev)
{
	struct clk *clk = platform_get_drvdata(pdev);

	of_clk_del_provider(pdev->dev.of_node);
	clk_unregister_iioproxy(clk);

	return 0;
}

static int of_iioproxy_clk_probe(struct platform_device *pdev)
{
	struct clk *clk;

	/*
	 * This function is not executed when of_iioproxy_clk_setup
	 * succeeded.
	 */
	clk = _of_iioproxy_clk_setup(pdev->dev.of_node);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	platform_set_drvdata(pdev, clk);

	return 0;
}

static const struct of_device_id of_iioproxy_clk_ids[] = {
	{ .compatible = "iioproxy-clock" },
	{ }
};
MODULE_DEVICE_TABLE(of, of_iioproxy_clk_ids);

static struct platform_driver of_iioproxy_clk_driver = {
	.driver = {
		.name = "of_iioproxy_clk",
		.of_match_table = of_iioproxy_clk_ids,
	},
	.probe = of_iioproxy_clk_probe,
	.remove = of_iioproxy_clk_remove,
};
module_platform_driver(of_iioproxy_clk_driver);

MODULE_AUTHOR("Henning Paul <hnch@gmx.net>");
MODULE_DESCRIPTION("IIO proxy clock driver");
MODULE_LICENSE("GPL v2");
#endif
