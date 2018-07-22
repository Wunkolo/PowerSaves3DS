--[[
	Wunkolo<wunkolo@gmail.com> 2018
	PowerSaves3DS dissector for use within WireShark
]]--

local CommandIDs = {
	[0x00] = "CMD_Reset",
	[0x02] = "CMD_Test",
	[0x04] = "CMD_Unknown4",
	[0x05] = "CMD_Unknown5",
	[0x06] = "CMD_Unknown6",
	[0x08] = "CMD_Reboot8",
	[0x09] = "CMD_Reboot9",
	[0x10] = "CMD_ModeSwitch",
	[0x11] = "CMD_ModeROM",
	[0x12] = "CMD_ModeSPI",
	[0x13] = "CMD_NTR",
	[0x14] = "CMD_CTR",
	[0x15] = "CMD_SPI",
}

local SPICommands = {
	[0x01] = "SPI_WRSR",  -- Write status register
	[0x02] = "SPI_PP",    -- Page Program
	[0x03] = "SPI_READ",  -- Read data bytes
	[0x04] = "SPI_WRDI",  -- Write disable
	[0x05] = "SPI_RDSR",  -- Read status register
	[0x06] = "SPI_WREN",  -- Write enable
	[0x0A] = "SPI_PW",    -- Page Write
	[0x0B] = "SPI_FAST",  -- Fast Read
	[0x20] = "SPI_BE",    -- Block Erase
	[0x9F] = "SPI_RDID",  -- Read manufacture ID, memory ID, capacity ID
	[0xAB] = "SPI_RDP",   -- Release from deep power down
	[0xB9] = "SPI_DPD",   -- Deep power down
	[0xC7] = "SPI_CE"     -- Chip Erase
}

local NTRCommands = {
	[0x9F] = "NTR_CMD_Dummy",
	[0x00] = "NTR_CMD_Header_Read",
	[0x90] = "NTR_CMD_Header_ChipID",
	[0x3C] = "NTR_CMD_Activate_BF",
	[0x3E] = "NTR_CMD_Activate_CMD16",
	[0x3D] = "NTR_CMD_Activate_BF2",
	[0x40] = "NTR_CMD_Activate_SEC",
	[0x10] = "NTR_CMD_Secute_ChipID",
	[0x20] = "NTR_CMD_Secure_Read",
	[0x60] = "NTR_CMD_Disable_SEC",
	[0xA0] = "NTR_CMD_Data_Mode",
	[0xB7] = "NTR_CMD_Data_Read",
	[0xB8] = "NTR_CMD_Data_ChipID"
}

local CTRCommands = {
	[0x82] = "CTR_CMD_GetHeader",
}

powersave_proto = Proto("powersaves3ds","PowerSave 3DS Dongle protocol")

local Fields = powersave_proto.fields
Fields.Command = ProtoField.uint8(
	"powersaves3ds.command",
	"Command",
	base.DEC_HEX,
	CommandIDs
)
Fields.CommandLength = ProtoField.uint16(
	"powersaves3ds.commandLength",
	"Command Length",
	base.DEC_HEX
)
Fields.ResponseLength = ProtoField.uint16(
	"powersaves3ds.responseLength",
	"Response Length",
	base.DEC_HEX
)
Fields.CommandData = ProtoField.bytes(
	"powersaves3ds.commanddata",
	"Command Data"
)
Fields.ResponseData = ProtoField.bytes(
	"powersaves3ds.responsedata",
	"Response Data"
)
Fields.CommandDataNTR = ProtoField.uint8(
	"powersaves3ds.commandntr",
	"NTR Command"
)
Fields.CommandDataCTR = ProtoField.bytes(
	"powersaves3ds.commandctr",
	"CTR Command"
)
Fields.CommandDataSPI = ProtoField.uint8(
	"powersaves3ds.commandspi",
	"SPI Command",
	base.DEC_HEX,
	SPICommands
)


local USBDirection = Field.new("usb.endpoint_address.direction")
local USBType = Field.new("usb.transfer_type")

function powersave_proto.dissector(buffer,pinfo,tree)
	if( USBType().value ~= 0x1 ) then
		return -- only process URB_INTERRUPTs
	end
	local subtree = tree:add(powersave_proto,buffer(),"PowerSave Protocol Data")
	local USBDir = USBDirection().value
	pinfo.cols["protocol"] = "PowerSaves"
	-- Outgoing packet/command
	if( USBDir == 0) then
		local CommandID = buffer(0,1):uint()
		pinfo.cols["info"] = CommandIDs[CommandID]
		subtree:add(Fields.Command,CommandID)
		subtree:add_le(Fields.CommandLength,buffer(1,2))
		subtree:add_le(Fields.ResponseLength,buffer(3,2))
		local datatree = subtree:add_le(Fields.CommandData,buffer(5,59))

		-- CMD_NTR
		if( CommandID == 0x13 ) then
			datatree:add_le(Fields.CommandDataNTR,buffer(5,1))
			pinfo.cols["info"]:append(
				": " .. ( NTRCommands[buffer(5,1):uint()] or buffer(5,8))
			)
		-- CMD_CTR
		elseif( CommandID == 0x14 ) then
			datatree:add_le(Fields.CommandDataCTR,buffer(5,16))
			pinfo.cols["info"]:append(
				": " .. ( CTRCommands[buffer(5,1):uint()] or buffer(5,16))
			)
		-- CMD_SPI
		elseif( CommandID == 0x15 ) then
			datatree:add_le(Fields.CommandDataSPI,buffer(5,1))
			local SPICommand = buffer(5,1):uint()
			pinfo.cols["info"]:append(
				": " .. ( SPICommands[SPICommand] or SPICommand)
			)
			-- SPI_READ
			if( SPICommand == 0x03) then
				local ReadAddress = buffer(6,3):uint()
				pinfo.cols["info"]:append(
					" (" .. string.format("0x%X",ReadAddress) .. ")"
				)
			end
		end
	-- Incoming packet
	elseif( USBDir == 1) then
		pinfo.cols["info"] = "DATA_IN: " .. buffer(0,64)
		local datatree = subtree:add_le(Fields.ResponseData,buffer(0,64))
	end
end


function powersave_proto.init()
    local USBProductTable = DissectorTable.get("usb.product")
    USBProductTable:add(0x1c1a03d5,powersave_proto)
end