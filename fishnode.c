#include "fish.h"
#include "fishnode.h"
#include "smartalloc.h"

#include <assert.h>
#include <signal.h>
#include <string.h>

#define DEBUG

#define L3_IMPL

Broadcast_History_Entry *broadcast_history_head = NULL;
FiB_Entry *fib_table_head = NULL;


static int noprompt = 0;

void sigint_handler(int sig)
{
   if (SIGINT == sig)
	   fish_main_exit();
}

static int print_route(void *callback_data __attribute__((unused)),
      fnaddr_t dest, int prefix_len __attribute((unused)),
      fnaddr_t net_hop __attribute((unused)),
      int metric __attribute((unused)),
      void *entry_data __attribute__((unused))) {
   printf("%s\n", fn_ntoa(dest));
   return 0;
}

static void keyboard_callback(char *line)
{
   if (0 == strcasecmp("show neighbors", line))
      fish_print_neighbor_table();
   else if (0 == strcasecmp("show arp", line))
      fish_print_arp_table();
   else if (0 == strcasecmp("show route", line)) {
      fish_print_forwarding_table();
      fish_fwd.iterate_entries(&print_route, NULL, FISH_FWD_TYPE_BROADCAST);
   }
   else if (0 == strcasecmp("show dv", line))
      fish_print_dv_state();
   else if (0 == strcasecmp("quit", line) || 0 == strcasecmp("exit", line))
      fish_main_exit();
   else if (0 == strcasecmp("show topo", line))
      fish_print_lsa_topo();
   else if (0 == strcasecmp("help", line) || 0 == strcasecmp("?", line)) {
      printf("Available commands are:\n"
             "    exit                         Quit the fishnode\n"
             "    help                         Display this message\n"
             "    quit                         Quit the fishnode\n"
             "    show arp                     Display the ARP table\n"
             "    show dv                      Display the dv routing state\n"
             "    show neighbors               Display the neighbor table\n"
             "    show route                   Display the forwarding table\n"
             "    show topo                    Display the link-state routing\n"
             "                                 algorithm's view of the network\n"
             "                                 topology\n"
             "    ?                            Display this message\n"
            );
   }
   else if (line[0] != 0)
      printf("Type 'help' or '?' for a list of available commands.  "
             "Unknown command: %s\n", line);

   if (!noprompt)
      printf("> ");

   fflush(stdout);
}

// Prototypes for program 2.  Taken directly from fish.h header file
#ifdef L2_IMPL
int my_fish_l2_send(void *l3frame, fnaddr_t next_hop, int len, uint8_t l2_proto)
{
   return 0;
}

int my_fishnode_l2_receive(void *l2frame)
{
   return 0;
}

void my_arp_received(void *l2frame)
{
}

void my_send_arp_request(fnaddr_t l3addr)
{
}

void my_add_arp_entry(fn_l2addr_t l2addr, fnaddr_t addr, int timeout)
{
}

void my_resolve_fnaddr(fnaddr_t addr, arp_resolution_cb cb, void *param)
{
}
#endif

#ifdef L3_IMPL

void add_hist_entry(fnaddr_t src_l3, uint8_t *packet_id) {
   Broadcast_History_Entry *new_entry = (Broadcast_History_Entry *)smartalloc(sizeof(Broadcast_History_Entry), "fishnode.c", __LINE__, 0);
   new_entry->src_l3 = src_l3;
   memcpy(new_entry->packet_id, packet_id, 4);
   new_entry->next_entry = NULL;

   Broadcast_History_Entry *current_entry = broadcast_history_head;
   if (current_entry == NULL) {
      broadcast_history_head = new_entry;
      return;
   }

   while (current_entry->next_entry != NULL) {
      current_entry = current_entry->next_entry;
   }
   current_entry->next_entry = new_entry;
}

int check_bcast_hist(fnaddr_t src_l3, uint8_t *packet_id) {
   Broadcast_History_Entry *current_node = broadcast_history_head;

   int diff_ids = 1;
   while(current_node != NULL) {
      diff_ids = memcmp(current_node->packet_id, packet_id, 4);
      if (current_node->src_l3 == src_l3 && diff_ids == 0) {
         // Returning 1 means that it does exist (caller should drop the packet)
         return 1; 
      }
         
      current_node = current_node->next_entry;
   }
   
   // Returing 0 means that the packet is not in the table. Continue forwarding
   return 0;
}

