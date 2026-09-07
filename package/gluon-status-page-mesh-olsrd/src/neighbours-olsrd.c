#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <json-c/json.h>

#include <libubox/uclient.h>
#include <libubox/blobmsg.h>
#include <libubox/uloop.h>
#include <libgluonutil.h>
#include <libolsrdhelper.h>

int main(void) {
	struct json_object *site = gluonutil_load_site_config();
	if (!site)
		return 1;

	printf("Content-type: text/event-stream\n\n");
	fflush(stdout);

	while (1) {
		struct olsr_info info;

		/* re-read every round, a daemon may have been started or stopped */
		if (!olsr_get_info(site, &info)) {
			struct json_object *obj = olsr_get_merged_neighs(&info);
			if (obj) {
				printf("data: %s\n\n", json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN));
				fflush(stdout);
				json_object_put(obj);
			}
		}

		sleep(10);
	}

	return 0;
}
