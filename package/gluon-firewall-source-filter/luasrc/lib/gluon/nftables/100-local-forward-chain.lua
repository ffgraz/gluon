-- Packets not matched by any of the allow rules are dropped by the
-- final rule added in 300-local-forward-rules.lua (the equivalent of
-- the DROP policy of the previous ebtables chain)
bridge_chain('LOCAL_FORWARD')