/* Returns the first entry in the FiB of the group that 
   matches the destination l3 addr */
FiB_Entry *check_fib(fnaddr_t dst) {
   FiB_Entry *current_entry = fib_table_head;

   if (current_entry == NULL) {return NULL;}

   while (current_entry != NULL) {
      if (current_entry->dst == dst) {return current_entry;}

      current_entry = current_entry->next_entry;
   }

   return NULL;
}

int netmask_to_slash(uint32_t netmask) {
   int prefix_len = 0;
   for (int i = 0U; i < 32U; i++) {
      if (netmask & (1 << (31U - i))) 
         prefix_len++;
   }
   return prefix_len;
}

int dv_receive(void *l3frame, int len) {
   if (len < (int)sizeof(DV_Ad_Header)) {
      return 0;
   }

   DV_Ad_Header *dv_packet = l3frame;
   if (dv_packet->num_ads == 0) {return 0;}  // No ads => nothing to do

   // Check if the len is enough for the size of the packet
   uint32_t num_ad_route_bytes = ntohs(dv_packet->num_ads) * sizeof(DV_Ad_Route);
   if (len < (sizeof(DV_Ad_Header) + num_ad_route_bytes)) {
      return 0;
   }

   fnaddr_t src = dv_packet->l3_addr; 
   DV_Ad_Route *current_route;
   uint8_t *start_ads =  ((uint8_t *)l3frame) + sizeof(DV_Ad_Header);
   for (uint32_t i = 0; i < ntohs(dv_packet->num_ads); i++) {
      current_route = (DV_Ad_Route *)(start_ads + (i * sizeof(DV_Ad_Route)));

      // Per route, check if we need to update FiB
      if (current_route->metric >= MAX_TTL) {continue;}
      
      FiB_Entry *fib_entry = check_fib(current_route->dest);
      if (fib_entry == NULL) {
         int prefix_length = netmask_to_slash(current_route->netmask);
         fish_fwd.add_fwtable_entry(current_route->dest, 
                                    prefix_length, 
                                    src, 
                                    current_route->metric + 1, 
                                    FISH_FWD_TYPE_DV, 
                                    NULL);
         continue;
      }

      /* When checking if i should replace an entry, I need to check:

      - Next hop is the same: if new is higher, REPLACE (something happened in 
         the network such that it now takes longer to get to dest). Else, 
         DONT change current entry  
      
      - NH is diff: If new is lower, then this is an alternative route that is 
         better than current (REPLACE old entry). Else, DONT change since our
         old entry is a better option.
      */       
      while (fib_entry != NULL && fib_entry->dst == current_route->dest) {
         if (fib_entry->type == FISH_FWD_TYPE_DV) {
            if (fib_entry->next_hop == src && 
               (current_route->metric + 1) > fib_entry->metric) {
               
               fish_fwd.update_fwtable_metric(fib_entry, current_route->metric + 1);
            }
            else if (fib_entry->next_hop != src &&
               (current_route->metric + 1) < fib_entry->metric) {
               
               fish_fwd.update_fwtable_metric(fib_entry, current_route->metric + 1);
            }

         }
         fib_entry = fib_entry->next_entry;
      }

   }

   return 0;
}

