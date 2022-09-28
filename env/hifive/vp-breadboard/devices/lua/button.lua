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

function getGraphBufferLayout()
	return {2, 2, "rgba"}
end

function drawArea()
	for x = 0, buffer_width-1 do
		for y = 0, buffer_height-1 do
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
