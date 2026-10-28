/*
 * Include this header file to gain access to the fishnet API.
 * Start by looking at the API section.
 */

#ifndef __FISH_H__
#define __FISH_H__

/* Some standard includes here so you don't need to do them */
#include <stdlib.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <stdio.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>

#if (__SIZEOF_POINTER__ == __SIZEOF_INT__)
#define IPTR_FMT "d"
#define UIPTR_FMT "u"
#else
#if (__SIZEOF_POINTER__ == __SIZEOF_LONG_LONG__)
#define IPTR_FMT "qd"
#define UIPTR_FMT "qu"
#else
#error Cant support the pointer size
#endif
#endif


/** \ingroup L3
  \brief The type definition for a fishnet L3 address
  */
typedef uint32_t fnaddr_t;

/** \ingroup L2
  \brief The type definition for a fishnet L2 address.

  This structure supports direct assignment (e.g., a = b), but not equality.
  Use the two provided macros to test for equality and validity.
*/
typedef struct {
   uint8_t l2addr[6];
} fn_l2addr_t;

/** \ingroup L2
    \brief  A macro that tests fishnet layer 2 addresses for equality
    \arg \c x , \c y Are both fn_l2addr_t structures (not pointers).
    \returns The macro evaluates to true if the addresses are the same and false
    otherwise.
    */
#define FNL2_EQ(x,y) (0 == memcmp(&(x).l2addr, &(y).l2addr, sizeof((x).l2addr)))

/** \ingroup L2
    \brief A macro that tests the validity of a fishnet layer 2 address
    \arg \c x A fn_l2addr_t structure (not a pointer) to test for validity
    \returns true if the address is valid, false otherwise.
    */
#define FNL2_VALID(x) ((x).l2addr[0] != 0 || (x).l2addr[1] != 0 || \
      (x).l2addr[2] != 0 || (x).l2addr[3] != 0 || (x).l2addr[4] != 0 || \
      (x).l2addr[5] != 0)

/**
 * \ingroup L3
 * \brief This Fishnet address allows you use fish_send() to send a frame to all
 * immediate neighbors.
 */
#define ALL_NEIGHBORS htonl(0xFFFFFFFF)

/** \ingroup L2
    \brief A global that holds the L2 broadcast address
    */
extern fn_l2addr_t ALL_L2_NEIGHBORS;

/** \ingroup L1
    \brief Packet size limits. The total packet length must be less than the
 * maximum transmission unit (MTU). */
#define MTU 1500

/** \ingroup L3
    \brief The time-to-live or TTL limits the number of hops a packet can
 * take, in order to prevent loops. */
#define MAX_TTL 64


/** \addtogroup debug Debugging functions
    \brief Functions and constants that facilitate debugging

    libfish includes a level-based debugging facility.  While your fishnode is
    running it selects which level of debugging messages to see by calling the
    fish_setdebuglevel() function.  All messages that are at that level or lower
    numerically will be printed.
    */
//@{

/// No debugging messages
#define FISH_DEBUG_NONE          0

/// Print debug messages from the application(s) (L7)
#define FISH_DEBUG_APPLICATION   1
/// A debug level for your own use
#define FISH_DEBUG_USER1         2
/// Debug messages for transport level events (L4)
#define FISH_DEBUG_TRANSPORT     3
/// A debug level for your own use
#define FISH_DEBUG_USER2         4
/// Debug level that includes routing messages (L3)
#define FISH_DEBUG_ROUTING       5
/// Debug level that incudes topology changes from fishhead
#define FISH_DEBUG_TOPOLOGY      6
/// Debug level that includes everything
#define FISH_DEBUG_ALL           7 
/// Debug level that includes internal fishnet debugging messages
#define FISH_DEBUG_INTERNAL      8

//@}

/* support building assignments using C++, while the library is 
   built in C. */
#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * fishnet structures and datatypes
 *--------------------------------------------------------------------------*/

/** \brief keyboard_handler is the type of the
 * upcall registered with fish_keybhook().  Your keyboard input
 * handler will take one parameter: *line_of_text*, which does not
 * include the newline character ('\n'), but is null-terminated so
 * that functions like sprintf will work.
 */
typedef
void (*keyboard_handler) (char* line_of_text);


/*--------------------------------------------------------------------------
 * fishnet API calls
 *--------------------------------------------------------------------------*/

/*
 * NOTE:
 * Any fishnet API call may abort and dump core on failure (e.g., if it is
 * given bad arguments).  An error message will be printed, and you can use
 * gdb to help you figure out what happened.
 */

/** \brief fish_joinnetwork adds your node to the Fishnet network and is
 * generally the first function you will call.
 *
 * The host and port arguments describe the fishhead to contact, which
 * is synonymous with the network that you are joining.  If the
 * fishhead_location is not "", then a fishhead node is contacted for
 * obtaining the network topology. If the fishhead_location is "",
 * then the network topology is dictated by the physical environment
 * (this is typically used for wireless).
 *
 * You may optionally supply your requested node address.
 *
 * The call will either succeed, in which case you have joined a Fishnet,
 * or will fail, printing an error message and exiting.  There are a
 * variety of possible errors, such as not being able to contact the
 * fishhead or the address you wanted already being in use.
 *
 * Note that this function will exit the program on failure.
 */
extern void
fish_joinnetwork(const char* fishhead_location); // In the format "host:port"

/** \brief Like fish_joinnetwork(), except you can specify a specific L3
 * address.  The function will fails if the address doesn't exist.
 */
extern void
fish_joinnetwork_addr(const char* fishhead_location, // In the format "host:port"
		 fnaddr_t addr);	      // Your choosen address.  A value of zero instructs fishnet to choose a port for you.

/** \brief fish_getheadhost can be used to get the name of the fishhead host after
 * fish_joinnetwork has been called.
 */
extern const char* fish_getheadhost(void);

/** \brief fish_getheadport can be used to get the fishhead port after
 * fish_joinnetwork has been called.
 */
extern int fish_getheadport(void);

/** \brief fish_getaddress can be used to get the fishnet node address after
 * fish_joinnetwork has been called.
 */
extern fnaddr_t fish_getaddress(void);

/** \brief fish_getl2address can be used to get the fishnet l2 node address after
 * fish_joinnetwork has been called.
 */
extern fn_l2addr_t fish_getl2address(void);