int my_fishnode_l3_receive(void *l3frame, int len, uint8_t protocol)
{
   // Separate DV ad handling
   if (protocol == DV_ROUTING_PROTOCOL) {
      dv_receive(l3frame, len);

      return 0;
   }

   
   // Handle regular L3 Packet
   
   // Drop packet with invalid packet length
   if (len < sizeof(L3_Header)) {return 0;}
   
   L3_Header *packet = l3frame;

   // TODO: Remove Debug print later
   fprintf(stderr,
        "\n\nDEBUG L3 receive: dest=%08X local=%08X protocol=%u l4proto=%u\n",
        ntohl(packet->dest_l3),
        ntohl(fish_getaddress()),
        protocol,
        packet->l4_protocol);

   if (packet->ttl > MAX_TTL) {
      return 0;   // Drop this invalid packet. TTL is too big
   }

   int l3frame_data_size = len - sizeof(L3_Header);
   uint8_t *l4frame = ((uint8_t *)l3frame) + sizeof(L3_Header);

   if (packet->dest_l3 == fish_getaddress()) {
      // Our node matches destination of packet. Send to l4
      fprintf(stderr,
        "\n\nDEBUG L3 receive: destination is local; passing to L4, len=%d\n",
        l3frame_data_size);  // TODO: Remove later
      
      fish_l4.fish_l4_receive(l4frame, l3frame_data_size, packet->l4_protocol, packet->src_l3);
      return 0;
   }
   
   if (packet->dest_l3 == ALL_NEIGHBORS){  // This packet is a broadcast 
      
      if (check_bcast_hist(packet->src_l3, packet->packet_id)) {
         // This node has already received the packet. DROP it with no FCMP msg
         return 0;   
      }
         
      // Pass up network stack and forward packet
      fish_l4.fish_l4_receive(l4frame, l3frame_data_size,  packet->l4_protocol, packet->src_l3);

      // Continue broadcasting packet: add to history, decrement ttl, forward 
      add_hist_entry(packet->src_l3, packet->packet_id);
      
      L3_Header *forward_copy = (L3_Header *)smartalloc(len, "fishnode.c", __LINE__, 0);
      memcpy(forward_copy, packet, len);
      
      if (forward_copy->ttl > 0) {
         forward_copy->ttl = forward_copy->ttl - 1;
      }
      
      fish_l3.fish_l3_forward(forward_copy, len); 

      smartfree(forward_copy, "fishnode.c", __LINE__);
      return 0;
   }

   // Packet needs to be forwarded (not broadcast)
   L3_Header *forward_copy = (L3_Header *)smartalloc(len, "fishnode.c", __LINE__, 0);
   memcpy(forward_copy, packet, len);
   
   // forward packet. Decrement TTL
   if (forward_copy->ttl > 0) {
      forward_copy->ttl = forward_copy->ttl - 1;
   }

   fish_l3.fish_l3_forward(forward_copy, len); 

   smartfree(forward_copy, "fishnode.c", __LINE__);
   return 0;
}

int my_fish_l3_send(void *l4frame, int len, fnaddr_t dst_addr,
               uint8_t proto, uint8_t ttl)
{
   if (len < 0 || 
       len > (MTU - sizeof(L3_Header) - sizeof(L2_Header))) {
      return 0;   // Invalid l3 payload (l4 stuff) size 
   }
   
   L3_Header *l3frame = smartalloc(len + sizeof(L3_Header), "fishnode.c", __LINE__, 0);
   
   if (ttl == 0 || ttl > MAX_TTL) {ttl = MAX_TTL;}    // Ensures ttl value is within our accepted range
   l3frame->ttl = ttl;
   l3frame->l4_protocol = proto;
   uint32_t pkt_id = htonl(fish_next_pktid());
   memcpy(l3frame->packet_id, &pkt_id, 4);
   l3frame->src_l3 = fish_getaddress();
   l3frame->dest_l3 = dst_addr;
   memcpy((uint8_t *)l3frame + sizeof(L3_Header), l4frame, len);
   
   fish_l3.fish_l3_forward(l3frame, len + sizeof(L3_Header)); 

   smartfree(l3frame, "fishnode.c", __LINE__);
   return 0;
}


int l3_broadcast_forward_cb(void *callback_data, fnaddr_t dest, int prefix_len, 
                         fnaddr_t next_hop, int metric, void *entry_data) {
   FWD_Callback_Data *cb_data = callback_data;
   L3_Header *l3frame = cb_data->l3frame;
   
   // Bcast means we dont care about prefixes or anything like that
   fish_l2.fish_l2_send(l3frame, next_hop, cb_data->len, 1);
   (void) dest;
   (void) prefix_len;
   (void) metric;
   (void) entry_data;
   return 0;
}

int my_fish_l3_forward(void *l3frame, int len)
{
   L3_Header *packet = l3frame;
   
   if (len <= sizeof(L3_Header)) {
      return 0; // invalid packet size 
   }

   // Check for invalid packet with its TTL
   if (packet->ttl == 0 && packet->dest_l3 != fish_getaddress()) {
      // generate FCMP TTL Exceeded error message and drop packet
      fish_fcmp.send_fcmp_response(l3frame, len, TTL_EXCEEDED);
      return 0;
   }
   
   // Handle l3 bcast packet
   if (packet->dest_l3 == ALL_NEIGHBORS) {
      FWD_Callback_Data *cb_data = smartalloc(sizeof(FWD_Callback_Data), "fishnode.c", __LINE__, 0);
      cb_data->l3frame = l3frame;
      cb_data->len = len;
      fish_fwd.iterate_entries(l3_broadcast_forward_cb, cb_data, FISH_FWD_TYPE_BROADCAST);
      smartfree(cb_data, "fishnode.c", __LINE__);
      return 0;
   }


   // Handle regular l3 packet
   fnaddr_t next_hop = fish_fwd.longest_prefix_match(packet->dest_l3);

   if (next_hop == 0) {
      // Generate FCMP error & Drop frame
      fish_fcmp.send_fcmp_response(l3frame, len, NETWORK_UNREACHABLE);
      return 0;   
   }
   
   // FiB has this node. forward to next hop addr
   fish_l2.fish_l2_send(l3frame, next_hop, len, 1);   // L3 Protocol
   
   return 0;
}

