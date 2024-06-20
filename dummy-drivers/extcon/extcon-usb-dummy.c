// SPDX-License-Identifier: GPL-2.0-only
/*
 * drivers/extcon/extcon-usb-dummy.c - USB Dummy extcon driver
 *
 * Author: Pan Junzhong <junzhong.>
 */

#include <linux/extcon-provider.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/pinctrl/consumer.h>
#include <linux/mod_devicetable.h>
#include <linux/debugfs.h>

#define USB_DUMMY_DEBOUNCE_MS	20	/* ms */

struct usb_extcon_info {
	struct device *dev;
	struct extcon_dev *edev;

	struct dentry *debugfs_dir;
	struct dentry *debugfs_id;
	struct dentry *debugfs_vbus;
	int id_value;
	int vbus_value;

	bool have_vbus_support;
	bool have_id_support;

	unsigned long debounce_jiffies;
	struct delayed_work wq_detcable;
};

static const unsigned int usb_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_NONE,
};

/*
 * "USB" = VBUS and "USB-HOST" = !ID, so we have:
 * Both "USB" and "USB-HOST" can't be set as active at the
 * same time so if "USB-HOST" is active (i.e. ID is 0)  we keep "USB" inactive
 * even if VBUS is on.
 *
 *  State              |    ID   |   VBUS
 * ----------------------------------------
 *  [1] USB            |    H    |    H
 *  [2] none           |    H    |    L
 *  [3] USB-HOST       |    L    |    H
 *  [4] USB-HOST       |    L    |    L
 *
 * In case we have only one of these signals:
 * - VBUS only - we want to distinguish between [1] and [2], so ID is always 1.
 * - ID only - we want to distinguish between [1] and [4], so VBUS = ID.
*/
static void usb_extcon_detect_cable(struct work_struct *work)
{
	int id, vbus;
	struct usb_extcon_info *info = container_of(to_delayed_work(work),
							struct usb_extcon_info,
							wq_detcable);
	id = info->have_id_support ? info->id_value : 1;
	vbus = info->have_vbus_support ? info->vbus_value : id;
	if (!info->edev)
		return;

	/* at first we clean states which are no longer active */
	if (id)
		extcon_set_state_sync(info->edev, EXTCON_USB_HOST, false);
	if (!vbus)
		extcon_set_state_sync(info->edev, EXTCON_USB, false);

	if (!id) {
		extcon_set_state_sync(info->edev, EXTCON_USB_HOST, true);
	} else {
		if (vbus)
			extcon_set_state_sync(info->edev, EXTCON_USB, true);
	}
}

static ssize_t id_value_write(struct file *file, const char __user *user_buf,
							  size_t count, loff_t *ppos)
{
	struct usb_extcon_info *info = file->private_data;
	int val;
	if (kstrtoint_from_user(user_buf, count, 0, &val) == 0) {
		info->id_value = val;
		queue_delayed_work(system_power_efficient_wq, &info->wq_detcable,
				info->debounce_jiffies);
		return count;
	}
	return -EINVAL;
}

static ssize_t vbus_value_write(struct file *file, const char __user *user_buf,
								size_t count, loff_t *ppos)
{
	struct usb_extcon_info *info = file->private_data;
	int val;
	if (kstrtoint_from_user(user_buf, count, 0, &val) == 0) {
		info->vbus_value = val;
		queue_delayed_work(system_power_efficient_wq, &info->wq_detcable,
				info->debounce_jiffies);
		return count;
	}
	return -EINVAL;
}