/** \brief fishnet_cleanup frees all the heap allocated memory used by libfishnet.
 * This can be useful when debugging memory leaks as it reduces valgrind output.
 * Once this function is call the library will stop work, however.  So only
 * call it once at the end of the program.
 */
extern void fishnet_cleanup(void);

/*******************************************************************
  This section describes the fishnet layer interactions.  All functions
  that accept a frame pointer must not free() the frame when they are done.
  Other code in libfish.a takes are of freeing the frame.  If code makes
  a duplicate copy of the frame, it is the code's responsibility to ensure
  that copy is freed correctly.

  The following chart shows the interactions of fishnet layers 1-4, plus the
  function calls that move frames up and down the fishnet stack.  The parameters
  to each function are documented in comments around the function's declaration.


             ----------------------------------
             |                                |
             |                        >=======|====> fishnode_l4_receive()
             |                        |       |
             |            Layer 4     >=======|====> fish_sendto_l4_builtin()
             |                        |       |
             |                        >=======|====> fish_sendto_l4_apps()
             |                        |       |
             ----------------------------------
                             v        ^ fish_l4_receive()
                             |        |
                             v        ^-----------------
                             |                         ^
             fish_l3_send()  v                         |
             ----------------------------------        ^
             |               |                |        |
             |   fish_l3_forward()    >=======|====> fishnode_l3_receive()
             |               v        |       |
             |  Layer 3      |        >=======|====> fish_sendto_l2_builtin()
             |               v        |       |
             |               |        >=======|====> fish_sendto_l2_apps()
             |               v        |       |
             ----------------------------------
                             v        ^ fish_l3_receive()
                             |        |
                             v        ^----------------
                             |                        ^
             fish_l2_send()  v                        |
             ----------------------------------       ^
             |                                |       |
             |                        >=======|===> fishnode_l2_receive()
             |                        |       |
             |            Layer 2     >=======|===> fish_sendto_l2_builtin()
             |                        |       |
             |                        >=======|===> fish_sendto_l2_apps()
             |                        |       |
             ----------------------------------
                             v        ^ fish_l2_receive()
                             |        |
                             v        ^
                             |        |
             fish_l1_send()  v        ^ 
             ----------------------------------
             |                                |
             |            Layer 1             |
             |     (libfish.a / fishhead)     |
             ----------------------------------


   Each fishnode_l*_receive() function should only un-encapsulate and pass
   frames up the network stack that are destined for the fishnode (e.g., either
   a unicast, multicast, or broadcast packet for this node).
*/

/** \addtogroup L2 L2 Functionality 
  \brief Functions, data structures, and constants that relate to implementing
  fishnet layer 2 functionality in a fishnode.
  */
   
/*@{*/



/**
  \brief Functions for transmitting and receiving L2 frames.
 
    This structure contains function pointers that implement all layer 2
    transmit and receive operations.  Layer 2 frames have the layer 2 header and
    any layer 2 payload.  These are provided as function pointers to
    allow the fishnode to override the built-in implementations with its own
    implementation(s).

    There is a single instance of this structure in a fishnode.  It is a global
    variable named fish_l2.  You can access the fields directly in any portion
    of your code (e.g., fish_l2.fishnode_l2_receive = NULL) to change the value
    of the function pointers or to call them.  Do not declare an instance of
    this structure yourself.

    **/

extern struct FishnetL2Funcs {
   /** \ingroup L2
       \brief Receive a new layer 2 frame from the layer 1 code.
       \arg \c l2frame A pointer to the received L2 frame.  The frame must not
       be modified.  The caller will free the memory for the frame as necessary.
       \return The return value is ignored
     
     This function is the main function responsible for accepting new L2
     packets from the L1 code.  It is responsible for
     calling fish_sendto_l2_apps(), fish_sendto_l2_builtin(), and
     fishnode_l2_receive(), in that order. It does nothing more than call those
     3 functions.  All remaining L2 logic is left for
     fish_l2::fishnode_l2_receive.
    */
   int (*fish_l2_receive)(void *l2frame);

   /** \ingroup L2
       \brief Receives a new layer 2 frame from the layer 1 code in libfish.
          Decapsulates the frame and passes it up the stack as needed.
       \arg \c l2frame A pointer to the received L2 frame.  The frame must not
       be modified.  The caller will free the memory for the frame as necessary.
       \return The return value is ignored

       This function is responsible for correctly directing valid packets to the
       higher network layers.  This requires
       following general steps:
       \li Dropping frames with invalid checksums
       \li Dropping frames that are not destined for this node (verifies the L2
       address.  doesn't consider the L3 address).
       \li Decapsulating frame and passing up the stack
       (fish_l3::fish_l3_receive).

       This is also the function that calls your implementation of fishnet L2
       protocols, such as ARP.
    */
   int (*fishnode_l2_receive)(void *l2frame);

   /** \ingroup L2
     \brief Receives a new L3 frame to be sent over the network.
       \arg \c l3frame A pointer to the L3 frame.  The original frame memory
       must not be modified.  The caller is responsible for freeing any memory
       after this function has completed.
      \arg \c next_hop The L3 address of the neighbor this frame should be sent
      to.
      \arg \c len The length of the L3 frame
      \arg \c l2_proto The layer 2 protocol of the packet being sent
       \return The return value is ignored
      
     This function takes to following steps to transmit \c l3frame:
     \li Adds an L2 header to the frame
     \li Uses the ARP cache to resolve the L3 address to L2 (see \ref ARP)
     \li Calls fish_l1_send() to transmit the frame
    */
   int (*fish_l2_send)(void *l3frame, fnaddr_t next_hop, int len, uint8_t l2_proto);

} fish_l2;

/** \brief Sends a layer 2 frame to all built-in protocol implementations that
 * are interested in it.
    \arg \c l2frame A pointer to the L2 frame (has L2+ headers)
    \arg \c mine A boolean flag that indicates if the packet is "for" this node.
    
    Sends a L2 frame (including full L2 header) to the libfish built-in protocol
   implementations.  This must be used in the order depicted in the flow chart
   above. The "mine" parameter enables both promiscuous and non-promiscious
   builtin functions.  This function returns true if the frame was "consumed"
   and should not be processed further.  If the function returns false then
   further processing is needed. */
extern int fish_sendto_l2_builtin(void *l2frame, int mine);