// Callback to broadcast DV advertisement
void my_timed_event(void*)
{
   int max_dv_payload = MTU - 17;   // MTU - L2 Header

   uint16_t max_ads = (max_dv_payload - sizeof(DV_Ad_Header)) / sizeof(DV_Ad_Route);
   int packet_len = sizeof(DV_Ad_Header) + (max_ads * sizeof(DV_Ad_Route));


   uint8_t *dv_packet = smartalloc(packet_len, "fishnode.c", __LINE__, 0);
   DV_Ad_Header *header = (DV_Ad_Header *)dv_packet;
   header->l3_addr = fish_getaddress();
   header->num_ads = 0;

   // Add to the dv packet with as many entries from FiB as we can fit 
   DV_Callback_Data dv_cb_data;
   dv_cb_data.packet = dv_packet;
   dv_cb_data.max_ads = max_ads;
   dv_cb_data.ads_added = 0;
   fish_fwd.iterate_entries(dv_ad_cb, &dv_cb_data, FISH_FWD_TYPE_DV);
   
   // After it returns, we should have the num ads added
   header->num_ads = htons(dv_cb_data.ads_added);
   int actual_len = sizeof(DV_Ad_Header) + (dv_cb_data.ads_added * sizeof(DV_Ad_Route));

   fish_l2.fish_l2_send(dv_packet, ALL_NEIGHBORS, actual_len, DV_ROUTING_PROTOCOL);
   smartfree(dv_packet, "fishnode.c", __LINE__);

   // Repeat every 30 seconds
   fish_scheduleevent(30000, my_timed_event, NULL);
}

/* 
   This adds the current entry from the FiB to the DV ad packet in the 
   route section. It follows the structure of the "fwtable_iterator_cb"
*/
int dv_ad_cb(void *callback_data, fnaddr_t dest, 
            int prefix_len, fnaddr_t next_hop, int metric, void *entry_data) {
   DV_Callback_Data *dv_build_context = callback_data;

   // Even if there are more entries in the FiB, we won't send them all since the MTU 
   if (dv_build_context->ads_added >= dv_build_context->max_ads) {
      return 0;
   }

   // Ptr to start of the routes in the DV Packet
   DV_Ad_Route *routes = (DV_Ad_Route *)(dv_build_context->packet + sizeof(DV_Ad_Header));

   // Gets the index for the route we are adding from FiB to this DV Packet
   DV_Ad_Route *current_route = &routes[dv_build_context->ads_added];
   current_route->dest = dest;
   current_route->metric = htonl(metric);

   
   uint32_t host_mask = 0;
   if (prefix_len != 0) {
      host_mask = 0xFFFFFFFFu << (32 - prefix_len);
   }
   current_route->netmask = htonl(host_mask);

   dv_build_context->ads_added++;

   (void) next_hop;
   (void) entry_data;
   return 0; 
}

// Full functionality
void* my_add_fwtable_entry(fnaddr_t dst, int prefix_length, fnaddr_t next_hop,
                   int metric, char type, void *user_data)
{
   if (prefix_length < 0 || prefix_length > 32) {
      return NULL;   // Invalid prefix
   }

   if (metric < 0) {
      return NULL;   // Invalid metric
   }
   
   FiB_Entry *new_entry = smartalloc(sizeof(FiB_Entry), "fishnode.c", __LINE__, 0);
   new_entry->dst = dst;
   new_entry->prefix_length = prefix_length;
   new_entry->next_hop = next_hop;
   new_entry->metric = metric;
   new_entry->type = type;
   new_entry->user_data = user_data;
   new_entry->next_entry = NULL;

   // Group routes with same destination. Otherwise, append at end
   if (fib_table_head == NULL) {
      fib_table_head = new_entry;
      return new_entry;
   }
   
   FiB_Entry *current_entry = fib_table_head;
   while (current_entry != NULL) {
      // Found same destination, insert here
      if (current_entry->dst == dst) {
         FiB_Entry *tmp = current_entry->next_entry;
         current_entry->next_entry = new_entry;
         new_entry->next_entry = tmp;
         return new_entry;
      }

      if (current_entry->next_entry == NULL) {
         current_entry->next_entry = new_entry;
         return new_entry;   // End of list, place the new entry here
      }

      current_entry = current_entry->next_entry;
   }
   
   // Shouldn't get here
   return new_entry;
}

