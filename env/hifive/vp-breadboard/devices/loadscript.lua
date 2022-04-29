local cage = {
  -- import some packages:
  string = string,  coroutine = coroutine,
  table = table, math = math,
  
  -- some 'global' functions:
  next = next, ipairs = ipairs, pairs = pairs,
  require = require, type = type,
  tonumber = tonumber, tostring = tostring,
  unpack = unpack, 
  setmetatable = setmetatable,
  getmetatable = getmetatable,

  -- modified global functions:
  print = myprint,
  error = myerror

  -- my own api:
  -- move = move
  -- kill = kill
}

--local mt = {__index=cage} 

function scriptloader (scriptname)
  -- print( "scriptloader loading " .. scriptname )
  local scriptenv = {}
  setmetatable (scriptenv, {__index=_G})
  
  chunk, error = loadfile (scriptname, "bt", scriptenv)
  if not chunk then
    print(error)
  else
    chunk()
    return scriptenv
  end
end

-- a = scriptloader("scripts/rtc.lua")
-- b = scriptloader("scripts/dht22.lua")
-- print( env.receiveSPI(0) )

-- a.setCS(true)
-- b.setCS(true)