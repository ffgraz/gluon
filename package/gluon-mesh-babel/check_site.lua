need_string_match(in_domain({'node_prefix6'}), '^[%x:]+/64$')

need_string_match(in_domain({'next_node', 'ip6'}), '^[%x:]+$', false)
need_string_match(in_domain({'next_node', 'ip4'}), '^%d+.%d+.%d+.%d+$', false)

need_string_match(in_domain({'next_node', 'mac'}), '^%x[02468aAcCeE]:%x%x:%x%x:%x%x:%x%x:%x%x$', false)

-- prefixes whose kernel routes babel imports, e.g. those a parallel
-- olsrd installs
need_array(in_domain({'mesh', 'babel', 'import'}), function(prefix)
	need_string_match(prefix, '^[%x:%.]+/%d+$')
end, false)