void* my_remove_fwtable_entry(void *route_key)
{
   FiB_Entry *route_entry = route_key;
   if (route_entry == NULL) {return NULL;}
   if (fib_table_head == NULL) {return NULL;}
   
   if (fib_table_head == route_entry) {
      fib_table_head = route_entry->next_entry;
      
      void *user_data = route_entry->user_data;
      smartfree(route_entry, "fishnode.c", __LINE__);
      return user_data;
   }
   
   FiB_Entry *current_entry = fib_table_head;
   while (current_entry != NULL) {
      if (current_entry->next_entry == route_entry) {
         FiB_Entry *tmp = route_entry->next_entry;
         current_entry->next_entry = tmp;
         
         void *user_data = route_entry->user_data;
         smartfree(route_entry, "fishnode.c", __LINE__);
         return user_data;
      }

      current_entry = current_entry->next_entry;
   }
   
   return NULL;   // Shoudln't get here
}

int my_update_fwtable_metric(void *route_key, int new_metric)
{
   FiB_Entry *entry = route_key;
   if (entry != NULL) {
      entry->metric = new_metric;
      return 1;   // Returns True for success
   }
   
   return 0;
}

int is_entry_cidr_match(FiB_Entry *entry, fnaddr_t addr) {
   // Check CIDR Mask
   fnaddr_t host_entryDst = ntohl(entry->dst);
   fnaddr_t host_addr = ntohl(addr);

   uint32_t prefix_len = entry->prefix_length;
   fnaddr_t cidr_mask = 0;

   if (prefix_len > 32) {return 0;}
   for (uint8_t i = 0; i < prefix_len; i++) {
      cidr_mask |= (1U << (31-i));
   }

   return ((host_addr & cidr_mask) == (host_entryDst & cidr_mask));
}


fnaddr_t my_longest_prefix_match(fnaddr_t addr)
{
   // All same destinations are grouped in the add entry function
   // Just have to find the destination and keep interating until we have checked all of the prefixes
   

   // If FiB is empty, this function returns 0
   if (fib_table_head == NULL) {
      return 0;
   }

   FiB_Entry *match = NULL;
   uint32_t best_prefix = 0;
   FiB_Entry *current_entry = fib_table_head;
   while (current_entry->next_entry != NULL) {
      if (is_entry_cidr_match(current_entry, addr)) {
         if (match == NULL || current_entry->prefix_length > best_prefix) {
            match = current_entry;
            best_prefix = current_entry->prefix_length;
         } 
         else if (current_entry->prefix_length == best_prefix && 
                  current_entry->metric < match->metric) {
            match = current_entry;
            best_prefix = current_entry->prefix_length;
         }   
      }


      current_entry = current_entry->next_entry;
   }

   if (is_entry_cidr_match(current_entry, addr)) {
      if (match == NULL || current_entry->prefix_length > best_prefix) {
         match = current_entry;
         best_prefix = current_entry->prefix_length;
      } 
      else if (current_entry->prefix_length == best_prefix && 
               current_entry->metric < match->metric) {
            match = current_entry;
            best_prefix = current_entry->prefix_length;
      } 
   }
   
   if (match == NULL) {
      return 0;
   }
   return match->next_hop;
}
#endif

