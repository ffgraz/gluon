-- One descriptor for the whole layer-3 mesh, whichever daemons are
-- running: the status page only ever reads a single mesh.lua, so a node
-- meshing over babel next to olsrd could not have one per daemon.
return {
	provider = '/cgi-bin/dyn/neighbours-layer3',
	-- List of mesh-specific attributes, each a tuple of
	-- 1) the internal identifier (JSON key)
	-- 2) human-readable key (not translatable yet)
	-- 3) value suffix (optional)
	attrs = {
		{'tq', 'TQ', ' %'},
		{'etx', 'Quality (ETX)', ' '},
		{'olsr4_ip', 'OLSR IPv4 IP', ' '},
		-- olsrd6 does not run here, babel carries IPv6
		{'babel_ip', 'Babel IPv6 IP', ' '},
	},
}
