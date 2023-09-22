classname = "button_lua"

function getPinLayout ()
	return {1, "output", "output"}
end

is_active = true

function getPin(number)
	if number == 1 then
		return is_active
	end
end

width = 25
height = 25

function getGraphBufferLayout()
	return {width, height, "rgba"}
end

function drawArea()
	for x = 0, width-1 do
		for y = 0, height-1 do
			if is_active then
				setGraphbuffer(x, y, graphbuf.Pixel(0,0,0,128))
			else 
				setGraphbuffer(x, y, graphbuf.Pixel(255,0,0,128))
			end
		end
	end
end

function initializeGraphBuffer()
	drawArea()
end

function onClick(active)
	is_active = not active
	drawArea()
end

function onKeypress(key, active)
	onClick(active)
end
