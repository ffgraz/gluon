/* SPDX-FileCopyrightText: 2021-2023 Maciej Krüger <maciej@xeredo.it> */
/* SPDX-FileCopyrightText: 2016-2019, Matthias Schiffer <mschiffer@universe-factory.net> */
/* SPDX-License-Identifier: BSD-2-Clause */

#include "respondd-common.h"

#include <libgluonutil.h>

#include <json-c/json.h>

#include <libolsrdhelper.h>

#include <glob.h>
#include <stdio.h>
#include <string.h>

static json_object * olsr1_get_plugins(void) {
	json_object *resp;

	if (olsr1_get_nodeinfo("plugins", &resp))
		return NULL;

	J_OUT(json_object_object_get(resp, "plugins"));
}

static json_object * olsr1_get_version (void) {
	json_object *resp;

	if (olsr1_get_nodeinfo("version", &resp))
		return NULL;

	J_OUT(json_object_object_get(json_object_object_get(resp, "version"), "version"));
}

static json_object * olsr2_get_version (void) {
	json_object *resp;

	if (olsr2_get_nodeinfo("systeminfo jsonraw version", &resp))
		return NULL;

	J_OUT(json_object_object_get(json_object_array_get_idx(json_object_object_get(resp, "version"), 0), "version_text"));
}

static json_object * olsr1_get_addresses (void) {
	json_object *resp;

	if (olsr1_get_nodeinfo("interfaces", &resp))
		return NULL;

	/*

	interfaces []
		name	"m_uplink"
		configured	true
		hostEmulation	false
		hostEmulationAddress	"0.0.0.0"
		olsrInterface
			-- might be false (and then ipAddress key is missing)
			up	true
			ipv4Address	"10.12.23.234"
			ipv4Netmask	"255.255.0.0"
			ipv4Broadcast	"10.12.255.255"
			mode	"mesh"
			ipv6Address	"::"
			ipv6Multicast	"::"
			-- we need this
			ipAddress	"10.12.23.234"
			..
		InterfaceConfiguration	{…}
		InterfaceConfigurationDefaults	{…}
	*/

	json_object *out = json_object_new_array();

	json_object *intfs = json_object_object_get(resp, "interfaces");

	for (int i = 0; i < json_object_array_length(intfs); i++) {
		struct json_object *el = json_object_array_get_idx(intfs, i);
		struct json_object *olsr = json_object_object_get(el, "olsrInterface");
		struct json_object *ip = J_OGET(olsr, "ipAddress"); // might be null (up=false)
		if (ip) {
			json_object_array_add(out, ip);
		}
	}

	return out;
}

static json_object * olsr2_get_addresses (void) {
	/*

	> olsrv2info jsonraw originator
	{"originator": [{
	"originator":"-"},{
	"originator":"fdff:182f:da60:abc:23:ee1a:dec6:d17c"}]}

	if you're wondering "what the fuck": me too, me too

	*/

	json_object *resp;

	if (olsr2_get_nodeinfo("olsrv2info jsonraw originator", &resp))
		return NULL;

	json_object *out = json_object_new_array();

	json_object *origs = json_object_object_get(resp, "originator");

	for (int i = 0; i < json_object_array_length(origs); i++) {
		json_object *el = json_object_array_get_idx(origs, i);
		json_object *orig = J_OGET(el, "originator");
		if (json_object_get_string(orig)[0] != "-"[0]) {
			json_object_array_add(out, orig);
		}
	}

	json_object_put(resp);

	return out;
}

struct json_object * olsr1_get_interfaces (void) {
	json_object *resp;

	if (olsr1_get_nodeinfo("interfaces", &resp))
		return NULL;

	/*

	interfaces []
		name	"m_uplink"
		configured	true
		hostEmulation	false
		hostEmulationAddress	"0.0.0.0"
		olsrInterface
			-- might be false (and then ipAddress key is missing)
			up	true
			ipv4Address	"10.12.23.234"
			ipv4Netmask	"255.255.0.0"
			ipv4Broadcast	"10.12.255.255"
			mode	"mesh"
			ipv6Address	"::"
			ipv6Multicast	"::"
			-- we need this
			ipAddress	"10.12.23.234"
			..
		InterfaceConfiguration	{…}
		InterfaceConfigurationDefaults	{…}
	*/

