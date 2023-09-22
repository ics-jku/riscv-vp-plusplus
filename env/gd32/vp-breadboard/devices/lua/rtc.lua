classname = "RTC_0815"

-- no config functions

is_cs_active = False
last_time = 0

time_width_bytes = 4

function setCS(bool_on)
    -- print ("RTC: setCS")
    is_cs_active = bool_on
    if bool_on then
        last_time = os.time(os.date("!*t"))
        state = 0
    end
end

function receiveSPI(byte_in)
    -- print ("RTC: receiveSPI " .. byte_in)
    if is_cs_active then
        if byte_in > time_width_bytes then
            return 0
        else
            r = ((0xFF << (8*byte_in)) & last_time) >> (8*byte_in)
            return r
        end
    end
end