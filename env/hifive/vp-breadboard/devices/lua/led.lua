classname = "LED"

-- no config functions

function getPinLayout ()
    -- number, [input | output | inout], name
    return  {1, "input", "led_on"}
end

local extent = 11
local extent_center = math.ceil(extent/2)

function getGraphBufferLayout()
    -- x width, y width, data type (currently only rgba)
    return {extent, extent, "rgba"}
end

local function setLED(r, g, b)
	for x = 1, extent do
		for y = 1, extent do
			local dist = math.sqrt((extent_center - x) ^ 2 + (extent_center - y) ^ 2)
			local norm_lumen = math.floor((1-dist/extent_center)*255)
			if norm_lumen < 0 then norm_lumen = 0 end
			if norm_lumen > 255 then norm_lumen = 255 end
			setGraphbuffer(x-1, y-1, graphbuf.Pixel(r, g, b, norm_lumen))
		end
	end
end

-- optional
function initializeGraphBuffer()
	setLED(50, 0, 0)
end

function setPin(number, val)
	-- print ( tostring(number) .. ": " .. tostring(val))
	if number == 1 then
		if val then
			setLED(255, 10, 10)
		else
			setLED(50, 0, 0)
		end
	end
end


-- graphbuf.Pixel(r, g, b, a) Where values range from 0-255 where 255 is color/opaque
-- setGraphbuffer(x, y, Pixel)

