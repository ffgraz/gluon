#include <stdbool.h>

#define ARP_STRING_LEN  1023
#define ARP_BUFFER_LEN  (ARP_STRING_LEN + 1)

struct arp_cache {
  char ipAddr[ARP_BUFFER_LEN];
  char hwAddr[ARP_BUFFER_LEN];
  char device[ARP_BUFFER_LEN];
  struct arp_cache * next;
};

char * resolve_mac(struct arp_cache * cache, const char * intf, const char * ip, bool active_resolve);

struct arp_cache * read_arp_cache ();
void cleanup_arp_cache (struct arp_cache * cache);
