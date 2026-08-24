return function(form, uci)
	local pkg_i18n = i18n 'gluon-config-mode-manman-sync'

	local msg = pkg_i18n.translate(
		'Sync configuration from ManMan ' ..
			'by entering ManMan location and node name here.<br>' ..
		'This will automatically keep name, location and ips ' ..
			'in sync with the values specified in ManMan.'
	)

	local s = form:section(Section, nil, msg)

	local manman = s:option(Flag, 'manman_sync', pkg_i18n.translate('Enable ManMan sync'))
	manman.default = uci:get_bool('gluon-manman-sync', 'sync', 'enabled')
	function manman:write(data)
		uci:set('gluon-manman-sync', 'sync', 'enabled', data)
	end

	local lid = s:option(Value, 'manman_location', pkg_i18n.translate('ManMan location'))
	lid:depends(manman, true)
	lid.default = uci:get('gluon-manman-sync', 'sync', 'location')
	lid.datatype = 'maxlength(16)'
	function lid:write(data)
		-- clear ID, gets fetched from manman-sync
		uci:set('gluon-manman-sync', 'sync', 'location_id', nil)

		uci:set('gluon-manman-sync', 'sync', 'location', data)
	end

	local nid = s:option(Value, 'manman_node', pkg_i18n.translate('ManMan node (optional)'),
		pkg_i18n.translate('Required if multiple nodes in location'))
	nid:depends(manman, true)
	nid.default = uci:get('gluon-manman-sync', 'sync', 'node')
	nid.datatype = 'maxlength(16)'
	function nid:write(data)
		-- clear ID, gets fetched from manman-sync
		uci:set('gluon-manman-sync', 'sync', 'node_id', nil)

		uci:set('gluon-manman-sync', 'sync', 'node', data)
	end

	function s:write()
		uci:save('gluon-manman-sync')
	end
end
