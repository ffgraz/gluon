/* SPDX-FileCopyrightText: 2016, Matthias Schiffer <mschiffer@universe-factory.net> */
/* SPDX-License-Identifier: BSD-2-Clause */


#include <respondd.h>

#include <json-c/json.h>
#include <libgluonutil.h>
#include <uci.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ifaddrs.h>

#include <netdb.h>
#include <errno.h>
#include <libbabelhelper/babelhelper.h>
#include <gluon-neighbours.h>

#include <libubox/blobmsg_json.h>
#include <libubus.h>

#define SOCKET_INPUT_BUFFER_SIZE 255

#define PROTOLEN 32

#define UBUS_TIMEOUT 30000

static struct babelhelper_ctx bhelper_ctx = {};

static bool get_linklocal_address(const char *ifname, char lladdr[INET6_ADDRSTRLEN]) {
	struct ifaddrs *ifaddr, *ifa;
	bool ret = false;

	if (getifaddrs(&ifaddr) == -1) {
		perror("getifaddrs");
		return false;
	}

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (!ifa->ifa_addr)
			continue;

		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;

		if (strcmp(ifname, ifa->ifa_name) != 0)
			continue;

		const struct in6_addr *address = &((const struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
		if (!IN6_IS_ADDR_LINKLOCAL(address))
			continue;

		if (!inet_ntop(AF_INET6, address, lladdr, INET6_ADDRSTRLEN)) {
			perror("inet_ntop");
			continue;
		}

		ret = true;
		break;
	}

	freeifaddrs(ifaddr);
	return ret;
}


static char*  get_line_from_run(const char* command) {
	FILE *fp;
	char *line = NULL;
	size_t len = 0;

	fp = popen(command, "r");

	if (fp != NULL) {
		ssize_t r = getline(&line, &len, fp);
		if (r >= 0) {
			len = strlen(line);

			if (len && line[len-1] == '\n')
				line[len-1] = 0;
		}
		else {
			free(line);
			line = NULL;
		}

		pclose(fp);
	}
	return line;
}

/** Collects the IPv4 address of every interface babeld runs on */
static bool handle_interface_address(char **data, void *arg) {
	struct json_object *out = (struct json_object *)arg;

	/* An interface line names the interface in INTERFACE, not IF -
	   that one belongs to the neighbour and route lines. */
	if (!data[INTERFACE] || !data[IPV4])
		return true;

	if (data[UP] && strcmp(data[UP], "true"))
		return true;

	if (!strcmp(data[IPV4], "0.0.0.0"))
		return true;

	struct json_object *address = json_object_new_string(data[IPV4]);

	for (size_t i = 0; i < json_object_array_length(out); i++) {
		if (json_object_equal(json_object_array_get_idx(out, i), address)) {
			json_object_put(address);
			return true;
		}
	}

	json_object_array_add(out, address);

	return true;
}

/** The node address gluon put on this node, from its own configuration */
static char * get_node_address(void) {
	struct uci_context *ctx = uci_alloc_context();
	if (!ctx)
		return NULL;

	ctx->flags &= ~UCI_FLAG_STRICT;

	char *ret = NULL;
	struct uci_ptr ptr = {
		.package = "network",
		.section = "loopback",
		.option = "ip6addr",
	};

	if (uci_lookup_ptr(ctx, &ptr, NULL, false) == UCI_OK &&
			ptr.o && ptr.o->type == UCI_TYPE_STRING && ptr.o->v.string) {
		ret = strdup(ptr.o->v.string);

		char *slash = strchr(ret, '/');
		if (slash)
			*slash = 0;
	}

	uci_free_context(ctx);

	return ret;
}

static struct json_object * get_addresses(void) {
	struct json_object *retval = json_object_new_array();

	/*
		The node address itself, taken from the configuration rather
		than derived from a MAC and the site prefix: which of the
		node's MACs it is built from is the site's business - ffgraz
		generates it in gluon-static-ip - and a derivation that
		guesses wrong names an address the node does not have.
	*/
	char *address = get_node_address();
	if (address) {
		json_object_array_add(retval, json_object_new_string(address));
		free(address);
	}

	/*
		Where babel carries IPv4 as well there is no olsrd to report
		the node's addresses of that family, and babeld already knows
		them - it names them on every interface it runs on.
	*/
	babelhelper_readbabeldata(&bhelper_ctx, "dump", (void*)retval, handle_interface_address);

	return retval;
}

/*
	network.mesh lists the MAC of every interface the node meshes over,
	the way batman-adv and gluon-mesh-olsrd report it - a link-local
	address there is not what any consumer reads, and it left babel
	nodes looking like they had no mesh interfaces at all.
*/
static void mesh_add_if(const char *ifname, struct json_object *mesh) {
	struct json_object *address = gluonutil_wrap_and_free_string(gluonutil_get_interface_address(ifname));

	if (!address)
		return;

	/* In case of VLAN and bridge interfaces, we want the lower interface
	 * to determine the interface type (but not for the interface address) */
	char lowername[IF_NAMESIZE];
	gluonutil_get_interface_lower(lowername, ifname);

	const char *type;
	switch(gluonutil_get_interface_type(lowername)) {
	case GLUONUTIL_INTERFACE_TYPE_WIRELESS:
		type = "wireless";
		break;

	case GLUONUTIL_INTERFACE_TYPE_WIRED:
		type = "wired";
		break;

	case GLUONUTIL_INTERFACE_TYPE_TUNNEL:
		type = "tunnel";
		break;

	default:
		type = "other";
	}

	struct json_object *entry = json_object_object_get(mesh, ifname);
	if (!entry) {
		entry = json_object_new_object();
		json_object_object_add(entry, "interfaces", json_object_new_object());
		json_object_object_add(mesh, ifname, entry);
	}

	struct json_object *interfaces = json_object_object_get(entry, "interfaces");

	struct json_object *list = json_object_object_get(interfaces, type);
	if (!list) {
		list = json_object_new_array();
		json_object_object_add(interfaces, type, list);
	}

	/* netifd's dump nests an interface deep enough that the walk
	   reaches the same one more than once */
	for (size_t i = 0; i < json_object_array_length(list); i++) {
		if (json_object_equal(json_object_array_get_idx(list, i), address)) {
			json_object_put(address);
			return;
		}
	}

	json_object_array_add(list, address);
}


static bool handle_neighbour(char **data, void *obj) {
	if (data[NEIGHBOUR]) {
		struct json_object *neigh = json_object_new_object();

		if (data[RXCOST])
			json_object_object_add(neigh, "rxcost", json_object_new_int(atoi(data[RXCOST])));
		if (data[TXCOST])
			json_object_object_add(neigh, "txcost", json_object_new_int(atoi(data[TXCOST])));
		if (data[COST])
			json_object_object_add(neigh, "cost", json_object_new_int(atoi(data[COST])));
		if (data[REACH])
			json_object_object_add(neigh, "reachability", json_object_new_double(strtod(data[REACH], NULL)));

		if (!data[IF])
			return true;

		struct json_object *nif;
		if (!json_object_object_get_ex(obj, data[IF], &nif)) {
			char str_ip[INET6_ADDRSTRLEN];

			nif = json_object_new_object();

			if (get_linklocal_address(data[IF], str_ip))
				json_object_object_add(nif, "ll-addr", json_object_new_string(str_ip));

			json_object_object_add(nif, "protocol", json_object_new_string("babel"));
			json_object_object_add(obj, data[IF], nif);

			json_object_object_add(nif, "neighbours", json_object_new_object());
		}

		struct json_object *neighborcollector;
		json_object_object_get_ex(nif, "neighbours", &neighborcollector);
		json_object_object_add(neighborcollector, data[ADDRESS], neigh);

	}
	return true;
}

static struct json_object * get_babel_neighbours(void) {

	struct json_object *neighbours;
	neighbours  = json_object_new_object();
	if (!neighbours)
		return NULL;

	babelhelper_readbabeldata(&bhelper_ctx, "dump", (void*)neighbours, handle_neighbour);

	return(neighbours);
}

/*
	babel rates a link by cost, where 96 is a perfect one and 65535 is
	no link at all; batman-adv rates it by tq, where 255 is perfect and
	0 is none. Scale the one onto the other so that a consumer reading
	tq gets the same idea of the link from either mesh.
*/
static int cost_to_tq(int cost) {
	if (cost <= 0 || cost >= 65535)
		return 0;

	int tq = 255 * 96 / cost;

	return tq > 255 ? 255 : tq;
}

/** Whether prefix is a single node address out of the site's node prefix */
static bool is_node_address(const char *prefix, char out[INET6_ADDRSTRLEN + 1]) {
	struct in6_addr node_prefix = {};
	struct in6_addr addr = {};

	const char *slash = strchr(prefix, '/');
	if (!slash || strcmp(slash, "/128"))
		return false;

	size_t len = slash - prefix;
	if (len > INET6_ADDRSTRLEN)
		return false;

	char buf[INET6_ADDRSTRLEN + 1] = {};
	memcpy(buf, prefix, len);

	if (inet_pton(AF_INET6, buf, &addr) != 1)
		return false;

	if (!gluonutil_get_node_prefix6(&node_prefix))
		return false;

	/* the node part is what the prefix leaves free, so the prefix
	   itself is what has to match */
	if (memcmp(addr.s6_addr, node_prefix.s6_addr, 8))
		return false;

	memcpy(out, buf, len + 1);

	return true;
}

/*
	The address a neighbour announces itself under, collected from the
	routes it originates - refmetric 0 means the route starts at that
	neighbour rather than being passed on. Deriving it from the MAC
	behind its link-local address would only be a guess: which of a
	node's MACs its address is built from is up to the site.
*/
static bool handle_neigh_address(char **data, void *arg) {
	struct json_object *map = (struct json_object *)arg;
	char address[INET6_ADDRSTRLEN + 1] = {};

	if (!data[ROUTE] || !data[PREFIX] || !data[VIA] || !data[REFMETRIC])
		return true;

	if (strcmp(data[REFMETRIC], "0"))
		return true;

	if (!is_node_address(data[PREFIX], address))
		return true;

	json_object_object_add(map, data[VIA], json_object_new_string(address));

	return true;
}

/*
	The same neighbours again, keyed by MAC and rated in tq, which is
	what gluon_neighbours_to_batadv() and everything downstream of it
	expects. babel only ever names a neighbour by its link-local
	address, so the MAC comes back out of that.
*/
struct neigh_ctx {
	struct json_object *out;
	struct json_object *addresses;
};

static bool handle_neighbour_by_mac(char **data, void *arg) {
	struct neigh_ctx *ctx = (struct neigh_ctx *)arg;
	char mac[18];

	if (!data[NEIGHBOUR] || !data[ADDRESS] || !data[IF])
		return true;

	if (!gluon_neighbours_mac_from_lladdr(data[ADDRESS], mac))
		return true;

	struct json_object *neigh = json_object_new_object();
	if (!neigh)
		return true;

	if (data[COST])
		json_object_object_add(neigh, "tq", json_object_new_int(cost_to_tq(atoi(data[COST]))));
	if (data[RXCOST])
		json_object_object_add(neigh, "rxcost", json_object_new_int(atoi(data[RXCOST])));
	if (data[TXCOST])
		json_object_object_add(neigh, "txcost", json_object_new_int(atoi(data[TXCOST])));
	if (data[COST])
		json_object_object_add(neigh, "cost", json_object_new_int(atoi(data[COST])));
	if (data[REACH])
		json_object_object_add(neigh, "reachability", json_object_new_double(strtod(data[REACH], NULL)));

	/*
		babel names a neighbour by its link-local address, which says
		nothing about the node behind it. The address that node is
		reachable and announced under is the one derived from its MAC
		and the site's node prefix, the same way this node derives its
		own in get_addresses().
	*/
	struct json_object *global = json_object_object_get(ctx->addresses, data[ADDRESS]);
	if (global)
		json_object_object_add(neigh, "ip", json_object_get(global));

	json_object_object_add(neigh, "ll_addr", json_object_new_string(data[ADDRESS]));
	json_object_object_add(neigh, "ifname", json_object_new_string(data[IF]));

	json_object_object_add(ctx->out, mac, neigh);

	return true;
}

static struct json_object * get_babel_neighbours_batadv(void) {
	struct json_object *by_mac = json_object_new_object();
	if (!by_mac)
		return NULL;

	struct neigh_ctx ctx = {
		.out = by_mac,
		.addresses = json_object_new_object(),
	};

	babelhelper_readbabeldata(&bhelper_ctx, "dump", (void*)ctx.addresses, handle_neigh_address);
	babelhelper_readbabeldata(&bhelper_ctx, "dump", (void*)&ctx, handle_neighbour_by_mac);

	json_object_put(ctx.addresses);

	struct json_object *merged = json_object_new_object();
	if (!merged) {
		json_object_put(by_mac);
		return NULL;
	}

	/* One source today, but the merge is what keeps this from
	   clobbering what another daemon reported for the same
	   neighbour once respondd puts the two together. */
	gluon_neighbours_merge(merged, by_mac, "babel");
	json_object_put(by_mac);

	struct json_object *batadv = gluon_neighbours_to_batadv(merged);
	json_object_put(merged);

	return batadv;
}

static void blobmsg_handle_list(struct blob_attr *attr, int len, bool array, struct json_object *mesh);

static void blobmsg_handle_element(struct blob_attr *attr, bool head, char **ifname, char **proto, struct json_object *mesh) {
	void *data;

	if (!blobmsg_check_attr(attr, false))
		return;

	data = blobmsg_data(attr);

	switch (blob_id(attr)) {
		case  BLOBMSG_TYPE_STRING:
			if (!strncmp(blobmsg_name(attr), "device", 6)) {
				free(*ifname);
				*ifname = strndup(data, IF_NAMESIZE);
			} else if (!strncmp(blobmsg_name(attr), "proto", 5)) {
				free(*proto);
				*proto = strndup(data, PROTOLEN);
			}
			return;
		case BLOBMSG_TYPE_ARRAY:
			blobmsg_handle_list(data, blobmsg_data_len(attr), true, mesh);
			return;
		case BLOBMSG_TYPE_TABLE:
			blobmsg_handle_list(data, blobmsg_data_len(attr), false, mesh);
	}
}

static void blobmsg_handle_list(struct blob_attr *attr, int len, bool array, struct json_object *mesh) {
	struct blob_attr *pos;
	int rem = len;

	char *ifname = NULL;
	char *proto = NULL;

	__blob_for_each_attr(pos, attr, rem) {
		blobmsg_handle_element(pos, array, &ifname, &proto, mesh);
	}

	if (ifname && proto) {
		/* A wired mesh interface runs the gluon_wired proto, and
		   leaving it out here is what made a node meshing over
		   ethernet report no mesh interface at all. */
		if (!strcmp(proto, "gluon_mesh") || !strcmp(proto, "gluon_wired")) {
			mesh_add_if(ifname, mesh);
		}
	}
	free(ifname);
	free(proto);
}

static void receive_call_result_data(struct ubus_request *req, int type, struct blob_attr *msg) {
	struct json_object *mesh = json_object_new_object();

	if (!mesh)
		return;

	if (!msg) {
		printf("empty message\n");
		json_object_put(mesh);
		return;
	}

	blobmsg_handle_list(blobmsg_data(msg), blobmsg_data_len(msg), false, mesh);

	*((struct json_object**)(req->priv)) = mesh;
}


static struct json_object * get_mesh_ifs() {
	struct ubus_context *ubus_ctx;
	struct json_object *ret = NULL;
	struct blob_buf b = {};

	unsigned int id=8;

	ubus_ctx = ubus_connect(NULL);
	if (!ubus_ctx) {
		fprintf(stderr,"could not connect to ubus, not providing mesh-data\n");
		goto end;
	}

	blob_buf_init(&b, 0);
	ubus_lookup_id(ubus_ctx, "network.interface", &id);
	int uret = ubus_invoke(ubus_ctx, id, "dump", b.head, receive_call_result_data, &ret, UBUS_TIMEOUT);

	if (uret > 0)
		fprintf(stderr, "ubus command failed: %s\n", ubus_strerror(uret));
	else if (uret == -2)
		fprintf(stderr, "invalid call, exiting\n");

	blob_buf_free(&b);

end:
	ubus_free(ubus_ctx);
	return ret;
}

/*
	Keyed by interface name, like gluon-mesh-olsrd and batman-adv report
	it, so that on a node running both daemons the two describe the same
	interfaces instead of each inventing a mesh of its own.
*/
static struct json_object * get_mesh(void) {
	struct json_object *mesh = get_mesh_ifs();

	return mesh ? mesh : json_object_new_object();
}

static struct json_object * get_babeld_version(void) {
	char *version = get_line_from_run("exec babeld -V 2>&1");
	struct json_object *ret = gluonutil_wrap_string(version);
	free(version);
	return ret;
}

static struct json_object * respondd_provider_nodeinfo(void) {
	bhelper_ctx.debug=false;
	struct json_object *ret = json_object_new_object();

	struct json_object *network = json_object_new_object();
	json_object_object_add(network, "addresses", get_addresses());
	json_object_object_add(network, "mesh", get_mesh());
	json_object_object_add(ret, "network", network);

	struct json_object *software = json_object_new_object();
	struct json_object *software_babeld = json_object_new_object();
	json_object_object_add(software_babeld, "version", get_babeld_version());
	json_object_object_add(software, "babeld", software_babeld);
	json_object_object_add(ret, "software", software);

	return ret;
}

static struct json_object * read_number(const char *ifname, const char *stat) {
	const char *format = "/sys/class/net/%s/statistics/%s";

	struct json_object *ret = NULL;
	int64_t i;

	char path[strlen(format) + strlen(ifname) + strlen(stat) + 1];
	snprintf(path, sizeof(path), format, ifname, stat);

	FILE *f = fopen(path, "r");
	if (!f)
		return NULL;

	if (fscanf(f, "%"SCNd64, &i) == 1)
		ret = json_object_new_int64(i);

	fclose(f);

	return ret;
}

static struct json_object * get_traffic(void) {
	const char *ifname = "br-client";

	struct json_object *ret = NULL;
	struct json_object *rx = json_object_new_object();
	struct json_object *tx = json_object_new_object();

	json_object_object_add(rx, "packets", read_number(ifname, "rx_packets"));
	json_object_object_add(rx, "bytes", read_number(ifname, "rx_bytes"));
	json_object_object_add(rx, "dropped", read_number(ifname, "rx_dropped"));
	json_object_object_add(tx, "packets", read_number(ifname, "tx_packets"));
	json_object_object_add(tx, "dropped", read_number(ifname, "tx_dropped"));
	json_object_object_add(tx, "bytes", read_number(ifname, "tx_bytes"));

	ret = json_object_new_object();
	json_object_object_add(ret, "rx", rx);
	json_object_object_add(ret, "tx", tx);

	return ret;
}

static bool handle_route_addgw_nexthop(char **data, void *arg) {
	struct json_object *obj = (struct json_object*) arg;
	if (data[PREFIX] && data[FROM] && data[VIA] && data[IF]) {
		if ( (! strncmp(data[PREFIX], "::/0", 4) ) && ( ! strncmp(data[FROM], "::/0", 4) ) ) {
			int gw_nexthoplen=strlen(data[VIA]) + strlen(data[IF])+2;
			char gw_nexthop[gw_nexthoplen];
			snprintf(gw_nexthop, gw_nexthoplen , "%s%%%s", data[VIA], data[IF]);
			json_object_object_add(obj, "gateway_nexthop", json_object_new_string(gw_nexthop));
		}
	}
	return true;
}

static int json_parse_get_clients(json_object * object) {
	if (object) {
		json_object_object_foreach(object, key, val) {
			if (! strncmp("clients", key, 7)) {
				return(json_object_get_int(val));
			}
		}
	}
	return(-1);
}

static int ask_l3roamd_for_client_count() {
	struct sockaddr_un addr;
	const char *socket_path = "/var/run/l3roamd.sock";
	int fd;
	int clients = -1;
	char *buf = NULL;
	int already_read = 0;

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr, "could not setup l3roamd-control-socket\n");
		return(-1);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path)-1);

	if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		fprintf(stderr, "connect error\n");
		return(-1);
	}

	if (write(fd,"get_clients\n",12) != 12) {
		perror("could not send command to l3roamd socket: get_clients");
		goto end;
	}

	int rc = 0;
	do {
		char *buf_tmp = realloc(buf, already_read + SOCKET_INPUT_BUFFER_SIZE + 1);
		if (buf_tmp == NULL) {
			fprintf(stderr, "could not allocate memory for buffer\n");
			goto end;
		}
		buf = buf_tmp;

		rc = read(fd, &buf[already_read], SOCKET_INPUT_BUFFER_SIZE);
		already_read+=rc;
		if (rc < 0) {
			perror("error on read in ask_l3roamd_for_client_count():");
			goto end;
		}
		buf[already_read]='\0';
	} while (rc == SOCKET_INPUT_BUFFER_SIZE);

	json_object * jobj = json_tokener_parse(buf);
	clients = json_parse_get_clients(jobj);
	json_object_put(jobj);

