/* PowerSaved3DS kernel module for linux
 * - Wunkolo <wunkolo@gmail.com>
 */

#include <linux/device.h>
#include <linux/input.h>
#include <linux/hid.h>
#include <linux/usb.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <stddef.h>

#include "usbhid/usbhid.h"

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

enum powersaves_ctr_command
{
	CTR_CMD_GetHeader      = 0x82
};

struct powersaves_command
{
	uint8_t command_id;
	uint16_t command_length;
	uint16_t response_length;
	union
	{
		uint8_t spi_command;
		uint8_t ctr_command;
		uint8_t ntr_command;
		uint8_t u8[59];
	};
} __attribute__((packed));

#define REPORT_SIZE sizeof(struct powersaves_command)

// PowerSaves driver data

struct powersaves_device
{
	struct hid_device* hid;
	struct usb_device* usb;
	struct urb* usb_urb;
	struct mutex mutex;
	// buffer of REPORT_SIZE for DMA tranfers
	uint8_t* buffer;
};

// URB_INTERRUPT transfers

static int powersaves_send_command(
	struct powersaves_device* powersaves,
	const struct powersaves_command* command
)
{
	int result = 0;
	int bytes_written = 0;
	memcpy(
		powersaves->buffer,
		command,
		REPORT_SIZE
	);

	mutex_lock(&powersaves->mutex);
	// result = hid_hw_raw_request(
	// 	powersaves->hid,
	// 	0,
	// 	powersaves->buffer,
	// 	REPORT_SIZE,
	// 	HID_OUTPUT_REPORT,
	// 	HID_REQ_SET_REPORT
	// );
	result = usb_interrupt_msg(
		powersaves->usb,
		usb_sndintpipe(
			powersaves->usb,
			0x1
		),
		powersaves->buffer,
		REPORT_SIZE,
		&bytes_written,
		100
	);
	mutex_unlock(&powersaves->mutex);

	if( result < 0 )
	{
		hid_err(
			powersaves->hid,
			"%s: error writing report: %d\n",
			__func__,
			result
		);
	}
	if( bytes_written != REPORT_SIZE )
	{
		hid_err(
			powersaves->hid,
			"%s: error writing full report: %d != %zu\n",
			__func__,
			bytes_written,
			REPORT_SIZE
		);
	}
	return result;
}

static int powersaves_recv(
	struct powersaves_device* powersaves,
	void* buffer,
	size_t size
)
{
	int result = 0;
	int bytes_read = 0;
	size_t readbytes = 0;

	mutex_lock(&powersaves->mutex);
	while( readbytes < size )
	{
		// result = hid_hw_raw_request(
		// 	powersaves->hid,
		// 	0,
		// 	powersaves->buffer,
		// 	REPORT_SIZE,
		// 	HID_INPUT_REPORT,
		// 	HID_REQ_GET_REPORT
		// );
		result = usb_interrupt_msg(
			powersaves->usb,
			usb_rcvintpipe(
				powersaves->usb,
				0x81
			),
			powersaves->buffer,
			REPORT_SIZE,
			&bytes_read,
			100
		);
		if( result >= 0 )
		{
			memcpy(
				buffer + readbytes,
				powersaves->buffer,
				min(size, REPORT_SIZE)
			);
			readbytes += bytes_read;
		}
		else
		{
			hid_err(
				powersaves->hid,
				"%s: error reading report %zu/%zu %d\n",
				__func__,
				readbytes,
				size,
				result
			);
			break;
		}
	}
	mutex_unlock(&powersaves->mutex);

	return result;
}

// PowerSaves command utils

