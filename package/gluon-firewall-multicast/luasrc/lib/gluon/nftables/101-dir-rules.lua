-- Packets not on br-client fall through and return unfiltered

bridge_rule('IN_ONLY', 'ibrname "br-client" iifname { "bat0", "local-port" } return')
bridge_rule('IN_ONLY', 'ibrname "br-client" drop')

bridge_rule('OUT_ONLY', 'obrname "br-client" oifname { "bat0", "local-port" } return')
bridge_rule('OUT_ONLY', 'obrname "br-client" drop')
