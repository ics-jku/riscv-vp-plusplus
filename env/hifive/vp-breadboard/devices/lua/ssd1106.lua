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


-- graphbuf.Pixel(r, g, b, a) Where values range from 0-255 where 255 is color/opaque
-- setGraphbuffer(x, y, Pixel)

function receiveSPI(byte_in)
    setGraphbuffer(50, 20, graphbuf.Pixel(byte_in, 255-byte_in, 0, 255))
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