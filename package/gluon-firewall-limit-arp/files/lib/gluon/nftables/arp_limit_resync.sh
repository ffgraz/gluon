#!/bin/sh

# Sourced by fw4 after every reload, once the new ruleset is in place.
#
# Reloading the firewall recreates 'table bridge gluon', which empties
# the limitmac and datips sets. gluon-arp-limiter fills those from
# batman-adv's translation table and DAT cache, and only calls nft when
# its own view of them changes - so it never refills a set that was
# emptied behind its back. Per-client ARP limiting then silently stops
# working, because 'arp saddr ether @limitmac' can no longer match, and
# expiring entries log a failed delete for an element that is long gone.
#
# Restarting the daemon makes it flush and repopulate from scratch.
#
# UCI is unavailable in fw4 includes, so this only uses the init script.

if /etc/init.d/gluon-arp-limiter enabled 2>/dev/null && \
	/etc/init.d/gluon-arp-limiter running 2>/dev/null; then
	/etc/init.d/gluon-arp-limiter restart >/dev/null 2>&1
fi
