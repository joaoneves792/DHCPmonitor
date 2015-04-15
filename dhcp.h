#pragma once

#define DHCP_SNAME_LEN 64
#define DHCP_FILE_LEN 128
#define DHCP_OPTION_LEN 64

struct dhcp_packet {
	u_int8_t op; /* 0: Message opcode/type */
	u_int8_t htype; /* 1: Hardware addr type (net/if_types.h) */
	u_int8_t hlen; /* 2: Hardware addr length */
	u_int8_t hops; /* 3: Number of relay agent hops from client */
	u_int32_t xid; /* 4: Transaction ID */
	u_int16_t secs; /* 8: Seconds since client started looking */
	u_int16_t flags; /* 10: Flag bits */
	struct in_addr ciaddr; /* 12: Client IP address (if already in use) */
	struct in_addr yiaddr; /* 16: Client IP address */
	struct in_addr siaddr; /* 18: IP address of next server to talk to */
	struct in_addr giaddr; /* 20: DHCP relay agent IP address */
	unsigned char chaddr [16]; /* 24: Client hardware address */
	char sname [DHCP_SNAME_LEN]; /* 40: Server name */
	char file [DHCP_FILE_LEN]; /* 104: Boot filename */
	unsigned char options [DHCP_OPTION_LEN];
	/* 212: Optional parameters
	(actual length dependent on MTU). */
};
/*
struct dhcp_options{//DO NOT MAKE A CAST!! it wont work with the options because some of them have variable lenghts!
	unsigned char magicCookie[4];
	


};
*/
