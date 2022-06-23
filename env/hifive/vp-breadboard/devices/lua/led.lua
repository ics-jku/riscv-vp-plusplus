classname = "LED"

-- no config functions

function getPinLayout ()
    -- number, [input | output | inout], name
    return  {1, "input", "led_on"}
end

function getGraphBufferLayout()
    -- x width, y width, data type (currently only rgba)
    return {1, 1, "rgba"}
end

function setPin(number, val)
	-- print ( tostring(number) .. ": " .. tostring(val))
	if number == 1 then
		if val then
			setGraphbuffer(0, 0, graphbuf.Pixel(255,0,0,255))
		else
			setGraphbuffer(0, 0, graphbuf.Pixel(255,0,0,100))
		end
	end
end


-- graphbuf.Pixel(r, g, b, a) Where values range from 0-255 where 255 is color/opaque
-- setGraphbuffer(x, y, Pixel)

