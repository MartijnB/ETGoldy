-----------------------------------------------------------------------------------------------------------------------
-- inspect.lua - v1.2.1 (2013-01)
-- Enrique García Cota - enrique.garcia.cota [AT] gmail [DOT] com
-- human-readable representations of tables.
-- inspired by http://lua-users.org/wiki/TableSerialization
-----------------------------------------------------------------------------------------------------------------------

inspect ={}
inspect.__VERSION = '1.2.0'

-- Apostrophizes the string if it has quotes, but not aphostrophes
-- Otherwise, it returns a regular quoted string
local function smartQuote(str)
  if string.match( string.gsub(str,"[^'\"]",""), '^"+$' ) then
    return "'" .. str .. "'"
  end
  return string.format("%q", str )
end

local controlCharsTranslation = {
  ["\a"] = "\\a",  ["\b"] = "\\b", ["\f"] = "\\f",  ["\n"] = "\\n",
  ["\r"] = "\\r",  ["\t"] = "\\t", ["\v"] = "\\v",  ["\\"] = "\\\\"
}

local function unescapeChar(c) return controlCharsTranslation[c] end

local function unescape(str)
  local result, _ = string.gsub( str, "(%c)", unescapeChar )
  return result
end

local function isIdentifier(str)
  return type(str) == 'string' and str:match( "^[_%a][_%a%d]*$" )
end

local function isArrayKey(k, length)
  return type(k)=='number' and 1 <= k and k <= length
end

local function isDictionaryKey(k, length)
  return not isArrayKey(k, length)
end

local sortOrdersByType = {
  ['number']   = 1, ['boolean']  = 2, ['string'] = 3, ['table'] = 4,
  ['function'] = 5, ['userdata'] = 6, ['thread'] = 7
}

local function sortKeys(a,b)
  local ta, tb = type(a), type(b)
  if ta ~= tb then return sortOrdersByType[ta] < sortOrdersByType[tb] end
  if ta == 'string' or ta == 'number' then return a < b end
  return false
end

local function getDictionaryKeys(t)
  local length = #t
  local keys = {}
  for k,_ in pairs(t) do
    if isDictionaryKey(k, length) then table.insert(keys,k) end
  end
  table.sort(keys, sortKeys)
  return keys
end

local function getToStringResultSafely(t, mt)
  local __tostring = type(mt) == 'table' and rawget(mt, '__tostring')
  local string, status
  if type(__tostring) == 'function' then
    status, string = pcall(__tostring, t)
    string = status and string or 'error: ' .. tostring(string)
  end
  return string
end

local Inspector = {}

function Inspector:new(t, depth)
  local inspector = {
    buffer = {},
    depth = depth,
    level = 0,
    maxIds = {
      ['function'] = 0,
      ['userdata'] = 0,
      ['thread']   = 0,
      ['table']    = 0
    },
    ids = {
      ['function'] = setmetatable({}, {__mode = "kv"}),
      ['userdata'] = setmetatable({}, {__mode = "kv"}),
      ['thread']   = setmetatable({}, {__mode = "kv"}),
      ['table']    = setmetatable({}, {__mode = "kv"})
    },
    tableAppearances = setmetatable({}, {__mode = "k"})
  }

  setmetatable(inspector, {__index = Inspector})

  inspector:countTableAppearances(t)

  return inspector:putValue(t)
end

function Inspector:countTableAppearances(t)
  if type(t) == 'table' then
    if not self.tableAppearances[t] then
      self.tableAppearances[t] = 1
      for k,v in pairs(t) do
        self:countTableAppearances(k)
        self:countTableAppearances(v)
      end
      self:countTableAppearances(getmetatable(t))
    else
      self.tableAppearances[t] = self.tableAppearances[t] + 1
    end
  end
end

function Inspector:tabify()
  self:puts("\n", string.rep("  ", self.level))
  return self
end

function Inspector:up()
  self.level = self.level - 1
end

function Inspector:down()
  self.level = self.level + 1
end

