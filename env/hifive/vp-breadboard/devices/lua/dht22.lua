classname = "DHT22"

function getConfig ()
    return 
        {"temperature", temp} ,
        {"humidity", humid}
        -- {"invalid config"}
    end

function setConfig(conf)
    -- user has to keep getConfig and setConfig in sync!
    temp = conf["temperature"]
    humid = conf["humidity"]
end


is_cs_active = False

temp = 22
humid = 70

function setCS(bool_on)
    -- print("DHT22: setCS")
    is_cs_active = bool_on
end

function receiveSPI(byte_in)
    -- print("DHT22: receiveSPI")
    if is_cs_active then
        return temp
    end
end