/** \brief Sends a L2 frame to applications that are attached to this node.
    \arg \c l2frame A pointer to the L2 frame.  The contents will not be
    modified and the memory will not be freed.
    \arg \c mine A boolean flag that indicates if the L2 packet should be
    processed by this node.

  Sends a L2 frame (including full L2 header) to the application(s) attached to
  the fishnode.  This enables applications like fishdump to receive
  raw fishnet frames.
  */
extern void fish_sendto_l2_apps(void *l2frame, int mine);

/*@}*/


/** \addtogroup L3 L3 Functionality 
  \brief Functions, data structures, and constants that relate to implementing
  fishnet layer 3 functionality in a fishnode.
  */
   
/*@{*/



/** \brief Functions for sending and receiving layer 3 frames.

    This structure contains function pointers that implement all layer 3
    transmit and receive operations.  Layer 3 frames have the layer 3 header and
    any layer 3 payload.  These are provided as function pointers to
    allow the fishnode to override the built-in implementations with its own
    implementation(s).

    There is a single instance of this structure in a fishnode.  It is a global
    variable named fish_l3.  You can access the fields directly in any portion
    of your code (e.g., fish_l3.fishnode_l3_receive = NULL) to change the value
    of the function pointers or to call them.  Do not declare an instance of
    this structure yourself.
   */

extern struct FishnetL3Funcs {
   /** \ingroup L3
       \brief Receive a new layer 3 frame from the layer 2 code.
       \arg \c l3frame A pointer to the received L3 frame.  The frame must not
       be modified.  The caller will free the memory for the frame as necessary.
       \arg \c The length of the layer 3 frame, in host byte order
       \arg \c protocol The protocol of the L3 frame
       \return The return value is ignored
     
     This function is the main function responsible for accepting new L3
     packets from the L2 code.  It is responsible for
     calling fish_sendto_l3_apps(), fish_sendto_l3_builtin(), and
     fishnode_l3_receive(), in that order. It does nothing more than call those
     3 functions.
    */
   int (*fish_l3_receive)(void *l3frame, int len, uint8_t protocol);

   /** \ingroup L3
       \brief Receives a new layer 3 frame from FishnetL3Funcs::fish_l3_receive.
          Decapsulates the frame and forwards it or passes it the L4 as needed.
       \arg \c l3frame A pointer to the received L3 frame.  The only part of
          the frame that can be modified is the TTL header field.  The caller
          will free the memory for the frame as necessary.
       \arg \c len The length of the layer 3 frame, in host byte order
       \arg \c protocol The protocol of the L3 frame
       \return The return value is ignored

       This function is responsible for correctly directing packets to the
       higher network layers or to the forwarding engine.  This requires
       following general steps:

       \li If the L3 destination is the node's L3 address, remove the L3 header
       and pass the frame to the L4 code.
       \li If the L3 destination is the broadcast address AND the frame was
       received by this node previously (use the combination of src L3 address
       and packet ID to determine this), drop the frame without sending an FCMP
       message.
       \li Otherwise, if the L3 destination is the broadcast address, the frame
       must be both passed up the network stack and forwarded back out over the
       fishnet with a decremented TTL.
       \li If the L3 addresses isn't the broadcast address and the L3 address
       isn't the node's address, the TTL is decremented and the frame is
       forwarded back over the fishnet.

       This function can not modify the l3frame passed in.  If a modification
       is necessary a deep copy of the frame must be made first.  This is also
       the function that calls your implementation of fishnet L3
       protocols, such as DV routing.
      */


   int (*fishnode_l3_receive)(void *l3frame, int len, uint8_t protocol);

   /** \ingroup L3
       \brief Receives a new L4 frame to be sent over the network.
       \arg \c l4frame A pointer to the L4 frame.  The original frame memory
       must not be modified.  The caller is responsible for freeing any memory
       after this function has completed.
      \arg \c len The length of the L4 frame
      \arg \c dst_addr The L3 address of the final destination for this frame.
      \arg \c proto The fishnet protocol number of the L4 frame
      \arg \c ttl The TTL for the frame, or 0 to use the maximum TTL.  If the TTL is greater than the maximum TTL, set it to the maximum TTL.
       \return The return value is ignored
      
      This function is responsible for encapsulating a L4 frame in a L3 header
      and then forwarding the packet through the fishnet.  Forwarding is
      accomplished by calling the fish_l3_forward() function.
    */
   int (*fish_l3_send)(void *l4frame, int len, fnaddr_t dst_addr,
         uint8_t proto, uint8_t ttl);

   /** \ingroup L3
       \brief Takes an already-encapsulated L3 frame, looks up the destination
        in the forwarding table, and passes the frame off to L2.
        \arg \c l3frame A pointer to the L3 frame.  Note that this is different
        than fish_l3_send which takes a L4 frame.  The original frame memory
        must not be modified.  The caller is responsible for freeing any
        memory after this function has completed.
        \arg \c len The length of the L3 frame.
       \return The return value is ignored
     
     fish_l3_forward is given a L3 encapsulated frame and is responsible for
     delivering it. This requires the following steps:
       \li If the TTL is 0 and the destination is not local, drop the packet and
       generate the correct FCMP error message.
       \li Lookup the L3 destination in the forwarding table.
       \li If there is no route to the destination, drop the frame and generate
       the correct FCMP error.
       \li Use fish_l2_send to send the frame to the next-hop neighbor indicated
       by the forwarding table.


     This function may be called from a number of other places in the code,
     including fish_l3_send and fishnode_l3_receive.
     */
   int (*fish_l3_forward)(void *l3frame, int len);
} fish_l3;

/** \brief  Sends a L3 frame (including full L3 header) to the libfish
         built-in protocol implementations.
    \arg \c l3frame A pointer to the L3 frame.  The contents will not be
    modified and the memory will not be freed.
    \arg \c len The length of the L3 frame
    \arg \c mine A boolean flag that indicates if the L3 packet should be
    processed by this node.
    \return true if the built-in implementations "consumed" the frame and false
    otherwise.  A consumed frame must not be sent up the protocol stack.
    
    This functions attempts to match the L3 frame to one of the built-in
    protocol implementations.  If a match is found, the implementation is given
    the frame and this function returns true.  Otherwise this function returns
    false.
    */
extern int fish_sendto_l3_builtin(void *l3frame, int len, int mine);

