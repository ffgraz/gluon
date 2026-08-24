-- olsrd is enabled per address family by prefix4 / prefix6 being set,
-- the site configuration only adds to the generated olsrd configuration
need_table({'mesh', 'olsrd', 'v4', 'config'}, nil, false)
need_table({'mesh', 'olsrd', 'v6', 'config'}, nil, false)
