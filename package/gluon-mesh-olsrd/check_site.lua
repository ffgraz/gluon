-- olsrd is enabled per address family by prefix4 / prefix6 being set,
-- the site configuration only adds to the generated olsrd configuration
need_table({'mesh', 'olsrd', 'v4', 'config'}, nil, false)
need_table({'mesh', 'olsrd', 'v6', 'config'}, nil, false)

-- the node takes an IPv4 address of its own out of node_prefix4 where the
-- site has one, and out of prefix4 otherwise
if need_string_match({'node_prefix4'}, '^%d+.%d+.%d+.%d+/%d+$', false) then
	need_number({'node_prefix4_range'}, true)
end
