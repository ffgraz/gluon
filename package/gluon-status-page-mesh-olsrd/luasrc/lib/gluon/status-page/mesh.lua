return {
	provider = '/cgi-bin/dyn/neighbours-olsrd',
	-- List of mesh-specific attributes, each a tuple of
	-- 1) the internal identifier (JSON key)
	-- 2) human-readable key (not translatable yet)
	-- 3) value suffix (optional)
	attrs = {
		{'etx', 'Quality (ETX)', ' '},
		{'olsr1_ip', 'OLSRv1 IP', ' '},
		{'olsr2_ip', 'OLSRv2 IP', ' '},
	},
}