function Inspector:puts(...)
  local args = {...}
  local len = #self.buffer
  for i=1, #args do
    len = len + 1
    self.buffer[len] = tostring(args[i])
  end
  return self
end

function Inspector:commaControl(comma)
  if comma then self:puts(',') end
  return true
end

function Inspector:putTable(t)
  if self:alreadyVisited(t) then
    self:puts('<table ', self:getId(t), '>')
  elseif self.depth and self.level >= self.depth then
    self:puts('{...}')
  else
    if self.tableAppearances[t] > 1 then
      self:puts('<',self:getId(t),'>')
    end
    self:puts('{')
    self:down()

      local length = #t
      local mt = getmetatable(t)

      local string = getToStringResultSafely(t, mt)
      if type(string) == 'string' and #string > 0 then
        self:puts(' -- ', unescape(string))
        if length >= 1 then self:tabify() end -- tabify the array values
      end

      local comma = false
      for i=1, length do
        comma = self:commaControl(comma)
        self:puts(' '):putValue(t[i])
      end

      local dictKeys = getDictionaryKeys(t)

      for _,k in ipairs(dictKeys) do
        comma = self:commaControl(comma)
        self:tabify():putKey(k):puts(' = '):putValue(t[k])
      end

      if mt then
        comma = self:commaControl(comma)
        self:tabify():puts('<metatable> = '):putValue(mt)
      end

    self:up()

    if #dictKeys > 0 or mt then -- dictionary table. Justify closing }
      self:tabify()
    elseif length > 0 then -- array tables have one extra space before closing }
      self:puts(' ')
    end
    self:puts('}')
  end
  return self
end

function Inspector:alreadyVisited(v)
  return self.ids[type(v)][v] ~= nil
end

function Inspector:getId(v)
  local tv = type(v)
  local id = self.ids[tv][v]
  if not id then
    id              = self.maxIds[tv] + 1
    self.maxIds[tv] = id
    self.ids[tv][v] = id
  end
  return id
end

function Inspector:putValue(v)
  local tv = type(v)

  if tv == 'string' then
    self:puts(smartQuote(unescape(v)))
  elseif tv == 'number' or tv == 'boolean' or tv == 'nil' then
    self:puts(tostring(v))
  elseif tv == 'table' then
    self:putTable(v)
  else
    self:puts('<',tv,' ',self:getId(v),'>')
  end
  return self
end

function Inspector:putKey(k)
  if isIdentifier(k) then return self:puts(k) end
  return self:puts( "[" ):putValue(k):puts("]")
end

function Inspector:tostring()
  return table.concat(self.buffer)
end

setmetatable(inspect, { __call = function(_,t,depth)
  return Inspector:new(t, depth):tostring()
end })

-------------------------------------------------------------------------------------------

function table.compare( tbl1, tbl2 )
	for k, v in pairs( tbl1 ) do
		if ( type(v) == "table" and type(tbl2[k]) == "table" ) then
			if ( table.compare( v, tbl2[k] ) ~= true ) then return false end
		else
			if ( v ~= tbl2[k] ) then return false end
		end
	end
	for k, v in pairs( tbl2 ) do
		if ( type(v) == "table" and type(tbl1[k]) == "table" ) then
			if ( table.compare( v, tbl1[k] ) ~= true ) then return false end
		else
			if ( v ~= tbl1[k] ) then return false end
		end
	end
	return true
end

function table.val_to_str ( v )
  if "string" == type( v ) then
    v = string.gsub( v, "\n", "\\n" )
    if string.match( string.gsub(v,"[^'\"]",""), '^"+$' ) then
      return "'" .. v .. "'"
    end
    return '"' .. string.gsub(v,'"', '\\"' ) .. '"'
  else
    return "table" == type( v ) and table.tostring( v ) or
      tostring( v )
  end
end

function table.key_to_str ( k )
  if "string" == type( k ) and string.match( k, "^[_%a][_%a%d]*$" ) then
    return k
  else
    return "[" .. table.val_to_str( k ) .. "]"
  end
end

