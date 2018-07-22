/* PowerSaved3DS kernel module for linux
 * - Wunkolo <wunkolo@gmail.com>
 */

#include <linux/device.h>
#include <linux/input.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <stddef.h>


MODULE_AUTHOR( "Wunkolo <wunkolo@gmail.com>" );
MODULE_DESCRIPTION( "PowerSaves 3DS USB driver" );
MODULE_LICENSE( "GPL" );
MODULE_VERSION("0.1");

/// Structures
enum powersaves_command_id
{
	CMD_Reset       = 0x00,
	CMD_Test        = 0x02,
	CMD_Unknown4    = 0x04,
	CMD_Unknown5    = 0x05,
	CMD_Unknown6    = 0x06,
	CMD_Reboot8     = 0x08,
	CMD_Reboot9     = 0x09,
	CMD_ModeSwitch  = 0x10,
	CMD_ModeROM     = 0x11,
	CMD_ModeSPI     = 0x12,
	CMD_NTR         = 0x13, // 8 byte commands
	CMD_CTR         = 0x14, // 16 byte commands
	CMD_SPI         = 0x15  // 1 byte commands
};

enum powersaves_spi_command
{
	SPI_WRSR  = 0x01,  // Write status register
	SPI_PP    = 0x02,  // Page Program
	SPI_READ  = 0x03,  // Read data bytes
	SPI_WRDI  = 0x04,  // Write disable
	SPI_RDSR  = 0x05,  // Read status register
	SPI_WREN  = 0x06,  // Write enable
	SPI_PW    = 0x0A,  // Page Write
	SPI_FAST  = 0x0B,  // Fast Read
	SPI_BE    = 0x20,  // Block Erase
	SPI_RDID  = 0x9F,  // Read manufacture ID, memory ID, capacity ID
	SPI_RDP   = 0xAB,  // Release from deep power down
	SPI_DPD   = 0xB9,  // Deep power down
	SPI_CE    = 0xC7   // Chip Erase
};

enum powersaves_ntr_command
{
	NTR_CMD_Reset          = 0x9F,
	NTR_CMD_Header_Read    = 0x00,
	NTR_CMD_Header_ChipID  = 0x90,
	NTR_CMD_Activate_BF    = 0x3C,
	NTR_CMD_Activate_CMD16 = 0x3E,
	NTR_CMD_Activate_BF2   = 0x3D,
	NTR_CMD_Activate_SEC   = 0x40,
	NTR_CMD_Secute_ChipID  = 0x10,
	NTR_CMD_Secure_Read    = 0x20,
	NTR_CMD_Disable_SEC    = 0x60,
	NTR_CMD_Data_Mode      = 0xA0,
	NTR_CMD_Data_Read      = 0xB7,
	NTR_CMD_Data_ChipID    = 0xB8
};
struct powersaves_command
{
	uint8_t Zero;
	uint8_t ID;
	uint16_t CommandLength;
	uint16_t ResponseLength;
	union
	{
		uint8_t SPICommand;
		uint8_t u8[59];
	};
} __attribute__((packed));

////

struct powersaves_device
{
	struct hid_device* hid;
	struct mutex mutex;
};

static int powersaves_send_command(
	struct powersaves_device* powersaves,
	const struct powersaves_command* command
)
{
	u8* commandbuffer = NULL;
	int result = 0;
	commandbuffer = kmemdup(
		command,
		sizeof(struct powersaves_command),
		GFP_KERNEL
	);

	if( !commandbuffer )
	{
		return -ENOMEM;
	}
	mutex_lock(&powersaves->mutex);
	result = hid_hw_output_report(
		powersaves->hid,
		commandbuffer,
		sizeof(struct powersaves_command)
	);
	mutex_unlock(&powersaves->mutex);

	kfree(commandbuffer);

	return result;
}

static int powersaves_recv(
	struct powersaves_device* powersaves,
	u8* buffer,
	unsigned int size
)
{
	int result = 0;

	mutex_lock(&powersaves->mutex);
	result = hid_hw_raw_request(
		powersaves->hid,
		0,
		buffer,
		65,
		HID_OUTPUT_REPORT,
		HID_REQ_GET_REPORT
	);
	mutex_unlock(&powersaves->mutex);
	return result;
}

/// HID Callbacks

static int powersaves_probe(
	struct hid_device *hdev,
	const struct hid_device_id *id
)
{
	int result = 0;
	struct powersaves_device* powersaves;
	// struct powersaves_command curcommand = { 0 };
	// u8 Response[64];
	// u8 Magic[] = {
	// 	0x71, 0xC9, 0x3F, 0xE9, 0xBB, 0x0A, 0x3B, 0x18
	// };

	hid_info(
		hdev,
		"Connected: %.128s on %.64s",
		hdev->name,
		hdev->phys
	);

	// Parse HW reports
	result = hid_parse(hdev);
	if( result )
	{
		hid_err(
			hdev,
			"%s: error parsing HID interface\n",
			__func__
		);
		return result;
	}

	// Allocate driver data
	powersaves = devm_kzalloc(
		&hdev->dev,
		sizeof(struct powersaves_device),
		GFP_KERNEL
	);
	if( !powersaves )
	{
		return -ENOMEM;
	}

	// Init driver data
	powersaves->hid = hdev;
	mutex_init(&powersaves->mutex);

	hid_set_drvdata(hdev,powersaves);

	// Start hardware
	result = hid_hw_start(hdev, HID_CONNECT_HIDRAW);
	if( result )
	{
		hid_err(
			hdev,
			"%s: failed to start hardware\n",
			__func__
		);
		return result;
	}

	hid_info(
		hdev,
		"Initialized: %.128s on %.64s",
		hdev->name,
		hdev->phys
	);

	// Read card header

	// Mode switch
	//curcommand.ID = CMD_Test;
	
	// powersaves_send_command(
	// 	powersaves,
	// 	&curcommand
	// );
	// powersaves_recv(
	// 	powersaves,
	// 	Response,
	// 	64
	// );

	return result;
}

static void powersaves_remove(struct hid_device *hdev)
{
	hid_info(
		hdev,
		"Disconnect: %.128s on %.64s",
		hdev->name,
		hdev->phys
	);
	struct powersaves_device* powersaves = hdev->driver_data;

	if( !powersaves )
	{
		return;
	}

	hid_hw_stop(hdev);
}

static const struct hid_device_id powersaves_device_list[] = {
	{ HID_USB_DEVICE(0x1C1A,0x03D5) },
	{}
};

MODULE_DEVICE_TABLE(hid,powersaves_device_list);

static struct hid_driver powersaves_driver = {
	.name     = "hid-powersaves",
	.id_table = powersaves_device_list,
	.probe    = powersaves_probe,
	.remove   = powersaves_remove
};

module_hid_driver(powersaves_driver);

// https://github.com/torvalds/linux/blob/master/drivers/hid/hid-steam.c
// https://github.com/torvalds/linux/tree/master/drivers/hid