/** \brief Sends a L3 frame to applications that are attached to this node.
    \arg \c l3frame A pointer to the L3 frame.  The contents will not be
    modified and the memory will not be freed.
    \arg \c len The length of the L3 frame
    \arg \c mine A boolean flag that indicates if the L3 packet should be
    processed by this node.

  Sends a L3 frame (including full L3 header) to the application(s) attached to
  the fishnode.  This enables applications like ping and traceroute to receive
  packets.
  */
extern void fish_sendto_l3_apps(void *l3frame, int len, int mine);

//@}

/**
    \brief Layer 4 frame handling functions.
    
    The functions in the fish_l4
    structure can be overridden by providing pointers to your own
    implementation.  The remaining functions can not be overridden.
  */

extern struct FishnetL4Funcs {
   int (*fish_l4_receive)(void *l4frame,     /* Frame that is being received,
                                                including the L4+ headers. */
                          int len,           /* length of the L4 frame */
                          uint8_t proto,     /* The fishnet protocol number of
                                                the L4 protocol. */
                          fnaddr_t src);     /* The source L3 fishnet address of
                                                the node that sent the packet. 
                                              */
   int (*fishnode_l4_receive)(void *l4frame, /* Frame that is being received,
                                                including the L4+ headers. */
                          int len,           /* length of the L4 frame */
                          uint8_t proto,     /* The fishnet protocol number of
                                                the L4 protocol. */
                          fnaddr_t src);     /* The source L3 fishnet address of
                                                the node that sent the packet. 
                                              */
                                             /* NOTE: This function may be NULL
                                              * if the fishnode does not need to
                                              * process L4 frames */
} fish_l4;

/** \brief Sends a L4 frame (including full L4 header) to the libfish
  built-in protocol implementations.
   
   This must be used in the order depicted in the flow chart
   above. This function returns true if the frame was "consumed"
   and should not be processed further.  If the function returns false then
   further processing is needed. */
extern int fish_sendto_l4_builtin(void *l4frame, /* Frame that is being
                                                     received,
                                                including the L4+ headers. */
                          int len,           /* length of the L4 frame */
                          uint8_t proto,     /* The fishnet protocol number of
                                                the L4 protocol. */
                          fnaddr_t src);     /* The source L3 fishnet address of
                                                the node that sent the packet. 
                                              */

/** \brief Sends a L4 frame (including full L4 header) to the application(s)
  attached to the fishnode.
  
  This must be used in the order depicted in the flow chart
   above. */
extern void fish_sendto_l4_apps(void *l4frame, /* Frame that is being
                                                     received,
                                                including the L4+ headers. */
                          int len,           /* length of the L4 frame */
                          uint8_t proto,     /* The fishnet protocol number of
                                                the L4 protocol. */
                          fnaddr_t src);     /* The source L3 fishnet address of
                                                the node that sent the packet. 
                                              */

/** \addtogroup L1 L1 Functions and constants
   \brief Functions and constants for the libfish L1 implementation.
   */

/** \ingroup L1
    \brief fish_l1_send sends a L2+ frame through the fishnet overlay.
    \arg \c frame A pointer to the L2 frame to send.  It will not be modified or
    freed by this function.
    \return 0 on success and -1 on failure

    This function will not accept invalid frame, including frames whose length
    is greater than the MTU.  The fishnode must be in the main event loop (see
    \ref events) before this function can be called.
 */
extern int fish_l1_send(void* frame);

/** \brief fish_keybhook registers a function that you write to be called at a
 * later time when a line of keyboard input is available.
 *
 * Like recvhook, it is an upcall that gets called from inside
 * fish_main.  It can do anything it likes with the input, but must
 * return before further input can be received.
 *
 * Note that the memory in which the input is stored belongs to libfish;
 * you MUST NOT free it.
 */
extern void fish_keybhook(keyboard_handler keybhandler);

/** \brief fish_main is the main fishnet loop.  Calling fish_main() is the last
 * thing you should do in your main() function.
 *
 * Your program will typically join a Fishnet, set up receive and keyboard
 * handlers, plus any timers, and then sit in fish_main() for the rest of
 * the program.  Frame reception, keyboard input and timer expiry are called
 * from within fish_main() and return control to fish_main().
 *
 * fish_main will automatically exit the program when the user types
 * "exit".
 */
extern void fish_main(void);

/** \brief When fish_main_exit is called fish_main will gracefully finish its current
 * operation and return from fish_main.  Use of fish_main_exit allows you to
 * cleanly shut down your fish node.  Note that fish_main_exit does *not* cause
 * fish_main to return immediately.  You fish node code must return control
 * to libfish before fish_main will return.
 */
extern void fish_main_exit(void);

/** \ingroup L3
  \brief Returns the next unique packet ID to be included in the L3 header in
  host byte order */
extern uint32_t fish_next_pktid(void);

/** \brief Callback function pointer declaration for a scheduled event.  The
 * callback takes a single void* parameter that is set when registering the
 * timed event.
 */
typedef void (*event_handler_t)(void *);

/** \brief Type declaration for the event handle returned when registering a
 * timed event.  This can be saved and then used to cancel an event prior to it
 * triggering.
 **/
typedef intptr_t event;

/** \brief fish_scheduleevent sets up a timer function to be called at some time in
 * the future.
 *
 * You write the timer function, and supply an argument value that it will
 * be invoked with. fish_scheduleevent returns a handle to the timer event,
 * which can be used to cancel it before it expires.  The return value is
 * an integer representing the event, which will always be greater than 0.
 *
 * Note that attempting to schedule two events with the same event_handler
 * and argument will result in an error.
 * */

extern event                           // returns a handle for the event
fish_scheduleevent(
        int msec_delay,                // how far in the future to call
	                               // the event handler (in milliseconds)
        void (*event_handler)(void *), // the timer function that you wrote
        void *event_handler_argument); // any one piece of data that you
                                       // want passed to the timer
                                       // function

/** \brief fish_cancelevent cancels a timer before it expires.  Its argument is
 * the handle returned by fish_scheduleevent.
 */
extern void *fish_cancelevent(event event_handle);

/** \addtogroup debug
  */
//@{

/** \brief Sets the fishnet debugging level, using the FISH_DEBUG_* constants.
    \arg \c level The desired debugging level

   Sets the debugging level to the specified value, causing all messages at the
   same level or numerically lower to be printed.  The debug level can be changed anywhere in the program.
 */
extern void fish_setdebuglevel(const int level);

/**
    \brief gets the current fishnet debugging level.
    \returns The current debugging level
 */
