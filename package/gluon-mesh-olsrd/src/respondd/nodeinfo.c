/* SPDX-FileCopyrightText: 2021-2023 Maciej Krüger <maciej@xeredo.it> */
/* SPDX-FileCopyrightText: 2016-2019, Matthias Schiffer <mschiffer@universe-factory.net> */
/* SPDX-License-Identifier: BSD-2-Clause */

#include "providers.h"

#include <libgluonutil.h>

#include <json-c/json.h>

#include <libolsrdhelper.h>

#include <glob.h>
#include <stdio.h>
#include <string.h>

static json_object * olsr_get_plugins(int ipv) {
	json_object *resp;

	if (olsr_get_nodeinfo(ipv, "plugins", &resp))
		return NULL;

	J_OUT(json_object_object_get(resp, "plugins"));
}

static json_object * olsr_get_version(int ipv) {
	json_object *resp;

	if (olsr_get_nodeinfo(ipv, "version", &resp))
		return NULL;

	J_OUT(json_object_object_get(json_object_object_get(resp, "version"), "version"));
}

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
static json_object * olsr_get_addresses(int ipv) {
	json_object *resp;

	if (olsr_get_nodeinfo(ipv, "interfaces", &resp))
		return NULL;

	json_object *out = json_object_new_array();

	json_object *intfs = json_object_object_get(resp, "interfaces");

	for (size_t i = 0; intfs && i < json_object_array_length(intfs); i++) {
		struct json_object *el = json_object_array_get_idx(intfs, i);
		struct json_object *olsr = json_object_object_get(el, "olsrInterface");
		struct json_object *ip = J_OGET(olsr, "ipAddress"); // might be null (up=false)

		if (ip)
			json_object_array_add(out, ip);
	}

	json_object_put(resp);

	return out;
}

static struct json_object * olsr_get_interfaces(int ipv) {
	json_object *resp;

	if (olsr_get_nodeinfo(ipv, "interfaces", &resp))
		return NULL;

	json_object *out = json_object_new_object();

	json_object *intfs = json_object_object_get(resp, "interfaces");

	for (size_t i = 0; intfs && i < json_object_array_length(intfs); i++) {
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

	json_object_put(resp);

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

/** Collects what one of the daemons has to say about itself */
static struct json_object * get_software(int ipv, const struct olsr_daemon_info *daemon,
		struct json_object *n_addresses, struct json_object *n_interfaces) {
	struct json_object *software = json_object_new_object();

	json_object_object_add(software, "running", json_object_new_boolean(daemon->running));

	if (!daemon->running)
		return software;

	struct json_object *version = olsr_get_version(ipv);
	if (version)
		json_object_object_add(software, "version", version);

	struct json_object *plugins = olsr_get_plugins(ipv);
	if (plugins)
		json_object_object_add(software, "plugins", plugins);

	struct json_object *addresses = olsr_get_addresses(ipv);
	if (addresses) {
		json_object_object_add(software, "addresses", addresses);

		for (size_t i = 0; i < json_object_array_length(addresses); i++)
			json_object_array_add(n_addresses, json_object_get(json_object_array_get_idx(addresses, i)));
	}

	struct json_object *interfaces = olsr_get_interfaces(ipv);
	if (interfaces) {
		json_object_object_add(software, "interfaces", interfaces);

		json_object_object_foreach(interfaces, name, interface) {
			json_object *merged_interface = json_object_object_get(n_interfaces, name);

			if (!merged_interface) {
				merged_interface = json_object_new_object();
				json_object_object_add(n_interfaces, name, merged_interface);
			}

			json_object_object_add(merged_interface, olsr_name(ipv), json_object_get(interface));
		}
	}

	return software;
}

struct json_object * respondd_provider_nodeinfo() {
	struct olsr_info info;

	if (olsr_get_info(&info))
		return NULL;

	struct json_object *ret = json_object_new_object();
	if (!ret)
		return NULL;

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

	static const int families[] = { OLSR_IPV4, OLSR_IPV6 };

	for (size_t i = 0; i < sizeof(families) / sizeof(families[0]); i++) {
		int ipv = families[i];
		const struct olsr_daemon_info *daemon = olsr_daemon(&info, ipv);

		if (!daemon->enabled)
			continue;

		json_object_object_add(software, olsr_name(ipv),
			get_software(ipv, daemon, n_addresses, n_interfaces));
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