int powersaves_get_game_id(
	struct powersaves_device* powersaves,
	uint32_t* game_id
)
{
	int result = 0;
	struct powersaves_command curcommand = { 0 };
	uint8_t* response = kzalloc(0x2000, GFP_KERNEL);
	uint8_t magic[8] = {
		0x71, 0xC9, 0x3F, 0xE9, 0xBB, 0x0A, 0x3B, 0x18
	};
	// Mode Switch
	curcommand = (struct powersaves_command){0};
	curcommand.command_id = CMD_ModeSwitch;
	powersaves_send_command(
		powersaves,
		&curcommand
	);

	// Mode: NTR
	curcommand = (struct powersaves_command){0};
	curcommand.command_id = CMD_ModeROM;
	powersaves_send_command(
		powersaves,
		&curcommand
	);

	// Test
	curcommand = (struct powersaves_command){0};
	curcommand.command_id = CMD_Test;
	curcommand.response_length = 64;
	powersaves_send_command(
		powersaves,
		&curcommand
	);
	powersaves_recv(
		powersaves,
		response,
		REPORT_SIZE
	);
	hid_info(
		powersaves->hid,
		"Test: %*phC\n",
		64,
		response
	);

	// NTR_Reset
	curcommand = (struct powersaves_command){0};
	curcommand.command_id = CMD_NTR;
	curcommand.command_length = 8;
	curcommand.response_length = 0x2000;
	curcommand.ntr_command = 0x9F;
	powersaves_send_command(
		powersaves,
		&curcommand
	);
	powersaves_recv(
		powersaves,
		response,
		0x2000
	);

	// Unknown
	curcommand = (struct powersaves_command){0};
	curcommand.command_id = CMD_NTR;
	curcommand.command_length = 8;
	memcpy(curcommand.u8, magic, 8);
	powersaves_send_command(
		powersaves,
		&curcommand
	);

	// Get gamecard ID
	curcommand = (struct powersaves_command){0};
	curcommand.command_id = CMD_NTR;
	curcommand.command_length = 8;
	curcommand.response_length = 4;
	curcommand.ntr_command = 0x90;
	powersaves_send_command(
		powersaves,
		&curcommand
	);
	powersaves_recv(
		powersaves,
		game_id,
		4
	);
	kzfree(response);
	return result;
}

int powersaves_get_spi_id(
	struct powersaves_device* powersaves,
	uint32_t* spi_id
)
{
	int result = 0;
	struct powersaves_command curcommand = { 0 };

	// Mode Switch
	curcommand = (struct powersaves_command){0};
	curcommand.command_id = CMD_ModeSwitch;
	powersaves_send_command(
		powersaves,
		&curcommand
	);

	// Mode: SPI
	curcommand = (struct powersaves_command){0};
	curcommand.command_id = CMD_ModeSPI;
	powersaves_send_command(
		powersaves,
		&curcommand
	);

	// SPI_RDID
	curcommand = (struct powersaves_command){0};
	curcommand.command_id = CMD_SPI;
	curcommand.command_length = 1;
	curcommand.response_length = 4;
	curcommand.spi_command = SPI_RDID;
	powersaves_send_command(
		powersaves,
		&curcommand
	);
	powersaves_recv(
		powersaves,
		spi_id,
		4
	);

	return result;
}

/// HID Callbacks

static int powersaves_probe(
	struct hid_device *hdev,
	const struct hid_device_id *id
)
{
	int result = 0;
	// Get USB data
	struct usb_device* usbdev = hid_to_usb_dev(hdev);
	struct powersaves_device* powersaves;
	uint32_t game_id = 0;
	uint32_t spi_id = 0;

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
		hid_err(
			hdev,
			"%s: error allocating driver data\n",
			__func__
		);
		return -ENOMEM;
	}

	// Init driver data
	powersaves->hid = hdev;
	powersaves->usb = usbdev;
	powersaves->usb_urb = usb_alloc_urb(0, GFP_KERNEL);
	if( !powersaves-> usb_urb)
	{
		hid_err(
			hdev,
			"%s: error allocating URB staging buffer\n",
			__func__
		);
		return -ENOMEM;
	}


	powersaves->buffer = devm_kzalloc(
		&hdev->dev,
		REPORT_SIZE,
		GFP_KERNEL
	);
	if( !powersaves->buffer )
	{
		hid_err(
			hdev,
			"%s: error allocating staging buffer\n",
			__func__
		);
		return -ENOMEM;
	}
	mutex_init(&powersaves->mutex);

	hid_set_drvdata(hdev, powersaves);

	// Start hardware
	result = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
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

	powersaves_get_game_id( powersaves, &game_id);
	hid_info(
		hdev,
		"Gamecart ID: %08X\n",
		game_id
	);
	powersaves_get_spi_id( powersaves, &spi_id);
	hid_info(
		hdev,
		"SPI ID: %.08X\n",
		spi_id
	);
	return result;
}

static void powersaves_remove(struct hid_device *hdev)
{
	struct powersaves_device* powersaves = NULL;
	hid_info(
		hdev,
		"Disconnect: %.128s on %.64s",
		hdev->name,
		hdev->phys
	);
	powersaves = hid_get_drvdata(hdev);

	if( !powersaves )
	{
		return;
	}

	hid_hw_stop(hdev);

	usb_free_urb(powersaves->usb_urb);
}

static const struct hid_device_id powersaves_device_list[] = {
	{ HID_USB_DEVICE( 0x1C1A, 0x03D5 ) },
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