static ssize_t id_value_read(struct file *file, char __user *user_buf,
							 size_t count, loff_t *ppos)
{
	struct usb_extcon_info *info = file->private_data;
	char buf[20]; // Assuming a reasonable size for the buffer
	int len;	
	// Format id_value info a string buffer
	len = snprintf(buf, sizeof(buf), "%d\n", info->id_value);
	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t vbus_value_read(struct file *file, char __user *user_buf,
							   size_t count, loff_t *ppos)
{
	struct usb_extcon_info *info = file->private_data;
	char buf[20]; // Assuming a reasonable size for the buffer
	int len;
	// Format id_value info a string buffer
	len = snprintf(buf, sizeof(buf), "%d\n", info->vbus_value);
	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations id_value_fops = {
	.open = simple_open,
	.read = id_value_read,
	.write = id_value_write,
	.llseek = no_llseek,
};

static const struct file_operations vbus_value_fops = {
	.open = simple_open,
	.read = vbus_value_read,
	.write = vbus_value_write,
	.llseek = no_llseek,
};

static int usb_extcon_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct usb_extcon_info *info;
	int ret;

	if (!np)
		return -EINVAL;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = dev;


	info->edev = devm_extcon_dev_allocate(dev, usb_extcon_cable);
	if (IS_ERR(info->edev)) {
		dev_err(dev, "failed to allocate extcon device\n");
		return -ENOMEM;
	}

	ret = devm_extcon_dev_register(dev, info->edev);
	if (ret < 0) {
		dev_err(dev, "failed to register extcon device\n");
		return ret;
	}

	info->debounce_jiffies = msecs_to_jiffies(USB_DUMMY_DEBOUNCE_MS);

	INIT_DELAYED_WORK(&info->wq_detcable, usb_extcon_detect_cable);

	platform_set_drvdata(pdev, info);
	device_set_wakeup_capable(&pdev->dev, true);

	info->id_value = device_property_read_bool(dev, "default-id");
	info->vbus_value = device_property_read_bool(dev, "default-vbus");
	info->have_vbus_support = device_property_read_bool(dev, "vbus-det");
	info->have_id_support = device_property_read_bool(dev, "id-det");

	/* Perform initial detection */
	usb_extcon_detect_cable(&info->wq_detcable.work);

	info->debugfs_dir = debugfs_create_dir("dummy_usb_extcon", NULL);
	if (!info->debugfs_dir) {
		ret = -ENODEV;
		return ret;
	}

	info->debugfs_id = debugfs_create_file("id", 0644, info->debugfs_dir,
										   info, &id_value_fops);
	if (!info->debugfs_id) {
		ret = -ENODEV;
		return ret;
	}

	info->debugfs_vbus = debugfs_create_file("vbus", 0644, info->debugfs_dir,
											 info, &vbus_value_fops);
	if (!info->debugfs_vbus) {
		ret = -ENODEV;
		return ret;
	}

	queue_delayed_work(system_power_efficient_wq, &info->wq_detcable,
		info->debounce_jiffies);

	return 0;
}

static int usb_extcon_remove(struct platform_device *pdev)
{
	struct usb_extcon_info *info = platform_get_drvdata(pdev);
	debugfs_remove(info->debugfs_id);
	debugfs_remove(info->debugfs_vbus);
	debugfs_remove(info->debugfs_dir);
	cancel_delayed_work_sync(&info->wq_detcable);
	device_init_wakeup(&pdev->dev, false);
	return 0;
}


static const struct of_device_id usb_extcon_dt_match[] = {
	{ .compatible = "linux,extcon-usb-dummy", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, usb_extcon_dt_match);

static const struct platform_device_id usb_extcon_platform_ids[] = {
	{ .name = "extcon-usb-dummy", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(platform, usb_extcon_platform_ids);

static struct platform_driver usb_extcon_driver = {
	.probe		= usb_extcon_probe,
	.remove		= usb_extcon_remove,
	.driver		= {
		.name	= "extcon-usb-dummy",
		.of_match_table = usb_extcon_dt_match,
	},
	.id_table = usb_extcon_platform_ids,
};

static int __init usb_extcon_init(void)
{
	return platform_driver_register(&usb_extcon_driver);
}
subsys_initcall(usb_extcon_init);

static void __exit usb_extcon_exit(void)
{
	platform_driver_unregister(&usb_extcon_driver);
}
module_exit(usb_extcon_exit);

MODULE_AUTHOR("Pan Junzhong <junzhong.>");
MODULE_DESCRIPTION("USB Dummy extcon driver");
MODULE_LICENSE("GPL v2");