	json_object *out = json_object_new_object();

	json_object *intfs = json_object_object_get(resp, "interfaces");

	for (int i = 0; i < json_object_array_length(intfs); i++) {
		json_object *el = json_object_array_get_idx(intfs, i);
		json_object *olsr = json_object_object_get(el, "olsrInterface");

		json_object *intf = json_object_new_object();
		json_object_object_add(out,
			json_object_get_string(json_object_object_get(el, "name")),
			intf
		);

		J_OCPY(intf, el, "configured");
		J_OCPY(intf, olsr, "up");
		J_OCPY(intf, olsr, "ipAddress");
		J_OCPY(intf, olsr, "mode");
	}

	return out;
}

struct json_object * olsr2_get_interfaces (void) {
	json_object *resp;

	if (olsr2_get_nodeinfo("nhdpinfo jsonraw interface", &resp))
		return NULL;

	/*

	we're currently just using nhdpinfo, but layer2info might be interesting at some point

	> nhdpinfo jsonraw interface
	{"interface": [{
	"if":"ibss0",
	"if_bindto_v4":"-",
	"if_bindto_v6":"-",
	"if_mac":"b8:69:f4:0d:1a:3c",
	"if_flooding_v4":"false",
	"if_flooding_v6":"false",
	"if_dualstack_mode":"-"},{
	"if":"lo",
	"if_bindto_v4":"-",
	"if_bindto_v6":"fdff:182f:da60:abc:23:ee1a:dec6:d17c",
	"if_mac":"00:00:00:00:00:00",
	"if_flooding_v4":"false",
	"if_flooding_v6":"false",
	"if_dualstack_mode":"-"},{

	> layer2info jsonraw interface
	{"interface": [{
	"if":"ibss0",
	"if_index":14,
	"if_local_addr":"b8:69:f4:0d:1a:3c",
	"if_type":"wireless",
	"if_dlep":"false",
	"if_ident":"",
	"if_ident_addr":"",
	"if_lastseen":0,
	"if_frequency1":"0",
	"if_frequency2":"0",
	"if_bandwidth1":"0",
	"if_bandwidth2":"0",
	"if_noise":"-92",
	"if_ch_active":"40448.827",
	"if_ch_busy":"1015.889",
	"if_ch_rx":"263.867",
	"if_ch_tx":"127.433",
	"if_mtu":"0",
	"if_mcs_by_probing":"true",
	"if_rx_only_unicast":"false",
	"if_tx_only_unicast":"false",
	"if_frequency1_origin":"",
	"if_frequency2_origin":"",
	"if_bandwidth1_origin":"",
	"if_bandwidth2_origin":"",
	"if_noise_origin":"nl80211",
	"if_ch_active_origin":"nl80211",
	"if_ch_busy_origin":"nl80211",
	"if_ch_rx_origin":"nl80211",
	"if_ch_tx_origin":"nl80211",
	"if_mtu_origin":"",
	"if_mcs_by_probing_origin":"nl80211",
	"if_rx_only_unicast_origin":"",
	"if_tx_only_unicast_origin":""},{
	"if":"lo",
	"if_index":1,
	"if_local_addr":"00:00:00:00:00:00",
	"if_type":"wireless",
	"if_dlep":"false",
	"if_ident":"",
	"if_ident_addr":"",
	"if_lastseen":0,
	"if_frequency1":"0",
	"if_frequency2":"0",
	"if_bandwidth1":"0",
	"if_bandwidth2":"0",
	"if_noise":"0",
	"if_ch_active":"0",
	"if_ch_busy":"0",
	"if_ch_rx":"0",
	"if_ch_tx":"0",
	"if_mtu":"0",
	"if_mcs_by_probing":"false",
	"if_rx_only_unicast":"false",
	"if_tx_only_unicast":"false",
	"if_frequency1_origin":"",
	"if_frequency2_origin":"",
	"if_bandwidth1_origin":"",
	"if_bandwidth2_origin":"",
	"if_noise_origin":"",
	"if_ch_active_origin":"",
	"if_ch_busy_origin":"",
	"if_ch_rx_origin":"",
	"if_ch_tx_origin":"",
	"if_mtu_origin":"",
	"if_mcs_by_probing_origin":"",
	"if_rx_only_unicast_origin":"",
	"if_tx_only_unicast_origin":""},{

	*/

