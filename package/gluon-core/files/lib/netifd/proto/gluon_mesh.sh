#!/bin/sh

# shellcheck disable=SC1091

. /lib/functions.sh
. ../netifd-proto.sh
init_proto "$@"

proto_gluon_mesh_init_config() {
	proto_config_add_boolean fixed_mtu
	proto_config_add_int hop_penalty
	proto_config_add_boolean transitive
	proto_config_add_string ipaddr
	proto_config_add_string ip6addr
}

proto_gluon_mesh_setup() {
	export CONFIG="$1"
	export IFNAME="$2"

	local fixed_mtu hop_penalty transitive ipaddr ip6addr
	json_get_vars fixed_mtu hop_penalty transitive ipaddr ip6addr

	export FIXED_MTU="${fixed_mtu:-0}"
	export TRANSITIVE="${transitive:-0}"

	for script in /lib/gluon/core/mesh/setup.d/*; do
		[ ! -x "$script" ] || "$script"
	done

	proto_init_update "$IFNAME" 1

	# Let netifd place the addresses. gluon_wired builds this interface
	# on an alias of the one carrying the ports, and an alias leaves
	# $IFNAME empty here, so adding them by hand ended in
	# "Cannot find device" and took the interface down with it.
	if [ -n "$ipaddr" ]; then
		proto_add_ipv4_address "${ipaddr%%/*}" "${ipaddr##*/}"
	fi
	if [ -n "$ip6addr" ]; then
		proto_add_ipv6_address "${ip6addr%%/*}" "${ip6addr##*/}"
	fi

	proto_add_data
	json_add_boolean fixed_mtu "$FIXED_MTU"
	[ -n "${hop_penalty}" ] && json_add_int hop_penalty "${hop_penalty}"
	json_add_boolean transitive "$TRANSITIVE"
	if [ ! -z "$ipaddr" ]; then
		json_add_string ipaddr "$ipaddr"
	fi
	if [ ! -z "$ip6addr" ]; then
		json_add_string ip6addr "$ip6addr"
	fi
	[ "$IFNAME" != 'br-wan' ] && json_add_string zone 'mesh'
	proto_close_data
	proto_send_update "$CONFIG"

	# A hook is something to notify, not something that decides whether
	# the interface came up: netifd takes a non-zero exit here as the
	# setup having failed and tears the interface down again.
	for script in /lib/gluon/core/mesh/post-setup.d/*; do
		[ ! -x "$script" ] || "$script" || true
	done

	return 0
}

proto_gluon_mesh_teardown() {
	export CONFIG="$1"
	export IFNAME="$2"

	for script in /lib/gluon/core/mesh/teardown.d/*; do
		[ ! -x "$script" ] || "$script"
	done
}

add_protocol gluon_mesh
