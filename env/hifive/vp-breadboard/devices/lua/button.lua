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

keybinding = 0

function getConfig()
	return {"keybinding", keybinding}
end

function setConfig(conf)
	keybinding = conf["keybinding"]
end

width = 35
height = 35

function getGraphBufferLayout()
	return {width, height, "rgba"}
end

function draw_area()
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

function mouse(active)
	is_active = not active
	draw_area()
end

function key(number, active)
	if number == keybinding then
		is_active = not active
		draw_area()
	end
end
