io.stderr:write("****+++++++++++++++++++ DLZ_LUA\n")

--[[
  	lookup is called when BIND needs to find an answer. (duh.)
	1. `name' is the domain name being queried (e.g. "www"). The zone
	    apex is "@" (and DLZ queries that very frequently).
	2. `zone' is the zone name you configure in the dlopen() driver
	    (e.g. "example.nil")
	3. `client' is the client's IP address or "unknown"
--]]

function lookup(name, zone, client)
	ret = {}

	-- bindlog() is a C function defined in dlz_lua.c
	bindlog("Lua lookup(" .. name .. ", " .. zone .. ", " .. client .. ")")

	if name == '@' then
		ret[1] = { type = 'SOA', ttl = 86400, 
			   rdata = 'example.nil. hostmaster.example.nil. 123 900 600 86400 3600' }
		ret[2] = { type = 'NS', ttl = 86400, rdata = 'jmbp.ww.mens.de' }
		ret[3] = { type = 'NS', ttl = 86400, rdata = 'example.nil' }
		ret[4] = { type = 'A', ttl = 86400, rdata = '127.0.0.2' }
		ret[4] = { type = 'A', ttl = 86400, rdata = '192.168.1.145' }
	end

	if name == 'www' then
		ret = www()
	end

	if name == 'jp' then
		ret[1] = { type = "A", ttl = 10, rdata = "192.168.69.17" }
	end

	if name == 'time' then
		iso8601 = os.date("%Y-%m-%dT%H:%M:%S") .. tz()
		ret[1] = { type = "TXT", ttl = 0, rdata = iso8601 }
	end

	if name == 'password' then
		ret[1] = { type = "TXT", ttl = 0, rdata = '"' .. generate(15, 15) .. '"' }
	end

	-- Allow query for "N.password" for generating passwords of length N
	if string.find(name, '.password') then
		pwlen = explode('.', name)[1]
		if tonumber(pwlen) == nil then
			pwlen = 8
		end
		if tonumber(pwlen) < 1 or tonumber(pwlen) > 40 then
			pwlen = 10
		end
		ret[1] = { type = "TXT", ttl = 0, rdata = '"' .. generate(pwlen, pwlen) .. '"' }
	end

	return 0, ret
end

function www()
        rrset = {}

        rrset[1] = { type = "txt", ttl = 3600, rdata = '"Hello JP!"' }
        rrset[2] = { type = "MX", ttl = 23, rdata = "10 mail.gmail.com." }
        rrset[3] = { type = "txt", ttl = 14, rdata = "bye to you" }
        rrset[4] = { type = "AAAA", ttl = 84, rdata = "fe80::223:32ff:fed5:3f" }

        return rrset
end

--[[
        Taken from
          http://lua-users.org/wiki/TimeZone
          http://lua-users.org/lists/lua-l/2008-03/msg00086.html
--]]

function tz()
        -- Return a timezone string in ISO 8601:2000 
        -- standard form (+hhmm or -hhmm)

        local function get_tzoffset(timezone)
          local h, m = math.modf(timezone / 3600)
          return string.format("%+.4d", 100 * h + 60 * m)
        end

        now = os.time()
        UTC_offset_seconds = os.difftime(os.time(os.date("*t", now)),
                        os.time(os.date("!*t", now)))

        tzoffset = get_tzoffset(UTC_offset_seconds)
        return tzoffset
end

--[[
	Reformatted from http://pastebin.com/DruT1MC6
--]]

-- Characters that will be used in the generator

char = {"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k",
        "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v",
	"w", "x", "y", "z", "0", "1", "2", "3", "4", "5", "6",
	"7", "8", "9", "@", "#", "$", "%", "&", "?" }

math.randomseed(os.time())

function generate(s, l) -- args: smallest and largest possible password lengths, inclusive
	size = math.random(s,l) -- random password length
	pass = {}

	for z = 1,size do

		case = math.random(1,2) -- randomly choose case (caps or lower)
		a = math.random(1,#char) -- randomly choose a character from the "char" array
		if case == 1 then
			x=string.upper(char[a]) -- uppercase if case = 1
		elseif case == 2 then
			x=string.lower(char[a]) -- lowercase if case = 2
		end
	table.insert(pass, x) -- add new index into array.
	end
	return(table.concat(pass)) -- concatenate all indicies of the "pass" array, then print out concatenation.
end

-- explode(separator, string)		-- http://lua-users.org/wiki/SplitJoin
function explode(d,p)
  local t, ll
  t={}
  ll=0
  if(#p == 1) then return {p} end
    while true do
      l=string.find(p,d,ll,true) -- find the next d in the string
      if l~=nil then -- if "not not" found then..
        table.insert(t, string.sub(p,ll,l-1)) -- Save it in our array.
        ll=l+1 -- save just after where we found it for searching next time.
      else
        table.insert(t, string.sub(p,ll)) -- Save what's left in our array.
        break -- Break at end, as it should be, according to the lua manual.
      end
    end
  return t
end

