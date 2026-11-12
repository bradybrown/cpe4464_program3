#ifndef __FISHNODE_H
#define __FISHNODE_H

#include <stdint.h>
#include "fish.h"

#define NEIGHBOR_PROTOCOL   3
#define DV_ROUTING_PROTOCOL 4

#define TTL_EXCEEDED        1
#define NETWORK_UNREACHABLE 2
#define HOST_UNREACHABLE    3

#define FIB_ENTRY_TIMEOUT (180 * 1000)

typedef struct __attribute__((packed)) L2_Header {
    fn_l2addr_t dest_l2;
    fn_l2addr_t src_l2;
    uint16_t checksum;
    uint16_t length;
    uint8_t l3_protocol;
} L2_Header; 


typedef struct __attribute__((packed)) L3_Header {
    uint8_t ttl;
    uint8_t l4_protocol;
    uint8_t packet_id[4];
    fnaddr_t src_l3;
    fnaddr_t dest_l3;
} L3_Header; 


typedef struct Broadcast_History_Entry {
    fnaddr_t src_l3;
    uint8_t packet_id[4];
    struct Broadcast_History_Entry* next_entry;
} Broadcast_History_Entry;

/*  
    Functionally this struct is a RiB as it will contain all entries, 
    regardless if they are the best or not. 
*/
typedef struct FiB_Entry {
    fnaddr_t dst;
    uint32_t prefix_length;
    fnaddr_t next_hop;
    uint32_t metric;
    char type;
    void *user_data;

    event timeout_handle;
    
    struct FiB_Entry *next_entry;
} FiB_Entry; 


typedef struct FWD_Callback_Data{
   void *l3frame;
   int len;
} FWD_Callback_Data;


typedef struct __attribute__((packed)) DV_Ad_Header {
    fnaddr_t l3_addr;
    uint16_t num_ads;
} DV_Ad_Header;

typedef struct __attribute__((packed)) DV_Ad_Route {
    fnaddr_t dest;
    fnaddr_t netmask;
    uint32_t metric;
} DV_Ad_Route;

typedef struct DV_Callback_Data {
    uint8_t *packet;
    uint16_t max_ads;
    uint16_t ads_added;
} DV_Callback_Data;



int check_bcast_hist(fnaddr_t src_l3, uint8_t *packet_id);
void add_hist_entry(fnaddr_t src_l3, uint8_t *packet_id);

// Full Functionality
int is_entry_cidr_match(FiB_Entry *entry, fnaddr_t addr);   // Used for forwarding logic
int dv_receive(void *l3frame, int len);     // Helper function for l3 receive
FiB_Entry * check_fib(fnaddr_t dst);        // Checks if the address is in the fib
int netmask_to_slash(uint32_t netmask);     

FiB_Entry *find_dv_candidate(fnaddr_t dest, uint32_t prefix_len, fnaddr_t next_hop);    // Gets the FiB entry ptr based on DV and l3 addr
void dv_route_expired(void *arg);   // Timeout handler function
int dv_ad_cb(void *callback_data, fnaddr_t dest, int prefix_len, 
            fnaddr_t next_hop, int metric, void *entry_data);
void my_iterate_entries(fwtable_iterator_cb callback, void *callback_param,
            char type);

#endif
