#include <libolsrdhelper.h>
#include "respondd-common.h"

int main (int argc, char *argv[]) {
  struct json_object *resp;

  if (!respondd_provider_neighbours())
    return 1;

  if (!respondd_provider_nodeinfo())
    return 1;

  if (olsr1_get_nodeinfo("all", &resp))
    return 1;

  if (olsr2_get_nodeinfo("nhdpinfo jsonraw link", &resp))
    return 1;

  return 0;
}
