local iwinfo = require 'iwinfo'
local uci = require("simple-uci").cursor()
local site = require 'gluon.site'
local wireless = require 'gluon.wireless'
local util = require 'gluon.util'
local sysconfig = require 'gluon.sysconfig'

local function txpower_list(phy)
	local list = iwinfo.nl80211.txpwrlist(phy) or { }
	local off  = tonumber(iwinfo.nl80211.txpower_offset(phy)) or 0
	local new  = { }
	local prev = -1
	for _, val in ipairs(list) do
		local dbm = val.dbm + off
		local mw  = math.floor(10 ^ (dbm / 10))
		if mw ~= prev then
			prev = mw
			table.insert(new, {
				display_dbm = dbm,
				display_mw  = mw,
				driver_dbm  = val.dbm,
			})
		end
	end
	return new
end

local f = Form(translate("WLAN"))

f:section(Section, nil, translate(
	"You can enable or disable your node's client and mesh network "
	.. "SSIDs here. Please don't disable the mesh network without "
	.. "a good reason, so other nodes can mesh with yours.<br><br>"
	.. "It is also possible to configure the WLAN adapters transmission power "
	.. "here. Please note that the transmission power values include the antenna gain "
	.. "where available, but there are many devices for which the gain is unavailable or inaccurate."
))


local mesh_vifs_5ghz = {}

local function add_or_remove_role(roles, role, enabled)
	if enabled then
		util.add_to_set(roles, role)
	else
		util.remove_from_set(roles, role)
	end
end
local mesh_outdoor_dependent = {}

local function vif_option(section, role_name, band, band_config, msg)
	local o = section:option(Flag, band .. '_' .. role_name .. '_enabled', msg)
	o.default = util.contains(band_config.role or {}, role_name)

	function o:write(data)
		-- Without the additional read before write in o:write, this would race
		local roles = uci:get_list('gluon', band, 'role')
		add_or_remove_role(roles, role_name, data)
		uci:set_list('gluon', band, 'role', roles)
	end

	return o
end

uci:foreach('gluon', 'wireless_band', function(band_config)
	local band = band_config['.name']

	local is_5ghz = false
	local is_60ghz = false
	local title

	if band == 'band_2g' then
		title = translate("2.4GHz WLAN")
	elseif band == 'band_5g' then
		is_5ghz = true
		title = translate("5GHz WLAN")
	elseif band == 'band_6g' then
		title = translate("6GHz WLAN")
	elseif band == 'band_60g' then
		is_60ghz = true
		title = translate("60GHz WLAN")
	else
		return
	end

	local p = f:section(Section, title)

	-- 60 GHz radios carry point-to-point links only; they have neither a
	-- client network nor an 802.11s mesh
	if not is_60ghz then
		vif_option(p, 'client', band, band_config,
			translate('Enable client network (access point)'))

		local mesh_vif = vif_option(p, 'mesh', band, band_config,
			translate("Enable mesh network (802.11s)"))
		if is_5ghz then
			table.insert(mesh_vifs_5ghz, mesh_vif)
		end
	end

	-- a point-to-point link can be set up on any band; the SSID and mode
	-- of the link are per radio and live in the per-radio section below
	vif_option(p, 'p2p', band, band_config,
		translate('Enable point-to-point (AP/STA) mesh'))
end)