extern int fish_getdebuglevel(void);

/** \brief Generate your own debugging message
    \arg \c level The level assigned to your debugging message
    \arg remaining arguments define the message in printf-style 

    This function verifies the current debugging level is >= the level assigned
    to the message before it prints the message.  In addition to printing the
    message this function will also output a timestamp for the message.

 */
extern void fish_debug(int level, const char* format, ...)
     __attribute__ ((format (printf, 2, 3)));

/** \brief Generate your own debugging messages without the timestamp

   This function is identical to fish_debug(), except it does not automatically
   add the timestamp to the message.
   */
extern void fish_debugsupp(int level, const char* format, ...)
     __attribute__ ((format (printf, 2, 3)));


/** \brief Generates your own debugging message, one character at a time.
    \arg \c level The debugging level for the character
    \arg \c ch The character to print
 *
 * The level is one of the constants defined above (or one you define).
 * The message will be printed and the output stream flushed if
 * (level >= fish_getdebuglevel()).
 */
extern void fish_debugchar(int level, char ch);

/** \brief Print a debugging message plus the contents of a frame.
    \arg \c level The debug level for the message
    \arg \c msg A c-string containing the message you wish to preceed the frame
    dump
    \arg \c frame A pointer to the frame to dump.  The memory isn't changed or
    freed
    \arg \c layer The numerical value (e.g., 2, 3, 4) of the first header on the
    frame.  If the frame is a L3 frame, pass 3 for this parameter.
    \arg \c len The length of the frame.  This is ignored for L2 frames.
    \arg \c l3_protocol The protocol number (e.g., 2 for ARP) for the L4 header.  Only necessary when passing an L4 point in via the frame parameter.  Otherwise pass 0.
    \arg \c l4_protocol The protocol number (e.g., 7 for DV) for the L4 header.  Only necessary when passing an L4 point in via the frame parameter.  Otherwise pass 0.
 */
extern void fish_debugframe(int level, const char* msg, const void *frame, int layer, int len,  uint16_t l3_proto, uint8_t l4_protocol);

/** \brief Select the destination for debugging output
    \arg \c f A FILE* to write debugging output to.
    
    Causes all subsequent debugging output to be written to \c f.  Can be called
    at any point in your program.  The default destination for debugging
    messages is stderr.
    
 */
extern void fish_setdebugfile(FILE *f);

//@}

/** \ingroup L3
    \brief Converts a L3 address into a printable c-string.
    \arg \c addr The L3 address to convert
    \returns A pointer to the c-string that contains the L3 address
    
    Warning - This function places the string into a static buffer, so if you
    call this function twice it will overwrite the result from the first call.
    Don't use this twice in the same call to printf()!
 */
extern char *fn_ntoa(fnaddr_t addr);

/** \ingroup L2
    \brief Converts a L2 address into a printable c-string.
    \arg \c addr The L2 address to convert
    \returns A pointer to the c-string that contains the L2 address
    
    Warning - This function places the string into a static buffer, so if you
    call this function twice it will overwrite the result from the first call.
    Don't use this twice in the same call to printf()!
 */
extern char *fnl2_ntoa(fn_l2addr_t addr);

/** \ingroup L3
    \brief Converts a c-string L3 address into a fnaddr_t
    \arg \c addr a pointer to the c-string to convert
    \returns The L3 address stored in a fnaddr_t
 */
extern fnaddr_t fn_aton(const char *addr);

/** \brief fish_readhook and fish_remove_readhook are advanced functions for
 * integrating ordinary TCP sockets in a fishnet application.
 *
 * read_ready_handler is called when socket is ready to be read.  Usually,
 * this means there is data to be read, but sometimes the socket may have
 * been closed by the other end, meaning that read() will return zero bytes.
 *
 * remove_readhook should be called when the socket is closed,
 * otherwise you may see "bad file descriptor" errors.
 */
void fish_readhook(int sd,                          // the socket descriptor
                   void (*read_ready_handler)(int socket));  // the callback
void fish_remove_readhook(int sd);

/** \brief in_cksum computes the Internet checksum over the given data.  Its
 * implementation and usage is exactly the same as the checksum.c code
 * distributed with previous assignments.
 */
extern unsigned short in_cksum(const void *addr,int byte_len);


/** \brief Connect to a fishnode from another process.  The layer specifies where
   in the protocol stack the process is connected.  Valid values are 2-7.  A
   layer 2 process will send and receive frames that include the L2 header, 
   and so on.  Layers 5, 6, and 7 are identical.  addr and l2addr are out
   parameters that provide the process with the address(es) necessary
   to complete the headers in their part of the frame.
 */
extern int fishsocket_joinnetwork(const char *node, uint32_t layer,
      uint32_t promisc, fnaddr_t dest, uint8_t proto, fnaddr_t *addr,
      fn_l2addr_t *l2addr);

/** \addtogroup ARP ARP Functionality
    \brief Functions and types that implemented the fishnet ARP functionality
    */
//@{
/** \brief Function signature for the ARP resolution callback.
    \arg \c addr The L2 address corresponding to the L3 address being resolved.
    If the resolution failed this will be the invalid L2 address.  Test with the
    macro FNL2_VALID().
    \arg \c param The param value passed to
    fishnet_arp_functions::resolve_fnaddr.  It is passed unchanged into the
    ARP completion callback.
    */
typedef void (*arp_resolution_cb)(fn_l2addr_t addr, void *param);

/** \brief Functions for interacting with the ARP cache and resolving L3
       addresses into L2 addresses.

    This structure contains function pointers that implement all ARP related
    operations. These are provided as function pointers to
    allow the fishnode to override the built-in implementations with its own
    implementation(s).  These fields are initialized to point to the libfish
    built-in implementations of the functions.

    There is a single instance of this structure in a fishnode.  It is a global
    variable named fish_arp.  You can access the fields directly in any portion
    of your code (e.g., fish_arp.resolve_fnaddr = NULL) to change the value
    of the function pointers or to call them.  Do not declare an instance of
    this structure yourself.
 */

