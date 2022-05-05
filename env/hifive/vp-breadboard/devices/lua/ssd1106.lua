classname = "SSD1106"

-- no config functions

function getPinLayout ()
    return 
        {1, "input", "data_command"}
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