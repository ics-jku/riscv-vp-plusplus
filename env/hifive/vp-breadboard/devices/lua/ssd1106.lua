classname = "SSD1106"

-- no config functions

function getPinLayout ()
    -- number, [input | output | inout], name
    return  {1, "input", "data_command"}
end

function getGraphBufferLayout()
    -- x width, y width, data type (currently only rgba)
    return {128, 64, "rgba"}
end

local dc

function getPin(number)
	if(number == 1)
	then
		return dc
	else
		return false
	end
end

function setPin(number, val)
	if number == 1
	then
		dc = val
	end
end


function receiveSPI(byte_in)
    print("SSD1106: Got byte")
    return byte_in
end