extern struct fishnet_arp_functions {
   /** \ingroup ARP
     \brief Add an entry to the ARP cache.
       \arg \c l2addr The L2 address of the new entry.
       \arg \c addr The L3 address of the new entry.
       \arg \c timeout The length of time the entry remains valid, in seconds.

       The entry defines a mapping between the supplied L3 and L2 addresses.  If
       an for the <L3, L2> pair already exists then the timeout value is reset
       to the value passed into the function.
    */
   void (*add_arp_entry)(fn_l2addr_t l2addr, fnaddr_t addr, int timeout);

   /** \ingroup ARP
     \brief This function resolves a L3 address into the corresponding L2
            address.
       \arg \c addr The L3 address to resolve.
       \arg \c cb The callback function to call once the resolution is complete.
       \arg \c param An opaque additional parameter to pass to the callback
       function when it gets called.  I strongly encourage you to pass the
       pointer to your L2 frame via this parameter.

       This function does not return the L2 address via a return value.
       Instead it calls the callback function \c cb once the mapping is known.
       This is necessary because it may require many seconds to realize a
       L3 address is not on the same L2 network.  You must supply your own
       \c cb function to call fishnet_arp_functions::resolve_fnaddr.  A
       NULL value is not sufficient.

       When \c cb is called, the value passed in via the \c param parameter
       is passed unmodified to \c cb.  You can use this to communicate as much
       information as necessary to the callback without resorting to global variables.
       fishnet_arp_functions::resolve_fnaddr will not modify \c param nor will
       it free \c param after the callback is called.  If you allocate \c param
       from the heap the callback is responsible for freeing it before the
       callback returns.  In the event an ARP lookup is unsuccessful the
       callback is called with an invalid L2 address (the FNL2_VALID() macro tests
       for an invalid L2 address).  This is important to permit the callback to
       free any heap-allocated memory associated with \c param.
     
      If the L3 to L2 address mapping is cached and hasn't expired, the callback
      function is called immediately with the mapping.  This occurs before
      fishnet_arp_functions::resolve_fnaddr returns.
      
      If the mapping is NOT present in the cache, an ARP lookup is initiated
      for the L3 address and the first ARP request is sent using the
      fishnet_arp_functions::send_arp_request function.  The callback is not
      called immediately.  Instead the callback wil be called once the
      corresponding ARP response has been received (typically 100 ms), or
      after all ARP retries have been attempted and there is still no
      answer (at least 10 seconds).  fishnet_arp_functions::resolve_fnaddr
      returns immediately in this case.

      During this potentially long interval it is necessary for the fishnode
      to continue processing packets, otherwise it won't receive the ARP
      response.  In order to process packets, the function that calls
      fishnet_arp_functions::resolve_fnaddr must return immediately after
      fishnet_arp_functions::resolve_fnaddr returns.  Therefore it is
      very important that there is no amount of code after the call to
      fishnet_arp_functions::resolve_fnaddr.  In particular, do NOT write code
      that assumes \c cb has been called before
      fishnet_arp_functions::resolve_fnaddr returns and do not loop waiting for
      \c cb to be called after calling fishnet_arp_functions::resolve_fnaddr.
    */
   void (*resolve_fnaddr)(fnaddr_t addr, arp_resolution_cb cb, void *param);

   /** \ingroup ARP
     \brief This function pointer gets called when an ARP frame arrives at the
        node for processing.
       \arg \c  l2frame A pointer to the L2 frame containing the ARP packet.
       
        The function is responsible for processing ARP packets of all ARP query
        types.  It is necessary to call
        fish_arp::add_arp_entry as part of processing an ARP response.
        There is a built-in L2 handler
        that calls this function for every ARP packet destined to this
        fishnode.  The built-in handler AUTOMATICALLY DISABLES ITSELF when you
        provide a pointer to your own arp_received implementation.  You must
        add code to fish_l2::fishnode_l2_receive to call this function if you
        override this function!
    */
   void (*arp_received)(void *l2frame);

   /** \ingroup ARP
     \brief This function creates and sends an ARP request for the given L3i
        address
        \arg \c l3addr The L3 address to send an ARP request for.

        This function is called as part of fish_arp::resolve_fnaddr when no
        entry is present in the ARP cache.  It must create and send an
        appropiate ARP request frame.
    */
   void (*send_arp_request)(fnaddr_t l3addr);
} fish_arp;
//@}

/** \addtogroup FWD Forwarding Table
    \brief Functions, data types, and constants for interacting with the
    fishnode forwarding table.
    */
//@{

/// The type constant for a directly connected route
#define FISH_FWD_TYPE_CONNECTED 'C'
/// The type constant for a loopback route
#define FISH_FWD_TYPE_LOOPBACK 'L'
/// The type constant for a broadcast route
#define FISH_FWD_TYPE_BROADCAST 'B'
/// The type constant for a distance-vector route
#define FISH_FWD_TYPE_DV 'D'
/// The type constant for a link-state route
#define FISH_FWD_TYPE_LS 'Z'
/// The type constant for routes learned through the neighbor protocol
#define FISH_FWD_TYPE_NEIGHBOR 'N'

/** \brief Function signature for the fwtable interation callback.
    \arg \c callback_data An opaque pointer passed in via the
    fishnet_fwtable_functions::iterate_entries function.
    \arg \c dest The final destination of this route.
    \arg \c prefix_length The prefix length for the route (e.g., 32 for a
       /32).
    \arg \c next_hop The next_hop for this entry in the forwarding table.
    \arg \c metric The metric for this entry in the forwarding table.
    \arg \c entry_data The user_data provided when adding the entry to the
    forwarding table.  This is NOT the same as the callback_data, which is
    passed in via fishnet_fwtable_functions::iterate_entries.
    \return The callback should return 0 (false) if the entry is to be kept in
    the forwarding table and true (1) if it is to be deleted.  Do not call
    fishnet_fwtable_functions::remove_fwtable_entry from within the callback.
    */
typedef int (*fwtable_iterator_cb)(void *callback_data, fnaddr_t dest, 
            int prefix_len, fnaddr_t next_hop, int metric, void *entry_data);

/** \brief A structure that contains function pointers for all fowarding table
    operations, such as lookup and insertion.

    This structure contains function pointers that manipulate the fishnode's
    forwarding table.  The pointers are initialized to point to the built-in
    implementation of the functions.  These are provided as function pointers to
    allow the fishnode to override the built-in implementations with its own
    implementation(s).

    There is a single instance of this structure in a fishnode.  It is a global
    variable named fish_fwd.  You can access the fields directly in any portion
    of your code (e.g., fish_fwd.longest_prefix_match = NULL) to change the
    value of the function pointers or to call them.  Do not declare an
    instance of this structure yourself.
  */
