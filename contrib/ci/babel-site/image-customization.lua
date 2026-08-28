features {
	'autoupdater',
	'mesh-babel',
	'mesh-olsrd',
	'mesh-vpn-fastd',
	'respondd',
	'status-page',
	'web-advanced',
	'web-wizard',
}

packages {
	'iwinfo',
}

if not device_class('tiny') then
	features {'wireless-encryption-wpa3'}
end
