local uci = require("simple-uci").cursor()
local wireless = require 'gluon.wireless'
local ip = require 'luci.ip'
local site = require 'gluon.site'

local f = Form(translate("Static IPs"))

local s4 = f:section(Section, nil, translate(
	'Configure the IPv4 addresses of your node.'
))

local s6 = site.tmpIp6() and f:section(Section, nil, translate(
	'Configure the IPv6 addresses of your node.'
))

local function translate_format(str, ...)
  return string.format(translate(str), ...)
end

local function intf_setting(intf, desc, enabled)
	local status = enabled and translate("enabled") or translate("disabled")

	if site.tmpIp4() then
		local v4addr = uci:get('gluon-static-ip', intf, 'ip4')
		local tmp = ip.new(site.tmpIp4(), site.tmpIp4Range())
		local isTmp = tmp:contains(ip.new(v4addr):host())

		if isTmp then
			local w = Warning()
			if enabled then
				w:setcontent(translate_format('The address %s for "%s" is an address in the temporary address range %s.<br />It should be replaced by a properly assigned address as soon as possible.',
					v4addr, desc, tmp:string()))
			else
				w:setcontent(translate_format('The address %s for "%s" is an address in the temporary address range %s.<br />If you are planning to use this interface, you will need to replace this address with a properly assigned one.',
					v4addr, desc, tmp:string()))
			end
			s4:append(w)
		end

		local v4 = s4:option(Value, intf .. '_ip4', translate_format("IPv4 for %s (%s)", desc, status), translate("IPv4 CIDR (e.g. 1.2.3.4/12)"))
		-- TODO: datatype = "ip4cidr"
		v4.datatype = "maxlength(32)"
		v4.default = v4addr

		function v4:write(data)
			-- TODO: validate via datatype
			if not ip.new(data) or not ip.new(data):is4() then
				error('Not a valid IPv4 for ' .. intf)
			end

			uci:set("gluon-static-ip", intf, "ip4", data)
		end
	end

	if site.tmpIp6() then
		local v6addr = uci:get('gluon-static-ip', intf, 'ip6')
		local tmp = ip.new(site.tmpIp6(), site.tmpIp6Range())
		local isTmp = tmp:contains(ip.new(v6addr):host())

		if isTmp then
			local w = Warning()
			if enabled then
				w:setcontent(translate_format('The address %s for "%s" is an address in the temporary address range %s.<br />It should be replaced by a properly assigned address as soon as possible.',
					v6addr, desc, tmp:string()))
			else
				w:setcontent(translate_format('The address %s for "%s" is an address in the temporary address range %s.<br />If you are planning to use this interface, you will need to replace this address with a properly assigned one.',
					v6addr, desc, tmp:string()))
			end
			s6:append(w)
		end

		local v6 = s6:option(Value, intf .. '_ip6', translate_format("IPv6 for %s (%s)", desc, status), translate("IPv6 CIDR (e.g. aa:bb:cc:dd:ee::ff/64)"))
		-- TODO: datatype = "ip6cidr"
		v6.datatype = "maxlength(132)"
		v6.default = uci:get('gluon-static-ip', intf, 'ip6')

		function v6:write(data)
			-- TODO: validate via datatype
			if not ip.new(data) or not ip.new(data):is6() then
				error('Not a valid IPv6 for ' .. intf)
			end

			uci:set("gluon-static-ip", intf, "ip6", data)
		end
	end
end

wireless.foreach_radio(uci, function(radio, index, config)
	local function do_conf(type, desc)
		local net = type .. radio['.name']
		intf_setting(net, desc, not uci:get_bool('wireless', net, 'disabled'))
	end

	if uci:get('network', 'ibss_' .. radio['.name'], 'proto') then
		do_conf('ibss_', translate_format('IBSS (legacy) Mesh on %s', radio['.name']))
	end
	do_conf('mesh_', translate_format('Mesh on %s', radio['.name']))
end)

if pcall(function() require 'gluon.mesh-vpn' end) then
	local vpn_core = require 'gluon.mesh-vpn'

  intf_setting('mesh_vpn', 'Mesh VPN', vpn_core.enabled())
end

local wan_mesh = not uci:get_bool('network', 'mesh_wan', 'disabled')
intf_setting('mesh_wan', 'Mesh on WAN', wan_mesh)

if uci:get('network', 'mesh_lan', 'proto') then
  local lan_mesh = not uci:get_bool('network', 'mesh_lan', 'disabled')
  intf_setting('mesh_lan', 'Mesh on LAN', lan_lan)
end

function f:write()
	uci:save("gluon-static-ip")
end

-- TODO: warning about 23 addrs


return f
