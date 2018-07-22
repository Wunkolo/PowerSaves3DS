/* PowerSaved3DS kernel module for linux
 * - Wunkolo <wunkolo@gmail.com>
 */

#include <linux/device.h>
#include <linux/input.h>
#include <linux/hid.h>
#include <linux/module.h>

#include <stddef.h>

MODULE_AUTHOR( "Wunkolo <wunkolo@gmail.com>" );
MODULE_DESCRIPTION( "PowerSaves 3DS USB driver" );
MODULE_LICENSE( "GPL" );
MODULE_VERSION("0.1");

/// HID Callbacks

static int powersaves_probe(
	struct hid_device *hdev,
	const struct hid_device_id *id
)
{
	int result = 0;
	hid_info(
		hdev,
		"%.128s on %.64s",
		hdev->name,
		hdev->phys
	);

	return result;
}

static void powersaves_remove(struct hid_device *hdev)
{
	hid_info(
		hdev,
		"%.128s on %.64s",
		hdev->name,
		hdev->phys
	);
}

static const struct hid_device_id powersaves_device_list[] = {
	{ HID_USB_DEVICE(0x1C1A,0x03D5) },
	{}
};

MODULE_DEVICE_TABLE(hid,powersaves_device_list);

static struct hid_driver powersaves_driver = {
	.name = "hid-powersaves",
	.id_table = powersaves_device_list,
	.probe = powersaves_probe,
	.remove = powersaves_remove
};

module_hid_driver(powersaves_driver);
