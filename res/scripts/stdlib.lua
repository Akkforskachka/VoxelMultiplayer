-- kit of standard functions

-- Check if given table is an array
function is_array(x)
    if #t > 0 then
        return true
    end
    for k, v in pairs(x) do
        return false
    end
    return true
end

local __cached_scripts = {}
local __cached_results = {}

-- Get entry-point and filename from `entry-point:filename` path 
function parse_path(path)
    local index = string.find(path, ':')
    if index == nil then
        error("invalid path syntax (':' missing)")
    end
    return string.sub(path, 1, index-1), string.sub(path, index+1, -1)
end

-- Load script with caching
--
-- path - script path `contentpack:filename`. 
--     Example `base:scripts/tests.lua`
--
-- nocache - ignore cached script, load anyway
function load_script(path, nocache)
    local packname, filename = parse_path(path)
    local fullpath = file.resolve(path);

    -- __cached_scripts used in condition because cached result may be nil
    if not nocache and __cached_scripts[fullpath] ~= nil then
        return __cached_results[fullpath]
    end
    local script = loadfile(fullpath)
    if script == nil then
        error("script '"..filename.."' not found in '"..packname.."'")
    end
    local result = script()
    if not nocache then
        __cached_scripts[fullpath] = script
        __cached_results[fullpath] = result
    end
    return result
end

function sleep(timesec)
    local start = time.uptime()
    while time.uptime() - start < timesec do
        coroutine.yield()
    end
end