extern struct fishnet_fwtable_functions {
   /** \ingroup FWD
     \brief Add an entry into the forwarding table.
       \arg \c dst The final destination of the route.
       \arg \c prefix_length The prefix length for the route (e.g., 32 for a
       /32).
       \arc \c next_hop The immediate neighbor through which to send the frame
       to get to the final destination.
       \arg \c metric The metric for this route.  Lower is better.  No metric
       can be less than 0.
       \arg \c type Indicates where this route was learned.  It is normally
       one of the FISH_FWD_TYPE_* constants, but any value is valid.
       \arg \c user_data A user specified pointer that is stored with the 
       route.  This can be used to store additional protocol specific data.
       \return An obaque key needed when making modifications (metric changes or
       deletions) to this route in the future.

       This function adds the given route, as described by the function's
       parameters, entry into the forwarding table.  There may be multiple
       routes to the same destination and prefix length.  In that case all
       routes are stored in the forwarding table, however only the route with
       the smallest metric is returned by
       fishnet_fwtable_functions::longest_prefix_match.
    
      The return value must be stored and used to remove / modify routes.

    */
   void* (*add_fwtable_entry)(fnaddr_t dst,
                              int prefix_length,
                              fnaddr_t next_hop,
                              int metric,
                              char type,
                              void *user_data);

   /** \ingroup FWD
     \brief Completely removes an entry from the forwarding table.
      \arg \c route_key The key for the entry to be removed.  The key is
      obtained via the
      return result from the fishnet_fwtable_functions::add_fwtable_entry
      function.
      \return The user_data value originally added into the forwarding table
      for the deleted entry.
    */
   void* (*remove_fwtable_entry)(void *route_key);

   /** \ingroup FWD
     \brief Updates the metric for the given entry key.
      \arg \c route_key The key for the entry to be updated.  The key is
      obtained via the
      return result from the fishnet_fwtable_functions::add_fwtable_entry
      function.
      \arg \c new_metric The new metric value.
      \return True or false if the update succeeds or fails, respectively.

      This function is provided as an optimization.  The same thing can be
      achieved by removing then entry and then adding it back in again, however
      calling update is computationally much cheaper.
    */
   int (*update_fwtable_metric)(void *route_key, int new_metric);

   /** \ingroup FWD
     \brief Performs a longest prefix lookup for the given destination address.
      \arg \c addr The address to lookup
      \return The L3 address of the best next-hop option, or an invalid L3
      address (0) if no entry matches the destination.
    */
   fnaddr_t (*longest_prefix_match)(fnaddr_t addr);

   /** \ingroup FWD
     \brief Looks up the user data void* originally stored when the route
     was added to the forwarding table.
      \arg \c addr The address to lookup
      \arg \c prefix_length The prefix length for the route (e.g., 32 for a
       /32).
      \arg \c type Type used when adding the route to the forwarding table.
      Should be one of the FISH_FWD_TYPE_* constants.
      \return The user_data void* associated with the entry in the
      forwarding table, or NULL if no entry matches the destination.
    */

   void* (*user_data)(fnaddr_t addr, int prefix_len, char type);
   /** \ingroup FWD
     \brief Iterates over all entries in the forwarding table of the given
     type.  Calls the user-supplied callback function for each entry.
      \arg \c callback The function to call for each entry.
      \arg \c callback_param The void* value to pass as the first parameter
      to the callback.
      \arg \c type Type used when adding the route to the forwarding table.
      Should be one of the FISH_FWD_TYPE_* constants.
    */
   void (*iterate_entries)(fwtable_iterator_cb callback, void *callback_param,
            char type);

} fish_fwd;

//@}

/** \addtogroup FCMP FCMP Functions
    \brief Functions and types for the fishnode FCMP implementation

    */
//@{
/** \brief A structure that contains function pointers for all FCMP
    related operations.

    This structure contains function pointers that create and process FCMP
    messages.  The pointers are initialized to point to the built-in
    implementation of the functions.  These are provided as function pointers to
    allow the fishnode to override the built-in implementations with its own
    implementation(s).

    There is a single instance of this structure in a fishnode.  It is a global
    variable named fish_fcmp.  You can access the fields directly in any portion
    of your code (e.g., fish_fcmp.fcmp_received = NULL) to change the
    value of the function pointers or to call them.  Do not declare an
    instance of this structure yourself.
  */
extern struct fishnet_fcmp_functions {
   /** \ingroup FCMP
       \brief This function pointer gets called when a FCMP frame arrives at the
        node for processing.
       \arg \c  l3frame A pointer to the L3 frame containing the FCMP packet.
       \arg \c len The length of the L3 frame.
       
        There is a built-in L2 handler
        that calls this function for every FCMP packet destined to this
        fishnode.  The built-in handler AUTOMATICALLY DISABLES ITSELF when you
        provide a pointer to your own fcmp_received implementation.  You must
        add code to FishnetL3Funcs::fishnode_l3_receive to call this function if you
        override this function!
    */
   void (*fcmp_received)(void *l3frame,      /* The L3 FCMP frame */
                        int len);            /* The length of the L3 frame */

   /** \ingroup FCMP
       \brief This function creates and sends a FCMP response.
       \arg \c l3frame The original L3 frame that triggered the FCMP error
       \arg \c len The length of the original L3 frame
       \arg \c err The FCMP error code to use in the response, in host order.

       This function will not send an FCMP error message in response to a
       broadcast packet or another FCMP packet, therefore it is safe to call on
       all frames that generate a FCMP error.
      */
   void (*send_fcmp_response)(void *l3frame, int len, uint32_t err);
} fish_fcmp;
//@}

/**
    \brief Functions and declarations that interact with the built-in name protocol
    implementation.  Modifing these function pointers allows you to provide
    your own implementation.
  */

extern struct fishnet_name_functions {
   /* This function gets called when a name packet arrives.  It is normally
    * called by the libfish.a code, however if you provide your own function
    * implementation then libfish will NOT automatically call it.
    */
   void (*name_received)(void *l4frame,   /* A pointer to the first byte of the
                                             name L4 payload
                                             */
                       uint16_t len,      /* The length of the L4 payload */
                       fnaddr_t src);     /* The src address of the node sending
                                             the name packet */
} fish_name;


