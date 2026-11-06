#include "fish.h"
#include "fishnode.h"
#include "smartalloc.h"

#include <assert.h>
#include <signal.h>
#include <string.h>

#define DEBUG

#define L3_IMPL

Broadcast_History_Entry *broadcast_history_head = NULL;


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


int my_fishnode_l3_receive(void *l3frame, int len, uint8_t protocol)
{
   // Drop packet with invalid packet length
   if (len < sizeof(L3_Header)) {return 0;}
      
   L3_Header *packet = l3frame;
   
   if (packet->ttl > MAX_TTL) {
      return 0;   // Drop this invalid packet. TTL is too big
   }

   int l3frame_data_size = len - sizeof(L3_Header);
   uint8_t *l4frame = ((uint8_t *)l3frame) + sizeof(L3_Header);

   if (packet->dest_l3 == fish_getaddress()) {
      // Our node matches destination of packet. Send to l4
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
   Callback_Data *cb_data = callback_data;
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
      Callback_Data *cb_data = smartalloc(sizeof(Callback_Data), "fishnode.c", __LINE__, 0);
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
}

// Full functionality
void* my_add_fwtable_entry(fnaddr_t dst, int prefix_length, fnaddr_t next_hop,
                   int metric, char type, void *user_data)
{
   return NULL;
}

void* my_remove_fwtable_entry(void *route_key)
{
   return NULL;
}

int my_update_fwtable_metric(void *route_key, int new_metric)
{
   return 0;
}

fnaddr_t my_longest_prefix_match(fnaddr_t addr)
{
   return 0;
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
   // fish_scheduleevent(0, &my_timed_event, NULL);
   // Full functionality
   // fish_fwd.add_fwtable_entry = &my_add_fwtable_entry;
   // fish_fwd.remove_fwtable_entry = &my_remove_fwtable_entry;
   // fish_fwd.update_fwtable_metric = &my_update_fwtable_metric;
   // fish_fwd.longest_prefix_match = &my_longest_prefix_match;
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

#if 1
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

   // Cleanup your data structures here
   
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


   // Cleanup RiB



   // Cleanup FiB





	return 0;
}