end:
	free(buf);
	close(fd);

	return clients;
}

static struct json_object * get_clients(void) {
	struct json_object *ret = json_object_new_object();

	int total = ask_l3roamd_for_client_count();
	if (total >= 0)
		json_object_object_add(ret, "total", json_object_new_int(total));

	return ret;
}

static struct json_object * respondd_provider_statistics(void) {
	struct json_object *ret = json_object_new_object();

	json_object_object_add(ret, "clients", get_clients());
	json_object_object_add(ret, "traffic", get_traffic());

	babelhelper_readbabeldata(&bhelper_ctx, "dump", (void*)ret, handle_route_addgw_nexthop );

	return ret;
}

static struct json_object * respondd_provider_neighbours(void) {
	struct json_object *ret = json_object_new_object();

	struct json_object *babel = get_babel_neighbours();
	if (babel)
		json_object_object_add(ret, "babel", babel);

	/* Reported the batman-adv way as well, so that the map and
	   everything else that only ever learned to read that shape sees
	   a babel mesh too - the same thing gluon-mesh-olsrd does. */
	struct json_object *batadv = get_babel_neighbours_batadv();
	if (batadv)
		json_object_object_add(ret, "batadv", batadv);


	return ret;
}


const struct respondd_provider_info respondd_providers[] = {
	{"nodeinfo", respondd_provider_nodeinfo},
	{"statistics", respondd_provider_statistics},
	{"neighbours", respondd_provider_neighbours},
	{}
};