	json_object *out = json_object_new_object();

	json_object *intfs = json_object_object_get(resp, "interface");

	for (int i = 0; i < json_object_array_length(intfs); i++) {
		json_object *el = json_object_array_get_idx(intfs, i);

		json_object *intf = json_object_new_object();
		json_object_object_add(out,
			json_object_get_string(json_object_object_get(el, "if")),
			intf
		);

		J_OCPY2(intf, el, "mac", "if_mac");

		if (json_object_get_string(json_object_object_get(el, "if_bindto_v4"))[0] != "-"[0]) {
			J_OCPY2(intf, el, "if_bindto_v4", "v4");
		} else {
			json_object_object_add(intf, "v4", json_object_new_null());
		}

		if (json_object_get_string(json_object_object_get(el, "if_bindto_v6"))[0] != "-"[0]) {
			J_OCPY2(intf, el, "v6", "if_bindto_v6");
		} else {
			json_object_object_add(intf, "v6", json_object_new_null());
		}
	}

	return out;
}

static void add_if_not_empty(struct json_object *obj, const char *key, struct json_object *val) {
	if (json_object_array_length(val))
		json_object_object_add(obj, key, val);
	else
		json_object_put(val);
}

static void mesh_add_subif(const char *ifname, struct json_object *wireless, struct json_object *wired,
		struct json_object *tunnel, struct json_object *other) {
	struct json_object *address = gluonutil_wrap_and_free_string(gluonutil_get_interface_address(ifname));

	/* In case of VLAN and bridge interfaces, we want the lower interface
	 * to determine the interface type (but not for the interface address) */
	char lowername[IF_NAMESIZE];
	gluonutil_get_interface_lower(lowername, ifname);

	switch(gluonutil_get_interface_type(lowername)) {
	case GLUONUTIL_INTERFACE_TYPE_WIRELESS:
		json_object_array_add(wireless, address);
		break;

	case GLUONUTIL_INTERFACE_TYPE_WIRED:
		json_object_array_add(wired, address);
		break;

	case GLUONUTIL_INTERFACE_TYPE_TUNNEL:
		json_object_array_add(tunnel, address);
		break;

	default:
		json_object_array_add(other, address);
	}
}

static struct json_object * get_mesh_subifs(const char *ifname) {
	struct json_object *wireless = json_object_new_array();
	struct json_object *wired = json_object_new_array();
	struct json_object *tunnel = json_object_new_array();
	struct json_object *other = json_object_new_array();

	const char *format = "/sys/class/net/%s/lower_*";
	char pattern[strlen(format) + strlen(ifname) - 1];
	snprintf(pattern, sizeof(pattern), format, ifname);

	size_t pattern_len = strlen(pattern);

	glob_t lower;
	int globreturn;
	if (!(globreturn = glob(pattern, GLOB_NOSORT, NULL, &lower))) {
		size_t i;
		for (i = 0; i < lower.gl_pathc; i++) {
			mesh_add_subif(lower.gl_pathv[i] + pattern_len - 1,
					wireless, wired, tunnel, other);
		}

		globfree(&lower);

		// TODO: add the device's own mac aswell
		// not sure if we're handling this correctly and if it may make more sense
		// to just query this
		mesh_add_subif(ifname, wireless, wired, tunnel, other);
	} else if (globreturn == GLOB_NOMATCH) {
		// this is already a lower interface, add directly
		mesh_add_subif(ifname, wireless, wired, tunnel, other);
	}

