#pragma once
#include <stdint.h>
#include <assert.h>

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

union powersaves_command
{
	struct
	{
		uint8_t Zero;
		uint8_t ID;
		uint16_t CommandLength;
		uint16_t ResponseLength;
		union
		{
			uint8_t SPICommand;
			uint8_t u8[59];
		}
	} __attribute__((packed));
	uint8_t u8[65];
} __attribute__((packed));

static_assert(
	sizeof( union powersaves_command ) == 65,
	"Invalid powersaves_command size(must be 65 bytes)"
);