function table.tostring( tbl )
  local result, done = {}, {}
  for k, v in ipairs( tbl ) do
    table.insert( result, table.val_to_str( v ) )
    done[ k ] = true
  end
  for k, v in pairs( tbl ) do
    if not done[ k ] then
      table.insert( result,
        table.key_to_str( k ) .. "=" .. table.val_to_str( v ) )
    end
  end
  return "{" .. table.concat( result, "," ) .. "}"
end

-------------------------------------------------------------------------------------------

function et.G_Printf(...)
	et.G_Print(string.format(unpack(arg)))
end

function et_InitGame(levelTime, randomSeed, restart)
	et.G_Printf("ETGoldy LUA Init [%d] [%d] [%d]\n", levelTime, randomSeed, restart)
	et.RegisterModname("ETGoldy")

	local init_time_start = os.clock()

	--et.G_Printf(inspect(et.G_SearchEntities({})))
	--et.G_Printf("\n")

	goldy_initScripts()
	goldy_initMap()

	et.G_Printf("ETGoldy LUA Init completed in %.2f ms\n", (os.clock() - init_time_start) * 1000)
end

function _exec_subscope(luaCode) 
	loadstring(luaCode)()
end

function goldy_initScripts()
	local files = et.trap_FS_GetFileList("scripts", "lua")

	et.G_Printf("Found LUA Scripts: %d\n", #files)

	for k,filename in ipairs( files ) do
		et.G_Printf("Loading " .. filename .. "... \n")

		local fd, len = et.trap_FS_FOpenFile("scripts\\" .. filename, et.FS_READ)
		if len ~= -1 then
			_exec_subscope(et.trap_FS_Read(fd, len))

			et.trap_FS_FCloseFile(fd)
		else
			et.G_Printf("Failed!\n")
		end
	end
end

function goldy_initMap()
	et.G_Printf("Loading LUA mapscript (".. et.trap_Cvar_Get("mapname") .. "_goldy.lua" ..")...\n")

	local fd, len = et.trap_FS_FOpenFile("mapscripts\\" .. et.trap_Cvar_Get("mapname") .. "_goldy.lua", et.FS_READ)
	if len ~= -1 then
		_exec_subscope(et.trap_FS_Read(fd, len))

		et.trap_FS_FCloseFile(fd)
	else
		et.G_Printf("LUA mapscript missing! \n")
	end
end

 _et_trap_FS_Write = et.trap_FS_Write

 function et.trap_FS_Write(filedata, fd)
	return  _et_trap_FS_Write(filedata, string.len(filedata), fd)
 end

function et.G_DumpEntities()
	-- get default entity info
	local entnum = et.G_Spawn() 
	local defaultFields = et.G_Entity(entnum)
	et.G_FreeEntity(entnum)

	local fd, len = et.trap_FS_FOpenFile("mapscripts\\" .. et.trap_Cvar_Get("mapname") .. "_entities.txt", et.FS_WRITE)
	if len == -1 then
		et.G_Printf("Can't open dumpfile!\n")
	end

	for entnum,fields in pairs(et.G_SearchEntities({})) do
		et.trap_FS_Write("[".. fields["classname"] .."]\n", fd)

		for key,value in pairs(fields) do
			local tv = type(value)

			if tv == 'string' then
				if defaultFields[key] ~= value then
					et.trap_FS_Write(key .. " = ".. value .. "\n", fd)
				end
			elseif tv == 'number' or tv == 'boolean' or tv == 'nil' then
				if defaultFields[key] ~= value then
					et.trap_FS_Write(key .. " = ".. value .. "\n", fd)
				end
			elseif tv == 'table' then
				if table.compare(defaultFields[key], value) ~= true  then
					et.trap_FS_Write(key .. " = ".. table.tostring(value) .. "\n", fd)
				end
			else
				et.trap_FS_Write(key .. " = ".. '<',tv,' ',self:getId(value),'>' .. "\n", fd)
			end
		end

		et.trap_FS_Write("\n", fd)
	end

	et.trap_FS_FCloseFile(fd)
end