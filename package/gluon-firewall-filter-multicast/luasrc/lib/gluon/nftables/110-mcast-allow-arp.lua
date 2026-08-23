-- B.A.T.M.A.N. bridge loop avoidance
-- nftables cannot compare arp saddr ip against arp daddr ip, so the
-- --arp-gratuitous check of the previous ebtables rules is left out;
-- the BLA group MAC destination is specific enough by itself.
-- (ff:43:04:00:00:00 is ff:43:05:00:00:00 pre-masked with the
-- ff:ff:ff:fc mask, as ebtables masked both sides of the comparison)
bridge_rule('MULTICAST_OUT', 'arp operation reply arp daddr ether & ff:ff:ff:fc:00:00 == ff:43:04:00:00:00 return')
bridge_rule('MULTICAST_OUT', 'arp operation reply arp daddr ether & ff:ff:ff:ff:00:00 == ff:43:05:05:00:00 return')

bridge_rule('MULTICAST_OUT', 'arp operation reply arp saddr ip 0.0.0.0 drop')
bridge_rule('MULTICAST_OUT', 'arp operation request arp daddr ip 0.0.0.0 drop')
bridge_rule('MULTICAST_OUT', 'ether type arp return')