	struct json_object *ret = json_object_new_object();
	add_if_not_empty(ret, "wireless", wireless);
	add_if_not_empty(ret, "wired", wired);
	add_if_not_empty(ret, "tunnel", tunnel);
	add_if_not_empty(ret, "other", other);
	return ret;
}

struct json_object * real_respondd_provider_nodeinfo() {
	struct olsr_info info;

	json_object *ret = NULL;

	if (oi(&info))
		return ret;

	ret = json_object_new_object();
	if (!ret) {
		return NULL;
	}

	struct json_object *network = json_object_new_object();

	struct json_object *n_addresses = json_object_new_array();

	json_object_object_add(network, "addresses", n_addresses);

	struct json_object *n_interfaces = json_object_new_object();

	json_object_object_add(network, "interfaces", n_interfaces);

	struct json_object *n_mesh = json_object_new_object();

	json_object_object_add(network, "mesh", n_mesh);

	json_object_object_add(ret, "network", network);

	struct json_object *software = json_object_new_object();

	json_object_object_add(ret, "software", software);

	if (info.olsr1.enabled) {
		struct json_object *software_olsr1 = json_object_new_object();

		json_object_object_add(software_olsr1, "running", json_object_new_boolean(info.olsr1.running));

		if (info.olsr1.running) {
			struct json_object *version = olsr1_get_version();
			if (version)
				json_object_object_add(software_olsr1, "version", version);

			struct json_object *plugins = olsr1_get_plugins();
			if (plugins)
				json_object_object_add(software_olsr1, "plugins", plugins);

			struct json_object *addresses = olsr1_get_addresses();
			if (addresses) {
				json_object_object_add(software_olsr1, "addresses", addresses);

				for (int i = 0; i < json_object_array_length(addresses); i++)
					json_object_array_add(n_addresses, json_object_get(json_object_array_get_idx(addresses, i)));
			}

			struct json_object *interfaces = olsr1_get_interfaces();
			if (interfaces) {
				json_object_object_add(software_olsr1, "interfaces", interfaces);

				json_object_object_foreach(interfaces, name, interface) {
					json_object *merged_interface = json_object_object_get(n_interfaces, name);

					if (!merged_interface) {
						merged_interface = json_object_new_object();
						json_object_object_add(n_interfaces, name, merged_interface);
					}

					json_object_object_add(merged_interface, "olsr1", json_object_get(interface));
				}
			}
		}

		json_object_object_add(software, "olsr1", software_olsr1);
	}

	if (info.olsr2.enabled) {
		struct json_object *software_olsr2 = json_object_new_object();

		json_object_object_add(software_olsr2, "running", json_object_new_boolean(info.olsr2.running));

		if (info.olsr2.running) {
			struct json_object *version = olsr2_get_version();
			if (version)
				json_object_object_add(software_olsr2, "version", version);

			struct json_object *addresses = olsr2_get_addresses();
			if (addresses) {
				json_object_object_add(software_olsr2, "addresses", addresses);

				for (int i = 0; i < json_object_array_length(addresses); i++)
					json_object_array_add(n_addresses, json_object_get(json_object_array_get_idx(addresses, i)));
			}

			struct json_object *interfaces = olsr2_get_interfaces();
			if (interfaces) {
				json_object_object_add(software_olsr2, "interfaces", interfaces);

				json_object_object_foreach(interfaces, name, interface) {
					json_object *merged_interface = json_object_object_get(n_interfaces, name);

					if (!merged_interface) {
						merged_interface = json_object_new_object();
						json_object_object_add(n_interfaces, name, merged_interface);
					}

					json_object_object_add(merged_interface, "olsr2", json_object_get(interface));
				}
			}
		}

		json_object_object_add(software, "olsr2", software_olsr2);
	}

	json_object_object_foreach(n_interfaces, name, value) {
		if (strcmp(name, "lo")) { // everything that ISN'T loopback
			struct json_object * intf = json_object_new_object();
			json_object_object_add(intf, "interfaces", get_mesh_subifs(name));
			json_object_object_add(n_mesh, name, intf);
		}
	}

	return ret;
}

make_safe_fnc(respondd_provider_nodeinfo, "i")
