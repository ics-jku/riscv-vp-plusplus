classname = "SSD1106"

-- no config functions

function getPinLayout ()
	-- number, [input | output | inout], name
	return  {1, "input", "data_command"}
end

local width = 132
local height = 64

function getGraphBufferLayout()
	-- x width, y width, data type (currently only rgba)
	return {width, height, "rgba"}
end

local isData

function setPin(number, val)
	if number == 1 then
		isData = val
	end
end

-- optional
function initializeGraphBuffer()
	for x = 0, width-1 do
		for y = 0, height-1 do
			setGraphbuffer(x, y, graphbuf.Pixel(0,0,0, 255))
		end
	end
end

operators = {
	COL_LOW	           = 0   ,
	COL_HIGH           = 0x10,
	PUMP_VOLTAGE       = 0x30, --0b00110000
	DISPLAY_START_LINE = 0x40, --0b01000000
	CONTRAST_MODE_SET  = 0x81, --0b10000001		//Double Command
	DISPLAY_ON         = 0xAE,
	PAGE_ADDR          = 0xB0,
	NOP                = 0xE3  --0b11100011
}

function getMask(op)
	if     op == operators.DISPLAY_START_LINE
	       	then return 0xC0 --0b11000000
	elseif op == operators.COL_LOW or
	       op == operators.COL_HIGH or
	       op == operators.PAGE_ADDR
	       	then return 0xF0 --0b11110000
	elseif op == operators.DISPLAY_ON
	       	then return 0xFE --0b11111110
	else return 0xFF --0b11111111
	end
end

function match(cmd)
	--print ("searching for " .. tostring(cmd))
	for key, op in pairs(operators) do
		--print ("testing " .. key .. " (" .. tostring(op) .. ") with mask " .. tostring(getMask(op)))
		--print (" difference: " .. tostring(cmd ~ op))
		if ( (cmd ~ op) & getMask(op) ) == 0 then
			--print ("Matched " .. key)
			return op, cmd & (~getMask(op))
		end
	end
	return operators.NOP, 0
end

local state = {
	column = 0,
	page = 0,
	contrast = 255,
	display_on = true
}

-- graphbuf.Pixel(r, g, b, a) Where values range from 0-255 where 255 is color/opaque
-- setGraphbuffer(x, y, Pixel)
function receiveSPI(byte_in)
	if isData then
		-- print( " data at " .. tostring(state.column))
		if state.column >= width then
			--print ( "LUA_OLED: Warning, exceeding column width")
			return 0
		end
		if state.page >= height/8 then
			print ( "LUA_OLED: Warning, exceeding page")
			return 0
		end
		for y = 0,7 do
			if (byte_in & 1 << y) > 0 then
				pix = 255
			else
				pix = 0
			end
			--print ( tostring(state.column) .. " x " .. tostring((state.page*8)+y) .. " to " .. tostring(pix))
			setGraphbuffer(state.column, (state.page*8)+y, graphbuf.Pixel(pix,pix,pix, state.contrast))
		end
		state.column = state.column + 1
	else
		op, payload = match(byte_in)
		if     op == operators.DISPLAY_START_LINE then
			return 0
		elseif op == operators.COL_LOW then
			state.column = (state.column & 0xf0) | payload
		elseif op == operators.COL_HIGH then
			state.column = (state.column & 0x0f) | (payload << 4)
		elseif op == operators.PAGE_ADDR then
			state.page = payload
		elseif op == operators.DISPLAY_ON then
			display_on = payload
		else
			print("unhandled operator " .. byte_in)
		end
	end
	return 0
end


function debug_printAll(table)
    print("given table:\n")
    for n in pairs(table) do print("\t" .. tostring(n)) end
    print("G:\n")
    for n in pairs(_G) do print("\t" .. tostring(n)) end
    print("graphbuf:\n")
    for n in pairs(graphbuf) do print("\t" .. tostring(n)) end
end
