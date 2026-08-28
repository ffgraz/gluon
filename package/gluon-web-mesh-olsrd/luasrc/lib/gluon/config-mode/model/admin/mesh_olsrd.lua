local uci = require('simple-uci').cursor()

local f = Form(translate("OLSR"))

local s = f:section(Section, nil, translate(
	"Enable the OLSR (old) mesh. This allows for compatibility with "
	.. "existing Funkfeuer Graz non-gluon nodes."))

local enabled = s:option(Flag, "enabled", translate("Enable OLSR (IPv4)"))
enabled.default = uci:get_bool('gluon', 'mesh_olsrd', 'enabled')

function enabled:write(value)
	uci:set('gluon', 'mesh_olsrd', 'enabled', value)
	uci:commit('gluon')

	-- Which daemons run and what babel redistributes are both decided
	-- when the configuration is generated, so it has to be generated
	-- again for the switch to mean anything.
	os.execute('exec gluon-reconfigure')
end

return f