local radio_sec = f:section(Section, translate("Per radio settings"))
wireless.foreach_radio(uci, function(radio, _, band_config)
	local radio_name = radio['.name']
	local wradio_name = wireless.get_wradio(uci, radio)
	local phy = wireless.find_phy(radio)
	if not phy or not wradio_name then
		return
	end

	if util.contains(wireless.radio_roles(uci, radio) or {}, 'p2p') then
		-- SSID and mode of the point-to-point link are per radio, and
		-- live in the radio's gluon section so that they survive the
		-- wireless config being regenerated on upgrade
		local id = radio_sec:option(Value, 'p2pid_' .. radio_name,
			translate('P2P SSID') .. ' (' .. radio_name .. ')')
		id.datatype = 'maxlength(32)'
		id.default = uci:get('gluon', wradio_name, 'p2p_ssid')
			or 'g-' .. string.sub(string.gsub(sysconfig.primary_mac, ':', ''), 8)
		function id:write(data)
			uci:set('gluon', wradio_name, 'p2p_ssid', data)
		end

		local mode = radio_sec:option(ListValue, 'p2pmode_' .. radio_name,
			translate('P2P Mode') .. ' (' .. radio_name .. ')',
			translate('Master=AP Slave=Station'))
		mode.default = uci:get('gluon', wradio_name, 'p2p_mode') or 'ap'
		mode:value('ap', translate('Master'))
		mode:value('sta', translate('Slave'))
		function mode:write(data)
			uci:set('gluon', wradio_name, 'p2p_mode', data)
		end
	end

	local txpowers = txpower_list(phy)
	if #txpowers > 1 then
		local str_txpower = translate("Transmission power") .. ' (' .. radio_name .. ')'
		local tp = radio_sec:option(ListValue, 'txpower_' .. radio_name, str_txpower)
		tp.default = uci:get('gluon', wradio_name, 'txpower') or 'default'

		tp:value('default', translate("(default)"))

		table.sort(txpowers, function(a, b) return a.driver_dbm > b.driver_dbm end)

		for _, entry in ipairs(txpowers) do
			tp:value(entry.driver_dbm, string.format("%i dBm (%i mW)", entry.display_dbm, entry.display_mw))
		end

		function tp:write(data)
			if data == 'default' then
				data = nil
			end
			uci:set('gluon', wradio_name, 'txpower', data)
		end

		-- outdoor 5 GHz radios get their channel and power from the
		-- outdoor chanlist, so those fields are hidden in outdoor mode
		if radio.band == '5g' then
			table.insert(mesh_outdoor_dependent, tp)
		end
	end

	local str_htmode = translate('HT Mode') .. ' (' .. radio_name .. ')'
	local ht = radio_sec:option(ListValue, 'htmode_' .. radio_name, str_htmode)
	ht.default = uci:get('gluon', wradio_name, 'htmode') or 'default'
	ht:value('default', translate("(default)"))

	for mode, available in pairs(iwinfo.nl80211.htmodelist(phy)) do
		if available then
			ht:value(mode, mode)
		end
	end

	function ht:write(data)
		if data == 'default' then
			data = nil
		end
		uci:set('gluon', wradio_name, 'htmode', data)
	end

	if band_config.channel_adjustable(false) then
		local ch = radio_sec:option(ListValue, 'channel_' .. radio_name,
			translate("Channel") .. ' (' .. radio_name .. ')')
		local default_channel = band_config.channel()
		local channels = iwinfo.nl80211.freqlist(phy)
		local has_default = false

		for _, entry in ipairs(channels) do
			if entry.channel == default_channel then
				has_default = true
			end
		end

		-- a radio that cannot do the site's channel probes one at
		-- runtime, so there is no entry to mark as the default one
		if not has_default then
			ch:value('default', translate("(default)"))
		end

		for _, entry in ipairs(channels) do
			ch:value(entry.channel, string.format(
				entry.channel == default_channel
					and "%i " .. translate("(default)") or "%i",
				entry.channel))
		end

		ch.default = uci:get('gluon', wradio_name, 'channel')
			or (has_default and tostring(default_channel) or 'default')

		function ch:write(data)
			-- storing nothing keeps the radio on whatever the site picks
			if data == 'default' or data == tostring(default_channel) then
				data = nil
			end
			uci:set('gluon', wradio_name, 'channel', data)
		end

		if radio.band == '5g' then
			table.insert(mesh_outdoor_dependent, ch)
		end
	end
end)


if wireless.device_uses_band(uci, '5g') and not wireless.preserve_channels(uci) then
	local r = f:section(Section, translate("Outdoor Installation"), translate(
		"Configuring the node for outdoor use tunes the 5 GHz radio to a frequency "
		.. "and transmission power that conforms with the local regulatory requirements. "
		.. "It also enables dynamic frequency selection (DFS; radar detection). At the "
		.. "same time, mesh functionality is disabled as it requires neighbouring nodes "
		.. "to stay on the same channel permanently."
	))

	local outdoor = r:option(Flag, 'outdoor', translate("Node will be installed outdoors"))
	outdoor.default = wireless.is_outdoor(uci)

	for _, mesh_vif in ipairs(mesh_vifs_5ghz) do
		mesh_vif:depends(outdoor, false)
		if outdoor.default then
			mesh_vif.default = not site.wifi5.mesh.disabled(false)
		end
	end

	for _, field in ipairs(mesh_outdoor_dependent) do
		field:depends(outdoor, false)
	end

	function outdoor:write(data)
		uci:set('gluon', 'wireless', 'outdoor', data)
	end
end


function f:write()
	uci:commit('gluon')
	os.execute('exec gluon-reconfigure >/dev/null')
end

return f
