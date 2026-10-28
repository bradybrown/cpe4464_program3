#include "fish.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>

#define ECHO_PROTOCOL			2
#define ECHO_REQUEST          1
#define ECHO_RESPONSE         2

struct Echo {
   uint32_t code;
   char msg[MTU];
};

void receive_packet(int sock, int sec, int usec)
{
   struct Echo packet;
	fd_set readset;
	int res;
	struct timeval to;
	unsigned int len;

	FD_ZERO(&readset);
	FD_SET(sock, &readset);
	to.tv_sec = sec;
	to.tv_usec = usec;

	res = select(sock+1, &readset, NULL, NULL, &to);
	if (res == 0)
	{
		printf("No response in %d seconds\n", sec);
		return;
	}
	if (res < 1)
	{
		perror("error in select");
		exit(1);
	}

	len = recv(sock, &packet, sizeof(packet), 0);
   if (len < sizeof(packet.code)) {
      printf("Response frame too short\n");
      return;
   }
   len -= sizeof(packet.code);
   packet.msg[len] = 0;

   if (ECHO_RESPONSE != ntohl(packet.code)) {
      printf("Bad code (%d) in response packet\n", ntohl(packet.code));
      return;
   }

	printf("Received: %s\n", packet.msg);
}

int main(int argc, char **argv)
{
	int sock;
	struct Echo packet;
	int len;
	fnaddr_t dest_addr;

	if (argc != 3)
	{
		printf("Usage: %s <domain socket> <fishnet addr of node to bounce echo off of>\n", argv[0]);
		return 1;
	}

	dest_addr = fn_aton(argv[2]);
	if (0 == dest_addr)
	{
		printf("%s is not a valid fishnet address.  Fishnet addresses have the form:\n", argv[2]);
		printf("\tF-n.n.n.n\nwhere n in a 1 byte decimal number (similar to IP dotted-quad notation\n");
		exit(1);
	}

	sock = fishsocket_joinnetwork(argv[1], 4, 0, dest_addr,
         ECHO_PROTOCOL, NULL, NULL);
	if (sock < 0)
	{
		perror("Error connecting to fishnet node");
		exit(1);
	}

	while(1)
	{
		printf("Enter the string to echo, or ^C to exit\n> ");
		fflush(stdout);

		if (NULL == fgets(packet.msg, sizeof(packet.msg), stdin)) {
			perror("Error reading from standard input");
			return 1;
		}

		len = strlen(packet.msg);
		while(packet.msg[len-1] == '\n' || packet.msg[len-1] == '\r')
			packet.msg[--len] = 0;

      packet.code = htonl(ECHO_REQUEST);
      len = strlen(packet.msg) + sizeof(packet.code);
		if (len != send(sock, &packet, len, 0))
		{
			perror("Error sending echo request");
			return 1;
		}

		receive_packet(sock, 10, 0);
	}

	return 0;
}
