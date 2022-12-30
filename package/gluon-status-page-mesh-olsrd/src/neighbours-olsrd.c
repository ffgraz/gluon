#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <json-c/json.h>

#include <libubox/uclient.h>
#include <libubox/blobmsg.h>
#include <libubox/uloop.h>
#include <libolsrdhelper.h>

void merge_neighs(json_object * out, json_object * neighs, char * version) {
	struct json_object_iterator neighs_it = json_object_iter_begin(neighs);
	struct json_object_iterator neighs_itEnd = json_object_iter_end(neighs);

	while (!json_object_iter_equal(&neighs_it, &neighs_itEnd)) {
		const char * mac = json_object_iter_peek_name(&neighs_it);

		json_object * neighbour = json_object_object_get(out, mac);

		if (!neighbour) {
			neighbour = json_object_new_object();
			json_object_object_add(out, mac, neighbour);
		}

		json_object * neighbour_original = json_object_object_get(neighs, mac);

		struct json_object_iterator neigh_it = json_object_iter_begin(neighbour_original);
		struct json_object_iterator neigh_itEnd = json_object_iter_end(neighbour_original);

		while (!json_object_iter_equal(&neigh_it, &neigh_itEnd)) {
			const char * key = json_object_iter_peek_name(&neigh_it);

			json_object * new = json_object_object_get(neighbour_original, key);
			json_object * cur = json_object_object_get(neighbour, key);

			if (!strcmp(key, "tq")) {
				if (cur) {
					json_object_object_add(
						neighbour,
						"tq",
						json_object_new_double(
							(json_object_get_double(cur) + json_object_get_double(new)) / 2
						)
					);
				} else {
					json_object_object_add(neighbour, "tq", new);
				}
			} else if (!strcmp(key, "ip")) {
				char * str = malloc(10);
				if (!str) {
					return;
				}
				sprintf(str, "%s_%s", version, key);

				json_object_object_add(neighbour, str, new);
			} else if (!strcmp(key, "best")) {
				if (cur) {
					json_object_object_add(
						neighbour,
						"best",
						json_object_new_boolean(
							json_object_get_boolean(cur) || json_object_get_boolean(new)
						)
					);
				} else {
					json_object_object_add(neighbour, "best", new);
				}
			} else {
				json_object_object_add(neighbour, key, new);
			}
		}

	}
}

static json_object *neighbours(void) {

	struct olsr_info *info;

	if (oi(&info))
		return NULL;

	json_object *out = json_object_new_object();
	if (!out) {
		return NULL;
	}

	if (info->olsr2.running) {
		json_object *olsr2_neigh;

		olsr2_neigh = olsr2_get_neigh();
		if (!olsr2_neigh) {
			return NULL;
		}

		merge_neighs(out, olsr2_neigh, "olsr2");
	}

	if (info->olsr1.running) {
		json_object *olsr1_neigh;

		olsr1_neigh = olsr1_get_neigh();
		if (!olsr1_neigh) {
			return NULL;
		}

		merge_neighs(out, olsr1_neigh, "olsr1");
	}

	return out;
}

int main(void) {
	struct json_object *obj;

	printf("Content-type: text/event-stream\n\n");
	fflush(stdout);

	while (1) {
		obj = neighbours();
		if (obj) {
			printf("data: %s\n\n", json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN));
			fflush(stdout);
			json_object_put(obj);
		}
		sleep(10);
	}

	return 0;
}