/** \brief Permits the built-in neighbor protocol to use
the "neighbor down" information provided by libfish.  This is the exact same
information provided to the 'neighbor_down_handler_t' as documented below.  This
option does not impact discovering neighbors.  That still takes place via 
broadcast frames as described in the fishnet spec.  Use of this option
causes most (but not all) neighbor departures to be detected immediately.
Not using this option requires a full timeout before detecting a neighbor
has left the network.
*/
#define NEIGHBOR_USE_LIBFISH_NEIGHBOR_DOWN 0x01

/** \brief This function enables the built-in implementation of the neighbor protocol.
 * This will enable a fishnode to discover all its one-hop neighbors.  It must
 * be called after fish_join* and before fish_main.  If it is called at any
 * other point in the code it will not work correctly.
 */

void fish_enable_neighbor_builtin(int features);


/** \brief If present, enable triggered updates in the
 * built-in implementation.  Triggered updates create a much higher network
 * load, especially in larger networks.  They can also make it hard to follow
 * what is happening since the updates fly by quickly.  Passing a value of 0
 * disables triggered updates.  Passing any other value enables them.
 */
#define DVROUTING_TRIGGERED_UPDATES 0x01

/** \brief If present, enable route withdraws.  When using route
withdraws, the dv code will advertise all withdrawn route with a metric of
infinity.  If route withdraws are not used then invalid routes will immediately
leave the dv state.
*/
#define DVROUTING_WITHDRAW_ROUTES 0x02

/** \brief Permits the built-in dv routing solution to use
the "neighbor down" information provided by libfish.  This is the exact same
information provided to the 'neighbor_down_handler_t' as documented below.  This
option does not impact discovering neighbors.  That still takes place via DV
frames as described in the fishnet spec.  Use of this option causes most (but
not all) neighbor departures to be detected immediately.  Not using this option
requires a full timeout before detecting a neighbor has left the network.
*/
#define RVROUTING_USE_LIBFISH_NEIGHBOR_DOWN 0x04

/** \brief Enable split horizion with poison reverse. Using this option
increases resiliency against counting to infinity problems, but also increases
the amount of routing traffic on the network. */
#define DVROUTING_SPLIT_HOR_POISON_REV 0x08

/** \brief Enables route history in the built-in dv implementation.
Non-optimal routes are kept as "backups" in case the primary route fails.  This
option tends to speed route convergance when there is churn in the network.
*/
#define DVROUTING_KEEP_ROUTE_HISTORY 0x10

/** \brief This function enabled the built-in implementation of the distance vector
 * routing protocol.  This routing protocol will discover both neighbors
 * (1-hop) and full L3 routes.  It must called after fish_join* and before
 * fish_main.  If it is called at any other point in the code it will not work
 * correctly.
 *
 * "features" is a bit field composed by bit-wise or-ing any of the feature
 * flags together.  This enables you to test the behavior with and without
 * optimizations in place.  Available features are:
 */
void fish_enable_dvrouting_builtin(int features);

/** \brief The LSA implementation is current incomplete!
  *  Functions, structures, and constants that relate to the built-in
  *   implementation of the link state routing algorithm
  *
  */


extern struct fishnet_lsa_functions {
   /** \brief Both of the function pointers may be NULL, in which case they shouldn't be
    * called!
    */
   void (*lsa_new_neighbor)(fnaddr_t neib);   /*  The neighbor protocol
                                                  implementation calls this
                                                  function when it discovers a
                                                  new neighbor
                                               */
   void (*lsa_neighbor_gone)(fnaddr_t neib);  /*  The neighbor protocol
                                                  implementation calls this
                                                  function when a previously
                                                  discovered neighbor leaves the
                                                  network
                                               */
} fish_lsa;

/** \brief This function enables the built-in implementation of the link-state routing
 * protocol.  There are currently no features that can be turned on/off, so
 * always pass 0 as the parameter */
extern void fish_enable_lsarouting_builtin(int features);

/** \brief The fish_register_neighbor_down_handler registers a function that gets called
 * when a neighbors leaves the network.  The function's signature is defined by
 * the type neighbor_down_handler_t.  This function is UNRELIABLE.  There are
 * many instances where a node is effectivelly down, but hasn't disconnected
 * from the network (e.g., stuck in a tight loop, etc).  This callback may be
 * used as an optimization, but not as the only means for detecting neighbor
 * departures.  Timeouts must also be used.
 *
 * Passing NULL as the only argument to fish_register_neighbor_down_handler will
 * disable the callback.
 */
typedef void (*neighbor_down_handler_t)(fnaddr_t);
void fish_register_neighbor_down_handler(neighbor_down_handler_t);

/** \brief This function prints the contents of the neighbor table maintained by the
 * built-in implementation of the neighbor protocol. */
void fish_print_neighbor_table(void);

/** \ingroup ARP
  \brief This function prints the contents of the ARP table maintained by the built-in
 * implementation of ARP.
 
 This function is meant as a debugging and development aid to help you
 understand why your node is not working correctly.  It only displays the
 contents of the built-in ARP table.  If you provide your own ARP
 table implementation this function is useless.
 */
void fish_print_arp_table(void);

/** \ingroup FWD
  \brief This function prints the contents of the forwarding table as maintained by
 * the built-in forwarding table.

 This function is meant as a debugging and development aid to help you
 understand why your node is not working correctly.  It only displays the
 contents of the built-in forwarding table.  If you provide your own forwarding
 table implementation this function is useless.
 */
void fish_print_forwarding_table(void);

/** \brief This function prints the state maintained as part of the built-in distance
   vector implementation.
 */
void fish_print_dv_state(void);

/** \brief This function prints the LSA's internal view of the fishnet topology.
 */
void fish_print_lsa_topo(void);


#ifndef FALSE
#define FALSE (0)
#endif
#ifndef TRUE
#define TRUE (!FALSE)
#endif

#ifdef __cplusplus
}
#endif
#endif /* __FISH_H__ */


/**
  \mainpage
  
  \section Overview Overview

  fish.h is the place where all public fishnet interfaces are defined.  Look at
  the documentation for that file (or the comments in that file) for specific
  information.

  \section ModIndex Module Documentation
  The fishnet documentation has been grouped into "modules" that should make it
  quicker to find documentation for the functionality you are looking for.
  The modules are:
  \li \ref L1
  \li \ref L2
  \li \ref ARP
  \li \ref L3
  \li \ref FWD
  \li \ref FCMP

  \li \ref debug

  */