int main(int argc, char **argv)
{
	struct sigaction sa;
   int arg_offset = 1;

   /* Verify and parse the command line parameters */
	if (argc != 2 && argc != 3 && argc != 4)
	{
		printf("Usage: %s [-noprompt] <fishhead address> [<fn address>]\n", argv[0]);
		return 1;
	}

   if (0 == strcasecmp(argv[arg_offset], "-noprompt")) {
      noprompt = 1;
      arg_offset++;
   }

   /* Install the signal handler */
	sa.sa_handler = sigint_handler;
	sigfillset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (-1 == sigaction(SIGINT, &sa, NULL))
	{
		perror("Couldn't set signal handler for SIGINT");
		return 2;
	}

   /* Set up debugging output */
#ifdef DEBUG
	fish_setdebuglevel(FISH_DEBUG_INTERNAL);
	// fish_setdebuglevel(FISH_DEBUG_ALL);
#else
	fish_setdebuglevel(FISH_DEBUG_NONE);
#endif
	fish_setdebugfile(stdout);

   /* Join the fishnet */
	if (argc-arg_offset == 1)
		fish_joinnetwork(argv[arg_offset]);
	else
		fish_joinnetwork_addr(argv[arg_offset], fn_aton(argv[arg_offset+1]));

   /* Install the command line parsing callback */
   fish_keybhook(keyboard_callback);
   if (!noprompt)
      printf("> ");
   fflush(stdout);

#ifdef L2_IMPL
   // Examples of overriding function pointers for program 2 base functionality
   fish_l2.fishnode_l2_receive = &my_fishnode_l2_receive;
   fish_l2.fish_l2_send = &my_fish_l2_send;
   fish_arp.arp_received = &my_arp_received;
   fish_arp.send_arp_request = &my_send_arp_request;
   // Full functionality functions
   fish_arp.add_arp_entry = &my_add_arp_entry;
   fish_arp.resolve_fnaddr = &my_resolve_fnaddr;
#endif

#ifdef L3_IMPL
   fish_l3.fishnode_l3_receive = &my_fishnode_l3_receive;
   fish_l3.fish_l3_send = &my_fish_l3_send;
   fish_l3.fish_l3_forward = &my_fish_l3_forward;
   // Set up a callback to broadcast DV advertisement
   fish_scheduleevent(0, &my_timed_event, NULL);
   // Full functionality
   fish_fwd.add_fwtable_entry = &my_add_fwtable_entry;
   fish_fwd.remove_fwtable_entry = &my_remove_fwtable_entry;
   fish_fwd.update_fwtable_metric = &my_update_fwtable_metric;
   fish_fwd.longest_prefix_match = &my_longest_prefix_match;
#endif

#if 1
   /* Enable the built-in neighbor protocol implementation.  This will discover
    * one-hop routes in your fishnet.  The link-state routing protocol requires
    * the neighbor protocol to be working, whereas it is redundant with DV.
    * Running them both doesn't break the fishnode, but will cause extra routing
    * overhead */
   fish_enable_neighbor_builtin( 0
         | NEIGHBOR_USE_LIBFISH_NEIGHBOR_DOWN
      );
#endif

   /* Enable the link-state routing protocol.  This requires the neighbor
    * protocol to be enabled. */
   // fish_enable_lsarouting_builtin(0);

#if 0
   /* Full-featured DV routing.  I suggest NOT using this until you have some
    * reasonable expectation that your code works.  This generates a lot of
    * routing traffic in fishnet */

   fish_enable_dvrouting_builtin( 0
         | DVROUTING_WITHDRAW_ROUTES
         | DVROUTING_TRIGGERED_UPDATES
         | RVROUTING_USE_LIBFISH_NEIGHBOR_DOWN
         | DVROUTING_SPLIT_HOR_POISON_REV
         | DVROUTING_KEEP_ROUTE_HISTORY
    );
#endif

   /* Execute the libfish event loop */
	fish_main();

   /* Clean up and exit */
   if (!noprompt)
      printf("\n");
   fish_keybhook(NULL);

	printf("Fishnode exiting cleanly.\n");

   fishnet_cleanup();

   
   // Cleanup Broadcast History list
   if (broadcast_history_head != NULL) {
      Broadcast_History_Entry *current_entry = broadcast_history_head->next_entry;
      smartfree(broadcast_history_head, "fishnode.c", __LINE__);
      broadcast_history_head = NULL;

      while(current_entry != NULL) {
         Broadcast_History_Entry *next_entry = current_entry->next_entry;
         smartfree(current_entry, "fishnode.c", __LINE__);
         current_entry = next_entry;
      }
   }


   // Cleanup FiB
   if (fib_table_head != NULL) {
      FiB_Entry *current_entry = fib_table_head->next_entry;
      smartfree(fib_table_head, "fishnode.c", __LINE__);
      fib_table_head = NULL;

      while(current_entry != NULL) {
         FiB_Entry *next_entry = current_entry->next_entry;
         smartfree(current_entry, "fishnode.c", __LINE__);
         current_entry = next_entry;
      }
   }


	return 0;
}
