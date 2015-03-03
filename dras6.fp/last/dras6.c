/*
*
*  DHCPv6 load generator
*
*
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <time.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef linux
 #include <sys/ioctl.h>
#endif
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "dhcp.h"

static const char hex[17] = "0123456789abcdef";

/* Local Definitions */
#define MAX_TOKENS		54
#define DUID_LLT_LEN		14
typedef struct {
	uint32_t	advertise_latency_avg;
	uint32_t	advertise_latency_min;
	uint32_t	advertise_latency_max;

	uint32_t	reply_latency_avg;
	uint32_t	reply_latency_min;
	uint32_t	reply_latency_max;

	uint32_t	solicits_sent;
	uint32_t	requests_sent;
	uint32_t	releases_sent;
	uint32_t	declines_sent;

	uint32_t	advertise_acks_received;
	uint32_t	advertise_naks_received;
	uint32_t	request_acks_received;
	uint32_t	request_naks_received;
	uint32_t	decline_acks_received;
	uint32_t	decline_naks_received;
	uint32_t	release_acks_received;
	uint32_t	release_naks_received;

	uint32_t	advertise_timeouts;
	uint32_t	request_ack_timeouts;
	uint32_t	release_ack_timeouts;
	uint32_t	decline_ack_timeouts;

	uint32_t	errors;
	uint32_t	failed;
	uint32_t	completed;
} dhcp_stats_t;

typedef struct DHCP_SESSION_T {
	uint8_t			transaction_id[3];
	uint32_t		state;
	uint8_t			options[DHCP6_OPTION_LEN];
	uint8_t			options_length;
	uint32_t		timeouts;
	uint32_t		session_start;
	uint32_t		lease_time;
	uint8_t			prefix_len;
	uint32_t		iaid;
	struct in6_addr		ipaddr;
	struct timeval		solicit_sent;
	struct timeval		advertise_received;
	struct timeval		request_sent;
	struct timeval		decline_sent;
	struct timeval		release_sent;
	struct timeval		reply_received;
	struct timeval		release_received;
	struct timeval		decline_received;
	char			last_packet[DHCP6_PACKET_LEN];
	size_t			last_packet_len;
	uint8_t			serverid[DUID_LLT_LEN];
	uint8_t			mac[6];
	char			hostname[64];
} dhcp_session_t;

typedef struct DHCP_SERVER_T {
	struct sockaddr_in6	sa;
	dhcp_stats_t		stats;
	dhcp_session_t		*list;
	uint32_t		active;
	struct timeval		first_packet_sent;
	struct timeval		last_packet_sent;
	struct timeval		last_packet_received;
	struct DHCP_SERVER_T	*next;
} dhcp_server_t;

typedef struct LEASE_DATA_T {
	struct in6_addr		ipaddr;
	uint8_t			mac[6];
	uint32_t		iaid;
	uint8_t			serverid[DUID_LLT_LEN];
	char			*hostname;
	struct LEASE_DATA_T	*next;
} lease_data_t;
/*
typedef struct {
	uint8_t			msg_type;
	uint8_t			hops;
	struct in6_addr		link_addr;
	struct in6_addr		peer_addr;
	uint16_t		relay_op_no;
	uint16_t		relay_op_len;
} dhcp6_relay_hdr_t;
*/
#define UNALLOCATED		0UL
#define SOLICIT_SENT		1UL
#define SOLICIT_ACK		2UL
#define SOLICIT_NAK		4UL
#define REQUEST_SENT		8UL
#define REQUEST_ACK		16UL
#define REQUEST_NAK		32UL
#define RELEASE_SENT		64UL
#define RELEASE_ACK		128UL
#define RELEASE_NAK		256UL
#define DECLINE_SENT		512UL
#define DECLINE_ACK		1024UL
#define DECLINE_NAK		2048UL
#define PACKET_ERROR		4096UL
#define SESSION_ALLOCATED	8192UL

#define DELTATV(a,b) (1000000*(a.tv_sec - b.tv_sec) + a.tv_usec - b.tv_usec)
/* Globals */
static __const char	*version="$Id$";
static uint32_t		socket_bufsize=128*1024;
static uint32_t		send_delay;
static uint32_t		ia_id = 0;
static int		use_sequential_mac;
static int		send_hostname;
static int		random_hostname;
static int		send_release;
static int		send_decline;
static int		send_info_request;
static int		retransmit;
static int		dhcp_ping;
static int		request_prefix;
static int		renew_lease;
static int		send_until_answered;
static int		verbose;
static int		sock = -1;
static int		use_relay;
static uint32_t		timeout = 5000000UL;
static uint32_t		number_requests=1;
static uint32_t		max_sessions = 25;
static dhcp_server_t	*servers;
static uint32_t		num_servers;
static struct in6_addr	srcaddr = IN6ADDR_ANY_INIT;
static uint8_t		firstmac[6];
static char		*update_domain;
static char		*logfile;
static char		*input_file;
static char		*output_file;
FILE			*logfp;
static FILE		*outfp;
static time_t		start_time;

static int nrequests = 0;
static uint16_t *info_requests;

static int nextraoptions = 0;
static struct option_st {
    int option_no;
    int data_len;
    char *data;
} **extraoptions = NULL;

/* Function prototypes */
static int			is_ack(const char *, int);
static void			increment_txn(uint8_t *);
static void			reader(void);
static void			sender(void);
static int			process_packet(void *, struct timeval *, uint32_t);
static int			send_packet6(uint8_t, dhcp_session_t *, dhcp_server_t *);
static void			parse_args(int , char **);
static int			add_servers(const char *);
static int			log_open(void);
static uint32_t			read_lease_data(lease_data_t **);
static void			usage(void);
static int			process_sessions(void);
static void			print_packet(size_t, struct dhcpv6_packet *, const char *);
static int			test_statistics(void);
static struct in6_addr		get_local_addr(void);
static int			addoption(int , char *);
static dhcp_session_t		*find_free_session(dhcp_server_t *);
static int			log_open(void);
static int			addoption(int , char *);
void 				getmac(uint8_t *);
int				get_tokens(char *, char **, int);
void				print_lease( FILE *, dhcp_session_t *);
int				pack_client_fqdn(uint8_t *, char *);
int				encode_domain(char *, uint8_t *);

// #include "dras_common.h"

int main(int argc, char **argv)
{
	struct sockaddr_in6 ca;
	int ret;
	dhcp_server_t *server;

	logfp = stderr;
	time(&start_time);
	parse_args(argc, argv);

	if (getuid()){
		fprintf(stderr,"\n\tThis program must be run as root\n");
		exit(1);
	}

	srand(getpid() + time(NULL));
	log_open();

	if (output_file != NULL){
		outfp = fopen(output_file,"w");
		if (outfp == NULL){
			fprintf(logfp,"Open failed: %s\n",output_file);
			exit(1);
		}
	}
	if (!memcmp(&srcaddr,&in6addr_any, sizeof(struct in6_addr)))
	    srcaddr = get_local_addr();
	signal(SIGPIPE, SIG_IGN);

	sock=socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0){
		perror("socket:");
		exit(1);
	}

	ret = setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
			(char *) &socket_bufsize, sizeof(socket_bufsize));
	if (ret < 0)
		fprintf(stderr, "Warning:  setsockbuf(SO_RCVBUF) failed\n");

        ret = setsockopt(sock, SOL_SOCKET, SO_SNDBUF,
			(char *) &socket_bufsize, sizeof(socket_bufsize));
        if (ret < 0)
		fprintf(stderr, "Warning:  setsockbuf(SO_SNDBUF) failed\n");

	memset(&ca, 0, sizeof(struct sockaddr_in6));
	ca.sin6_family = AF_INET6;
	ca.sin6_port = htons(DHCP6_LOCAL_PORT);

	memcpy(&ca.sin6_addr, &srcaddr, sizeof(struct in6_addr));
	if (bind(sock, (struct sockaddr *)&ca, sizeof(ca))< 0 ){
		perror("bind");
		exit(1);
	}

	for (server=servers; server != NULL; server=server->next){
		server->list = malloc(max_sessions * sizeof(dhcp_session_t));
		memset(server->list, '\0', max_sessions * sizeof(dhcp_session_t));
	}

	sender();
	return(test_statistics());
}

/* Function to increment txn_id */
void increment_txn(uint8_t *transaction_id)
{
	if (transaction_id[0] != 0xff)
		transaction_id[0]++;
	else {
		transaction_id[0] = 0x00;
		if (transaction_id[1] != 0xff)
			transaction_id[1]++;
		else {
			transaction_id[1] = 0x00;
			transaction_id[2]++;
		}
	}
}

/*
    Test complete when sessions_started = completed + timeouts
*/
void sender(void)
{
	dhcp_session_t		*session;
	dhcp_server_t		*current_server = servers;
	lease_data_t		*leases = NULL;
	lease_data_t		*lease;
	uint8_t			ntransactions = 1;
	int			complete = 0;

	if ( input_file != NULL &&
			(number_requests=read_lease_data(&leases)) == 0 ){
			exit(1);
	}
	lease = leases;
	while  (!complete){
		if (input_file != NULL){
			if (lease != NULL){
				session = find_free_session(current_server);
				if (session != NULL){
					increment_txn(session->transaction_id);
					ntransactions++;
					session->ipaddr = lease->ipaddr;
					memcpy(session->serverid, lease->serverid, DUID_LLT_LEN);

					if (lease->hostname != NULL)
                                           strncpy(session->hostname, lease->hostname, sizeof(session->hostname));
                                    	else
                                           *(session->hostname) = '\0';

					session->iaid = lease->iaid;

					if (send_delay)
						poll(NULL, 0, send_delay);

					if (!memcmp(&lease->ipaddr,&in6addr_any, sizeof(struct in6_addr)))
						send_packet6(DHCPV6_SOLICIT, session, current_server);
					else {
						session->state |= SOLICIT_SENT|SOLICIT_ACK;
						send_packet6(DHCPV6_RENEW, session, current_server);
					}
					lease=lease->next;
				}
			}
		}
		else if (ntransactions <= number_requests){
			session = find_free_session(current_server);
			if (session != NULL){
				getmac(session->mac);
				increment_txn(session->transaction_id);
				ntransactions++;
				session->iaid = ++ia_id;
				session->ipaddr = in6addr_any;
    	                        if (send_hostname) {
                                	if (random_hostname)
                                    		snprintf(session->hostname, sizeof(session->hostname),
							"h%u%u.%s", rand(), rand(),
							update_domain ? update_domain : "");
                                	else
                                    		snprintf(session->hostname,sizeof(session->hostname),
                                                  "h%02x%02x%02x%02x%02x%02x.%s",
                                                  session->mac[0], session->mac[1],
                                                  session->mac[2], session->mac[3], session->mac[4],
                                                  session->mac[5], update_domain ? update_domain : "");
       		                }
               		        else
                                	*(session->hostname) = '\0';

				session->serverid[0] = 0xff;
				if (send_delay)
					poll(NULL, 0, send_delay);

				send_packet6(DHCPV6_SOLICIT, session, current_server);
			}
                        else if (num_servers > 1)
                                getmac(NULL);    /* keep MAC allocation constant */

		}
		reader();
		complete = process_sessions();
		if ( current_server->next == NULL)
			current_server = servers;
		else
			current_server = current_server->next;
	}
}

int test_statistics(void)
{
	dhcp_server_t	*iter;
	double	elapsed;
	int	retval = 0;
	char	ipstr[INET6_ADDRSTRLEN];

	fprintf(logfp, "\nTest started:         %s\n",
			ctime(&start_time));
	for (iter=servers; iter != NULL; iter=iter->next){
		inet_ntop(AF_INET6, &iter->sa.sin6_addr, ipstr, sizeof(struct in6_addr));
		fprintf(logfp,"Server %-s\n", ipstr);
		if ( iter->stats.advertise_acks_received +
			iter->stats.advertise_naks_received +
			iter->stats.request_acks_received +
			iter->stats.request_naks_received +
			iter->stats.release_acks_received +
			iter->stats.release_naks_received == 0){
			fprintf(logfp,"\n\tNo replies received.\n");
			retval = 1;
			continue;
		}

                if (iter->stats.failed + iter->stats.errors > 0 )
			retval = 1;
		fprintf(logfp,"Solicits sent:          %6u\n",
				iter->stats.solicits_sent);
		fprintf(logfp,"Advertise Acks Received:     %6u\n",
				iter->stats.advertise_acks_received);
		fprintf(logfp,"Advertise Naks Received:     %6u\n",
				iter->stats.advertise_naks_received);

		if (send_info_request) {
			fprintf(logfp,"Info-Req sent:          %6u\n",
					iter->stats.requests_sent);
			fprintf(logfp,"Info-Req Acks Received: %6u\n",
					iter->stats.request_acks_received);
			fprintf(logfp,"Info-Req Naks received: %6u\n",
					iter->stats.request_naks_received);
		}
		else if (renew_lease) {
			fprintf(logfp,"Renews sent:            %6u\n",
					iter->stats.requests_sent);
			fprintf(logfp,"Renew Acks Received:    %6u\n",
					iter->stats.request_acks_received);
			fprintf(logfp,"Renew Naks received:    %6u\n",
					iter->stats.request_naks_received);
		} else {
			fprintf(logfp,"Requests sent:          %6u\n",
					iter->stats.requests_sent);
			fprintf(logfp,"Request Acks Received:  %6u\n",
					iter->stats.request_acks_received);
			fprintf(logfp,"Request Naks received:  %6u\n",
					iter->stats.request_naks_received);
		}

		fprintf(logfp,"Releases sent:          %6u\n",iter->stats.releases_sent);
		fprintf(logfp,"Release Acks Received:  %6u\n",
				iter->stats.release_acks_received);
		fprintf(logfp,"Declines sent:          %6u\n",iter->stats.declines_sent);
		fprintf(logfp,"Decline Acks Received:  %6u\n",
				iter->stats.decline_acks_received);

		fprintf(logfp,"Advertise timeouts:     %6u\n",
			iter->stats.advertise_timeouts);
		fprintf(logfp,"ACK Timeouts:           %6u\n",
			iter->stats.request_ack_timeouts);
		elapsed = DELTATV(iter->last_packet_received,
					iter->first_packet_sent)/1000000.000;

		fprintf(logfp,"Completed:              %6u\n",iter->stats.completed);
		fprintf(logfp,"Failed:                 %6u\n",iter->stats.failed);
		fprintf(logfp,"Errors:                 %6u\n",iter->stats.errors);
		fprintf(logfp,"Elapsed time:         %15.2f secs\n", elapsed);
		fprintf(logfp,"Advertise Latency (Min/Max/Avg): %.3f/%.3f/%.3f (ms)\n",
		0.0010 * (double) iter->stats.advertise_latency_min,
		0.0010 * (double)iter->stats.advertise_latency_max,
		0.0010 * (double) iter->stats.advertise_latency_avg / (double)
		(iter->stats.advertise_acks_received + iter->stats.advertise_naks_received ?
		 	iter->stats.advertise_acks_received + iter->stats.advertise_naks_received : 1));
		fprintf(logfp,"Reply Latency (Min/Max/Avg):       %.3f/%.3f/%.3f (ms)\n",
		0.0010 * (double) iter->stats.reply_latency_min,
		0.0010 * (double) iter->stats.reply_latency_max,
		0.0010 * (double) iter->stats.reply_latency_avg / (double)
		(iter->stats.release_acks_received ? iter->stats.release_acks_received : 1));
		fprintf(logfp,"Advertise Acks/sec:                  %6.2f\n",
			(double)iter->stats.advertise_acks_received/elapsed);
		fprintf(logfp,"Leases/sec:                      %6.2f\n",
			(double)iter->stats.request_acks_received/elapsed);

		fprintf(logfp, "-----------------------------------------\n");
	}
	fprintf(logfp,"Return value: %d\n", retval);
	return(retval);
}

int send_packet6(uint8_t type, dhcp_session_t *session, dhcp_server_t *server)
{
	uint8_t		buffer[1024]; /* Fix to DHCP_MTU */
	struct dhcpv6_packet	*packet;
	//dhcp6_relay_hdr_t		*relay_hdr;
	struct timeval		timestamp;
	int			offset=0, i=0, j=0, dhcp_msg_len;
	unsigned char *iana_len = NULL;
	unsigned char *iapd_len = NULL;

	//fprintf(logfp,"Entering send_packet6");

	packet = (struct dhcpv6_packet *) buffer;
	if (use_relay){  // Add relay header 
		buffer[0] = DHCPV6_RELAY_FORW; //msg type
		buffer[1] = 1; // Relay MSG Hops
		memcpy(buffer + 2, &srcaddr, sizeof(struct in6_addr));
		memcpy(buffer + 18, &session->ipaddr, sizeof(struct in6_addr));
		*((uint16_t *) (buffer + 34)) = htons(D6O_RELAY_MSG);
		packet = (struct dhcpv6_packet *) ( buffer + 38);
	}

		
	packet->msg_type = type;
	for (j = 0; j < 3; j++)
		packet->transaction_id[j] = session->transaction_id[j];

	if (type == DHCPV6_SOLICIT || type == DHCPV6_RENEW){
		/* CLIENT_ID option */
		*((uint16_t *) (packet->options + offset)) = htons(D6O_CLIENTID);
		offset += 2;
		*((uint16_t *) (packet->options + offset)) = htons(DUID_LLT_LEN);
		offset += 2;
		*((uint16_t *) (packet->options + offset)) = htons(1);
		offset += 2;
		*((uint16_t *) (packet->options + offset)) = htons(1);
		offset += 2;
		memset(packet->options + offset, 0x88, 4);
		offset += 4;
		memcpy(packet->options + offset, session->mac, 6);
		offset += 6;

		/* request Status Code from server */
		*((uint16_t *) (packet->options + offset)) = htons(D6O_STATUS_CODE);
		offset += 2;
		*((uint16_t *) (packet->options + offset)) = htons(2);
		offset += 2;
		*((uint16_t *) (packet->options + offset)) = htons(STATUS_Success);
		offset += 2;

		if (request_prefix)
		{
			/* IA_PD */
			*((uint16_t *) (packet->options + offset)) = htons(D6O_IA_PD);
			offset += 2;
			iapd_len = packet->options + offset; /* remember iapd_len pointer for later value calculation */
			offset += 2;
			*((uint32_t *) (packet->options + offset)) = htonl(session->iaid);
			offset += 4;
			memset(packet->options + offset, 0, 8); /* T1 = 0, T2 = 0 */
			offset += 8;

			/* IA_PD_options: IAPREFIX */
			*((uint16_t *) (packet->options + offset)) = htons(D6O_IAPREFIX);
			offset += 2;

			/* 16 bytes: IPv6 prefix + 8 bytes: lease times + 1 byte: prefix-length */
			*((uint16_t *) (packet->options + offset)) = htons(25);
			offset += 2;

			/* we accept any lease times */
			memset(packet->options + offset, 0, 8);
			offset += 8;

			/* prefix-length */
			*(packet->options + offset) = session->prefix_len;
			offset += 1;

			/* IP prefix */
			memcpy(packet->options + offset, &session->ipaddr, sizeof(session->ipaddr));
			offset += sizeof(session->ipaddr);

			*((uint16_t *) iapd_len) = htons((packet->options + offset) - iapd_len - 2);
		}
		else
		{
			/* IA_NA */
			*((uint16_t *) (packet->options + offset)) = htons(D6O_IA_NA);
			offset += 2;
			iana_len = packet->options + offset; /* remember iana_len pointer for later value calculation */
			offset += 2;
			*((uint32_t *) (packet->options + offset)) = htonl(session->iaid);
			offset += 4;

			memset(packet->options + offset, 0xfe, 8); /* T1 = 0, T2 = 0 */
			offset += 8;

			/* IA_NA_options: IAADDR */
			*((uint16_t *) (packet->options + offset)) = htons(D6O_IAADDR);
			offset += 2;

			/* IADDR option-len: 16 bytes: IPv6 + 8 bytes: lease times */
			*((uint16_t *) (packet->options + offset)) = htons(24);
			offset += 2;

			/* IP address */
			memcpy(packet->options + offset, &session->ipaddr, sizeof(session->ipaddr));
			offset += sizeof(session->ipaddr);

			/* we accept any lease times */
			memset(packet->options + offset, 0, 8);
			offset += 8;

			*((uint16_t *) iana_len) = htons((packet->options + offset) - iana_len - 2);
		}
	}
	else if (type == DHCPV6_REQUEST || type == DHCPV6_RELEASE || type == DHCPV6_DECLINE) {
		/* just reuse previous ADVERTISE packet options */
		unsigned char *last_packet_opts = ((struct dhcpv6_packet *) session->last_packet)->options;
		memcpy(packet->options + offset, last_packet_opts, session->last_packet_len - 4);
		offset += session->last_packet_len - 4;
	}
	else if (type == DHCPV6_INFORMATION_REQUEST)
	{
		/* CLIENT_ID option */
		*((uint16_t *) (packet->options + offset)) = htons(D6O_CLIENTID);
		offset += 2;
		*((uint16_t *) (packet->options + offset)) = htons(DUID_LLT_LEN);
		offset += 2;
		*((uint16_t *) (packet->options + offset)) = htons(1);
		offset += 2;
		*((uint16_t *) (packet->options + offset)) = htons(1);
		offset += 2;
		memset(packet->options + offset, 0x88, 4);
		offset += 4;
		memcpy(packet->options + offset, &session->mac, 6);
		offset += 6;

		/* append options to request */
/*
		*((uint16_t *) (packet->options + offset)) = htons(D6O_ORO);
		offset += 2;
		*((uint16_t *) (packet->options + offset)) = 0; //Empty doesn't seem to work!
		offset += 2;
*/
/*
		*((uint16_t *) (packet->options + offset)) = htons(nrequests * sizeof(info_requests[0]));
		offset += 2;
		memcpy(packet->options + offset, info_requests, nrequests * sizeof(info_requests[0]));
		offset += nrequests * sizeof(info_requests[0]);
*/
	}
	else
	{
		fprintf(logfp,"\n\tUnsupported packet type: %d\n", type);
		exit(1);
	}
	if (session->serverid[0] != 0xff){
		//printf("adding serverid to packet\n");
		*((uint16_t *) (packet->options + offset)) = htons(D6O_SERVERID);
		offset += 2;
		*((uint16_t *) (packet->options + offset)) = htons(DUID_LLT_LEN);
		offset += 2;
		memcpy(packet->options + offset, session->serverid, DUID_LLT_LEN);
		offset += DUID_LLT_LEN;
	}
	if (session->hostname[0] != '\0')
		offset += pack_client_fqdn(packet->options + offset, session->hostname);

	/*  Ask for all options */
	*((uint16_t *) (packet->options + offset)) = htons(D6O_ORO);
	offset += 2;
	*((uint16_t *) (packet->options + offset)) = htons(nrequests * sizeof(uint16_t));
	offset += 2;
	memcpy(packet->options + offset, info_requests, nrequests * sizeof(uint16_t));
	offset += nrequests * sizeof(uint16_t);

	/* append any user defined options */
	if (nextraoptions) {
		for (i = 0; i < nextraoptions; i++) {
			*((uint16_t *) (packet->options + offset)) = htons(extraoptions[i]->option_no);
			offset += 2;
			*((uint16_t *) (packet->options + offset)) = htons(extraoptions[i]->data_len);
			offset += 2;
			memcpy(packet->options + offset, extraoptions[i]->data, extraoptions[i]->data_len);
			offset += extraoptions[i]->data_len;
		}
	}

	gettimeofday(&timestamp, NULL);
	if (session->session_start == 0)
		session->session_start = timestamp.tv_sec;

	/* Set the relayed message option length for relay agents */
	dhcp_msg_len = offset + 4 ;
	// printf("\nDHCP packet size: %u\n", dhcp_msg_len);
	if (use_relay){
		*((uint16_t *) (buffer + 36)) =  htons(dhcp_msg_len);
		dhcp_msg_len += 38;
	}
	/* send the packet */
	if (sendto(sock, buffer, dhcp_msg_len, 0, (struct sockaddr *)&server->sa, sizeof(struct sockaddr_in6)) < 0 ){
		fprintf(logfp,"sendto failed:\n");
		return(-1);
	}

	if (verbose)
		print_packet(use_relay ? dhcp_msg_len - 38 : dhcp_msg_len, packet, "Sent: ");

	/* Update statistics */
	server->last_packet_sent.tv_sec = timestamp.tv_sec;
	server->last_packet_sent.tv_usec = timestamp.tv_usec;
	if (server->first_packet_sent.tv_sec == 0){
		server->first_packet_sent.tv_sec = timestamp.tv_sec;
		server->first_packet_sent.tv_usec = timestamp.tv_usec;
	}

	if (type == DHCPV6_SOLICIT) {
		server->stats.solicits_sent++;
		session->state |= SOLICIT_SENT;
		session->solicit_sent.tv_sec = timestamp.tv_sec;
		session->solicit_sent.tv_usec = timestamp.tv_usec;
	}
	else if (type == DHCPV6_REQUEST || type == DHCPV6_RENEW ||
			type == DHCPV6_INFORMATION_REQUEST){
		server->stats.requests_sent++;
		session->state |= REQUEST_SENT;
		session->request_sent.tv_sec = timestamp.tv_sec;
		session->request_sent.tv_usec = timestamp.tv_usec;
	}
	else if (type == DHCPV6_RELEASE){
		server->stats.releases_sent++;
		session->state |= RELEASE_SENT;
	}
	else if (type == DHCPV6_DECLINE){
		server->stats.declines_sent++;
		session->state |= DECLINE_SENT;
	}

	//fprintf(logfp,"Leaving send_packet6");
	return(0);
}

int  process_sessions(void)
{
	dhcp_session_t *session;
	dhcp_server_t *server;
	uint32_t sent=0, completed=0, failed=0;
	struct timeval	now;
	int		i,j;

	gettimeofday(&now, NULL);
	for (server=servers; server != NULL; server=server->next){
	for (i = 0,j=0; j< server->active && i < max_sessions; i++){
		session = server->list + i;
		if (session->state == UNALLOCATED)
			continue;
		j++;
		switch (session->state){
			case SESSION_ALLOCATED:
			break;
			case PACKET_ERROR:
			case SESSION_ALLOCATED|SOLICIT_SENT|SOLICIT_NAK:
				server->stats.errors++;
				server->stats.failed++;
				memset(session, '\0', sizeof(dhcp_session_t));
				server->active--;
			break;
			case SESSION_ALLOCATED|SOLICIT_SENT:
				if ( DELTATV(now, session->solicit_sent) > timeout ){
					server->stats.advertise_timeouts++;
					if (retransmit > session->timeouts || send_until_answered){
						session->timeouts++;
						send_packet6(DHCPV6_SOLICIT, session, server);
					}
					else {
						server->stats.failed++;
						memset(session, '\0', sizeof(dhcp_session_t));
						server->active--;
					}
				}
			break;
			case SESSION_ALLOCATED|SOLICIT_SENT|SOLICIT_ACK:
				if (dhcp_ping == 1){
					server->stats.completed++;
					memset(session, '\0', sizeof(dhcp_session_t));
					server->active--;

					continue;
				}
				session->timeouts = 0;
				if (send_info_request)
					send_packet6(DHCPV6_INFORMATION_REQUEST, session, server);
				else if (renew_lease)
					send_packet6(DHCPV6_RENEW, session, server);
				else
					send_packet6(DHCPV6_REQUEST, session, server);
			break;
			case SESSION_ALLOCATED|SOLICIT_SENT|SOLICIT_ACK|REQUEST_SENT:
				if ( DELTATV(now, session->request_sent) > timeout ){
					server->stats.request_ack_timeouts++;

					if (retransmit > session->timeouts || send_until_answered){
						session->timeouts++;
						if (send_info_request)
							send_packet6(DHCPV6_INFORMATION_REQUEST, session, server);
						else if (renew_lease)
							send_packet6(DHCPV6_RENEW, session, server);
						else
							send_packet6(DHCPV6_REQUEST, session, server);
					}
					else {
						server->stats.failed++;
						memset(session, '\0', sizeof(dhcp_session_t));
						server->active--;
					}
				}
			break;
			case SESSION_ALLOCATED|SOLICIT_SENT|SOLICIT_ACK|REQUEST_SENT|REQUEST_ACK:
				if (send_release){
					send_packet6(DHCPV6_RELEASE, session, server);
				}
				else if (send_decline){
					send_packet6(DHCPV6_DECLINE, session, server);
				}
				else {
					server->stats.completed++;
					if (outfp)
						print_lease(outfp, session);
					memset(session, '\0', sizeof(dhcp_session_t));
					server->active--;
				}
			break;
			case SESSION_ALLOCATED|SOLICIT_SENT|SOLICIT_ACK|REQUEST_SENT|REQUEST_NAK:
				server->stats.failed++;
				memset(session, '\0', sizeof(dhcp_session_t));
				server->active--;
			break;
			case SESSION_ALLOCATED|SOLICIT_SENT|SOLICIT_ACK|REQUEST_SENT|REQUEST_ACK|DECLINE_SENT:
				if ( DELTATV(now, session->decline_sent) > timeout ){
					server->stats.decline_ack_timeouts++;
					if (retransmit > session->timeouts || send_until_answered){
						session->timeouts++;
						send_packet6(DHCPV6_DECLINE, session, server);
					}
					else {
						server->stats.failed++;
						memset(session, '\0', sizeof(dhcp_session_t));
						server->active--;
					}
				}
			break;
			case SESSION_ALLOCATED|SOLICIT_SENT|SOLICIT_ACK|REQUEST_SENT|REQUEST_ACK|RELEASE_SENT:
				if ( DELTATV(now, session->release_sent) > timeout ){
					server->stats.release_ack_timeouts++;
					if (retransmit > session->timeouts || send_until_answered){
						session->timeouts++;
						send_packet6(DHCPV6_RELEASE, session, server);
					}
					else {
						server->stats.failed++;
						memset(session, '\0', sizeof(dhcp_session_t));
						server->active--;
					}
				}
			break;
			case SESSION_ALLOCATED|SOLICIT_SENT|SOLICIT_ACK|REQUEST_SENT|REQUEST_ACK|DECLINE_SENT|DECLINE_ACK:
			case SESSION_ALLOCATED|SOLICIT_SENT|SOLICIT_ACK|REQUEST_SENT|REQUEST_ACK|RELEASE_SENT|RELEASE_ACK:
				server->stats.completed++;
				memset(session, '\0', sizeof(dhcp_session_t));
				server->active--;
			break;
			default:
				server->stats.errors++;
				server->stats.failed++;
				fprintf(logfp,"Undefined session state %u\n", session->state);
				fprintf(logfp,"\tSent-SOLICIT: %u.%u\n"
					"\tRecv-ADVERTISE: %u.%u\n"
					"\tSent-REQUEST: %u.%u\n"
					"\tRecv-ACK: %u.%u\n",
					(uint32_t)session->solicit_sent.tv_sec,
					(uint32_t)session->solicit_sent.tv_usec,
					(uint32_t)session->advertise_received.tv_sec,
					(uint32_t)session->advertise_received.tv_usec,
					(uint32_t)session->request_sent.tv_sec,
					(uint32_t)session->request_sent.tv_usec,
					(uint32_t)session->reply_received.tv_sec,
					(uint32_t)session->reply_received.tv_usec);
					memset(session, '\0', sizeof(dhcp_session_t));
					server->active--;
		}

	}
		if ( input_file != NULL) {
			sent += server->stats.requests_sent;
		} else {
			sent += server->stats.solicits_sent;
		}
		completed += server->stats.completed;
		failed    += server->stats.failed;
	}
	//fprintf(stderr, "Sent %u Completed %u Failed %u\n",
		//sent, completed, failed);
	if ( sent >= number_requests){
		if (send_until_answered && completed < number_requests){
			return(0);
		}
		else if ( completed + failed >= number_requests){/* We are done */
			return(1);
		}
	}
	return(0);
}

/* Parse options and check if there is any STATUS CODE with state != Success */
int is_ack(const char *options, int length)
{
	const char *option = options;
	while (option < options + length)
	{
		uint16_t option_type = ntohs(*((uint16_t *) option));
		if (option_type == D6O_IA_NA || option_type == D6O_IA_TA || option_type == D6O_IA_PD)
		{
			size_t option_len = ntohs(*((uint16_t *) (option + 2)));
			const char *options_base = option + 16; /* start of IA_ADDR/IA_PD-options */
			while (options_base < option + option_len + 4)
			{
				if (*((uint16_t *) options_base) == htons(D6O_STATUS_CODE))
				{
					if (*((uint16_t *) (options_base + 4)) != htons(STATUS_Success))
						return 0;
					break;
				}
				/* option size + option-len size */
				options_base += ntohs(*((uint16_t *) (options_base + 2))) + 4;
			}
		}

		if (option_type == D6O_STATUS_CODE)
			if (*((uint16_t *) (option + 4)) != htons(STATUS_Success))
				return 0;

		/* option size + option-len size */
		option += ntohs(*((uint16_t *) (option + 2))) + 4;
	}
	return 1;
}

int process_packet(void *p, struct timeval *timestamp, uint32_t length)
{
	int		found=0;
	dhcp_server_t	*server;
	dhcp_session_t	*session=NULL;
	dhcp_stats_t	*stats=NULL;
	struct dhcpv6_packet *packet = (struct dhcpv6_packet *) p;
	const char *options = (const char *) packet->options;
	uint32_t	offset = 0;
	uint32_t	dt;
	uint16_t	otype;
	int		i;

	// XXX Bad assumption that relay message is first option
	if (use_relay == 1 && *((char *)p) == DHCPV6_RELAY_REPL){
		length -= 38; //XXX  Assumes RELAY_REPLY is the only option!
		packet = (struct dhcpv6_packet *) ((char *)p + 38);// Skip relay hdr
		options = (const char *) packet->options;
	}

	for (server=servers; server != NULL; server=server->next){
		for (i=0; i < max_sessions; i++){
			session = server->list + i;
			if (!memcmp(session->transaction_id, packet->transaction_id, 3)){
				found=1;
				break;
			}
		}
		if (found == 1)
			break;
	}
	while ( offset < length - 4){
		otype =  ntohs(*((uint16_t *)(options + offset)));
		if (otype == 0){
			fprintf(logfp,"Error: Option is zero!\n");
			break;
		}
		//printf("process_packet: Option %u found\n", otype);
		if (otype == D6O_SERVERID){
			memcpy(session->serverid, packet->options + offset +4, DUID_LLT_LEN);//XXX Assume DUID-LLT
			//printf("Server ID: ");	
			//for (i=0; i < DUID_LLT_LEN; i++)
				//printf("%02x", session->serverid[i]);
			//printf("\n");	
		}
		if (otype == D6O_IA_NA){  //XXX lame peek of offered IP
			if ( ntohs(*((uint16_t *)(options + offset + 16))) != D6O_IAADDR){
				fprintf(logfp, "Uh Oh...No D6O_IAADDR found in D6O_IA_NA\n");
			}
			else {
				memcpy(&session->ipaddr, options + offset + 20, sizeof(struct in6_addr));
				//fprintf(logfp, "Found IP in IAADDR: %s\n",
				//inet_ntop(AF_INET6, &session->ipaddr, addr, INET6_ADDRSTRLEN));
			}
		}
			

		offset += (4 + ntohs(*((uint16_t *)(options + offset + 2))));
	}
		
	if ( server == NULL || session == NULL)
		return(-1);

	server->last_packet_received.tv_sec = timestamp->tv_sec;
	server->last_packet_received.tv_usec = timestamp->tv_usec;
	stats = &server->stats;

	switch (packet->msg_type){
		case DHCPV6_ADVERTISE:
			if (is_ack(options, length - 4))
			{
				session->state |= SOLICIT_ACK;
				stats->advertise_acks_received++;
			}
			else
			{
				session->state |= SOLICIT_NAK;
				stats->advertise_naks_received++;
			}

			session->advertise_received.tv_sec = timestamp->tv_sec;
			session->advertise_received.tv_usec = timestamp->tv_usec;
			dt = DELTATV((*timestamp), session->solicit_sent);
			stats->advertise_latency_avg += dt;
			if ( dt < stats->advertise_latency_min )
				stats->advertise_latency_min = dt;
			if ( dt > stats->advertise_latency_max )
				stats->advertise_latency_max = dt;

		break;
		case DHCPV6_REPLY:
			switch (session->state) {
				case SESSION_ALLOCATED|SOLICIT_SENT|SOLICIT_ACK|REQUEST_SENT:
					if (is_ack(options, length - 4))
					{
						session->state |= REQUEST_ACK;
						stats->request_acks_received++;
					}
					else
					{
						session->state |= REQUEST_NAK;
						stats->request_naks_received++;
					}
					session->reply_received.tv_sec = timestamp->tv_sec;
					session->reply_received.tv_usec = timestamp->tv_usec;
				break;
				case SESSION_ALLOCATED|SOLICIT_SENT|SOLICIT_ACK|REQUEST_SENT|REQUEST_ACK|DECLINE_SENT:
					if (is_ack(options, length - 4))
					{
						session->state |= DECLINE_ACK;
						stats->decline_acks_received++;
					}
					else
					{
						session->state |= DECLINE_NAK;
						stats->decline_naks_received++;
					}
					session->decline_received.tv_sec = timestamp->tv_sec;
					session->decline_received.tv_usec = timestamp->tv_usec;
				break;
				case SESSION_ALLOCATED|SOLICIT_SENT|SOLICIT_ACK|REQUEST_SENT|REQUEST_ACK|RELEASE_SENT:
					if (is_ack(options, length - 4))
					{
						session->state |= RELEASE_ACK;
						stats->release_acks_received++;
					}
					else
					{
						session->state |= RELEASE_NAK;
						stats->release_naks_received++;
					}
					session->release_received.tv_sec = timestamp->tv_sec;
					session->release_received.tv_usec = timestamp->tv_usec;
				break;
				default:
					fprintf(logfp, "Unknown session state: %d\n", session->state);
				break;
			}

			dt = DELTATV((*timestamp), session->request_sent);
			stats->reply_latency_avg += dt;
			if ( dt < stats->reply_latency_min )
				stats->reply_latency_min = dt;
			if ( dt > stats->reply_latency_max )
				stats->reply_latency_max = dt;

			break;
		default:
			fprintf(logfp,"Unknown DHCP type: %d\n", packet->msg_type);
			session->state = PACKET_ERROR;
			return(-1);
	}

	
	/* remember last packet for next possible reuse */
	memcpy(session->last_packet, packet, length);
	session->last_packet_len = length;

	if (verbose)
		print_packet(length, packet, "Recv: ");

	return(0);
}

void parse_args(int argc, char **argv)
{
	char		ch;
	int		i;
	uint64_t	val;
	int		temp[6];
	char		*tokes[32];

	if ( argc < 3)
		usage();

	while ((ch = getopt(argc, argv, "a:Ac:ed:D:f:hHi:I:l:mn:No:O:pPq:rR:t:vw")) != -1){
		switch (ch) {
           	case 'a':
                	if (strchr(optarg, ':') == NULL) {
                			val = atoll(optarg);
                  			for (i = 0; i < 6; i++)
                  				firstmac[5 - i] = (val & (0x00ffULL << i * 8)) >> i * 8;
                  		}
                		else {
                			if (sscanf(optarg, "%2x:%2x:%2x:%2x:%2x:%2x",
                			temp, temp + 1, temp + 2, temp + 3, temp + 4, temp + 5) < 6) {
                 			fprintf(stderr, "\nBad MAC Address in -a option: %s\n", optarg);
                 			usage();
                 		}
                  		for (i = 0; i < 6; i++)
                  			firstmac[i] = (char) temp[i];
                  	}
                  	use_sequential_mac = 1;
                  	break;
		case 'A':
			fprintf(stderr,"Will simulate relay agent\n");
			use_relay = 1;
			break;
		case 'c':
                        if ( inet_pton(AF_INET6, optarg, &srcaddr) == 0 ){
                                fprintf(stderr,"Invalid source IPV6 address %s\n",
                                        optarg);
                                exit(1);
                        }
                        break;
                case 'd':
			send_delay = atol(optarg);
			break;
                case 'D':
			update_domain = strdup(optarg);
			fprintf(stderr,"Using %s for DDNS updates\n", update_domain);
			break;
		case 'e':
			send_decline = 1;
			break;
		case 'f':
			input_file = strdup(optarg);
			break;
		case 'h':
			send_hostname = 1;
			break;
		case 'H':
			random_hostname = 1;
			send_hostname = 1;
			break;
		case 'i':
			if (add_servers(optarg) < 0)
				exit(1);
			break;
		case 'I':
		        nrequests = get_tokens(optarg, tokes, MAX_TOKENS);
			info_requests = malloc(nrequests * sizeof(uint16_t));
			assert(info_requests);
			for (i=0; i < nrequests; i++)
				info_requests[i] = htons(atoi(tokes[i]));
			break;
		case 'l':
			logfile = strdup(optarg);
			break;
		case 'm':
                	use_sequential_mac = 1;
                	break;
		case 'n':
			number_requests = atol(optarg);
			break;
		case 'N':
			send_until_answered = 1;
			break;
		case 'o':
			output_file = strdup(optarg);
			break;
		case 'O':
			{
				int option;
				char *s;

				option = strtol(optarg, &s, 0);
				if (option == 0xff || s[0] != ':' || s[1] == '\0')
					usage();

				if (addoption(option, s + 1) < 0)
					usage();
			}
			break;
		case 'p':
			dhcp_ping = 1;
			break;
		case 'P':
			request_prefix = 1;
			break;
		case 'q':
			max_sessions = atol(optarg);
			break;
		case 'r':
			send_release = 1;
			break;
		case 'R':
			retransmit = atol(optarg);
			break;
		case 't':
			timeout = 1000*atol(optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		case 'w':
			renew_lease =1;
			break;
		case '?':
		default:
                     usage();
		}
	}

	if (send_info_request && (renew_lease || send_decline || send_release)) {
		fprintf(stderr, "Information-Request doesn't grab IP address\n");
		usage();
	}
	/* require server IP address and lease file */
	if (servers == NULL){
		fprintf(stderr, "No servers defined. Using FF05::1:3\n");
		add_servers("ff05::1:3");
		//usage();
	}
}

int add_servers(const char *s)
{
	const char *p1 = s;
	dhcp_server_t *sp = NULL;

	char buffer[INET6_ADDRSTRLEN];
	int len;

	assert(s != NULL);
	while (*p1 != '\0'){
		while (*p1 != ',' && *p1 != '\0')
			p1++;
		len = p1 - s;
		if (len == 0 || len + 1 > sizeof(buffer))
			return(-1);
		strncpy(buffer, s, len);
		s = p1+1;
		buffer[len]='\0';

		if ( *p1 != '\0')
			p1++;

		sp = malloc(sizeof(dhcp_server_t));
		assert(sp);
		memset(sp,'\0', sizeof(dhcp_server_t));
		sp->stats.advertise_latency_min=10000000;
		sp->stats.reply_latency_min=10000000;
		sp->sa.sin6_port = htons(DHCP6_SERVER_PORT);
		sp->sa.sin6_family = AF_INET6;
		if (inet_pton(AF_INET6, buffer, &sp->sa.sin6_addr) == 0){
			fprintf(stderr,"Invalid address %s\n", buffer);
			return(-1);
		}
		sp->next = servers;
		servers = sp;
		num_servers++;
	}
	return(0);
}

void usage()
{
	fprintf(stderr,
"Usage: dras6 -i <server IP> [-f <input-lease-file>] [-o <output-lease-file>]\n"
"	[-A] -O <dec option-no>:<hex data>] [-l <logfile>] [-t <timeout>]\n"
"	[-n <number requests>] [-q <max outstanding> [-R <retransmits>]\n"
"	[-d <delay>] [-c <relay agent IP>] [-e|N|p|r|w] [-I <requested options>]\n\n");

	fprintf(stderr,
"	-a Starting MAC address\n"
"	-A Simulate relay agent\n"
"	-d Delay before sending next packet\n"
"	-D DNS zone for CLIENT_FQDN option\n"
"	-e Send decline after ACK received\n"
"	-f Input file with ClientID/IPv6 leases\n"
"	-g \"option_no1 option_no2 option_no3 ...\"\n"
"	-h Add CLIENT_FQDN option\n"
"	-H Add CLIENT_FQDN optionwith random hostnames\n"
"	-i Server IP Address (multiple servers are separated by commas)\n"
"	-I List of option numbers to request (separated by commas or semicolons)\n"
"	-l Output logfile (default: stderr)\n"
"	-m Start at MAC 0\n"
"	-n Number of requests\n"
"	-N Retransmit until answer received\n"
"	-o Output lease file\n"
"	-O Send option <option-no> (dec) with data <hex data> (w/o length)\n"
"	-p Ping mode: end after ADVERTISE received\n"
"	-q Maximum outstanding requests\n"
"	-r Send release after ACK received\n"
"	-R Number of retransmits (default 0)\n"
"	-t Timeout on requests (ms)\n"
"	-v Verbose output\n"
"	-w Renew mode\n\n");

	exit(1);
}

void print_packet(uint32_t packet_len, struct dhcpv6_packet *p, const char *prefix)
{
	__const char *typestrings[] = {"SOLICIT", "ADVERTISE", "REQUEST",
		"CONFIRM", "RENEW", "REBIND", "REPLY", "RELEASE", "DECLINE",
		"RECONFIGURE", "INFORMATION_REQUEST", "RELAY_FORW", "RELAY_REPL",
		"LEASEQUERY", "LEASEQUERY_REPLY"};

	__const char *optionstrings[] = { "CLIENTID", "SERVERID", "IA_NA",
		"IA_TA", "IAADDR", "ORO", "PREFERENCE", "ELAPSED_TIME",
		"RELAY_MSG", "!UNASSIGNED!", "AUTH", "UNICAST", "STATUS_CODE",
		"RAPID_COMMIT", "USER_CLASS", "VENDOR_CLASS", "VENDOR_OPTS",
		"INTERFACE_ID", "RECONF_MSG", "RECONF_ACCEPT", "SIP_SERVERS_DNS",
		"SIP_SERVERS_ADDR", "NAME_SERVERS", "DOMAIN_SEARCH", "IA_PD",
		"IAPREFIX", "NIS_SERVERS", "NISP_SERVERS", "NIS_DOMAIN_NAME",
		"NISP_DOMAIN_NAME", "SNTP_SERVERS", "INFORMATION_REFRESH_TIME",
		"BCMCS_SERVER_D", "BCMCS_SERVER_A", "!UNASSIGNED!", "GEOCONF_CIVIC",
		"REMOTE_ID", "SUBSCRIBER_ID", "CLIENT_FQDN", "PANA_AGENT",
		"NEW_POSIX_TIMEZONE", "NEW_TZDB_TIMEZONE", "ERO", "LQ_QUERY",
		"CLIENT_DATA", "CLT_TIME", "LQ_RELAY_DATA", "LQ_CLIENT_LINK" };
	int i = 0, option_len = 0, suboption_len = 0;
	uint16_t option_code = 0, suboption_code = 0;

	fprintf(logfp, "%sPacket Type %d (%s), len = %d, ", prefix, p->msg_type, typestrings[p->msg_type-1], packet_len);
	fprintf(logfp, "txn: [%d %d %d]\n", p->transaction_id[0], p->transaction_id[1], p->transaction_id[2]);

	unsigned char *option = p->options;
	while (option < p->options + packet_len - 4)
	{
		option_code = ntohs(*((uint16_t *) option));
		assert(option_code <= sizeof(optionstrings));

		option_len = ntohs(*((uint16_t *) (option + 2)));

		fprintf(logfp, "   Option %d (%s), len = %d [", option_code, optionstrings[option_code-1], option_len);
		for (i = 0; i < option_len; i++)
			fprintf(logfp, " %d", option[i + 4]);
		fprintf(logfp, " ]\n");

		/* expand IA_NA/IA_TA/IA_PD options */
		if (option_code == D6O_IA_NA || option_code == D6O_IA_TA || option_code == D6O_IA_PD) {
			unsigned char *suboption = option + 16;
			//unsigned char *suboption = option + 8;
			while (suboption < option + option_len + 4) {
				suboption_code = ntohs(*((uint16_t *) suboption));
				suboption_len = ntohs(*((uint16_t *) (suboption + 2)));
				/* TODO: Why do we get IA_NA/IA_TA/IA_PD suboptions with 4*x padding? */
				if (!suboption_code) {
					suboption += 4;
					continue;
				}
				fprintf(logfp, "      Option %d (%s), len = %d [", suboption_code, optionstrings[suboption_code-1], suboption_len);
				for (i = 0; i < suboption_len; i++)
					fprintf(logfp, " %d", suboption[i + 4]);
				fprintf(logfp, " ]\n");

				suboption += suboption_len + 4;
			}
		}

		/* option size + option-len size */
		option += option_len + 4;
	}
	fprintf(logfp, "\n");
}

#define MAX_CLIENTID_LEN 100
uint32_t read_lease_data(lease_data_t **leases)
{
	char		buf[128];
	//char		hostname[64];
	int		ret, i;
	int		ntokes;
	char		*tokes[MAX_TOKENS];
	int		temp[6];
	uint8_t		mac[6];
	uint32_t	read_count = 0;
	uint32_t	iaid=0;
	struct in6_addr	ipaddr;
	uint8_t		a,b;
	int		lineno = 0;
	lease_data_t	*lease = *leases;
	FILE		*fpin;

        fpin = fopen(input_file, "r");
	if (fpin == NULL){
		fprintf(logfp,"Could not open input file: %s\n",input_file);
		return(0);
	}
	while ( fgets(buf, sizeof(buf), fpin) != NULL){
		lineno++;
		if ((ntokes = get_tokens(buf, tokes, MAX_TOKENS)) < 3){
			fprintf(logfp, "Too few tokens: %d, line %d\n", ntokes,lineno);
			continue;
		}

           ret = sscanf(tokes[0], "%2x:%2x:%2x:%2x:%2x:%2x", temp,
                                 temp + 1, temp + 2, temp + 3, temp + 4, temp + 5);
           if (ret < 6) {
                  fprintf(logfp, "Line %d, MAC format error: %s\n", lineno, tokes[0]);
                  continue;
           }
           for (i = 0; i < 6; i++)
                  mac[i] = (char) temp[i];

	   ret = sscanf(tokes[1], "%u", &iaid);
           if (ret < 1) {
                  fprintf(logfp, "Line %d, IAID format error: %s\n", lineno, tokes[1]);
                  continue;
           }

           if (*tokes[2] == '\0' || inet_pton(AF_INET6, tokes[2], &ipaddr) == 0) {
                  if (strncmp(tokes[2], "SOLIC", 5) == 0)
                         ipaddr = in6addr_any;
                  else {
                         fprintf(logfp, "Line %d, format error: %s\n", lineno, tokes[2]);
                         continue;
                  }
           }

           if (*leases == NULL) {
                  *leases = calloc(1, sizeof(lease_data_t));
                  lease = *leases;
           }
           else {
                  lease->next = calloc(1, sizeof(lease_data_t));
                  lease = lease->next;
           }
           assert(lease != NULL);

           lease->ipaddr = ipaddr;
           lease->iaid = iaid;
           memcpy(lease->mac, mac, 6);
           lease->next = NULL;

	   if (ntokes > 3){
	          for (i=0; i < DUID_LLT_LEN; i++){
			  a = *(tokes[3] + 2*i);
			  b = *(tokes[3] + 2*i +1);
			  if ( a >= '0' && a <= '9')
				a = a - '0';
			  else 
				a = 10 + a - 'a';

			  if ( b >= '0' && b <= '9')
				b = b - '0';
			  else 
				b = 10 + b - 'a';
		          lease->serverid[i] = a * 16 + b;
		  }
    	   }
	   else 
		lease->serverid[0] = 0xff;

           if (ntokes > 4 && *tokes[4] != '-')
                  lease->hostname = strdup(tokes[4]);
           else
                  lease->hostname = NULL;

	    read_count++;
	}
	return(read_count);
}

struct in6_addr get_local_addr(void)
{

	int		lsock;
	struct		sockaddr_in6 ca;
	socklen_t	calen = sizeof(ca);
	char		ip6str[272];


	if ((lsock=socket(AF_INET6, SOCK_DGRAM, 0)) < 0 ){
		perror("socket:");
		exit(1);
	}
	if (servers == NULL){
		fprintf(logfp,"get_local_addr: servers list is NULL\n");
		exit(1);
	}
	if ( connect(lsock, (const  struct sockaddr *) &servers->sa,
						sizeof(servers->sa)) < 0 ){
		perror("get_local_addr: Connect");
		return(in6addr_any);
	}
	getsockname(lsock, (struct sockaddr *) &ca, &calen);
	close(lsock);
	fprintf(stderr, "get_local_addr: Using source address: %s\n", inet_ntop(AF_INET6, &ca.sin6_addr,
				ip6str, sizeof(ip6str)));
	return(ca.sin6_addr);
}


dhcp_session_t *find_free_session( dhcp_server_t *s)
{
	int		i;
	dhcp_session_t	*list = s->list;
	if ( s->active == max_sessions)
		return(NULL);
	for (i=0; i < max_sessions; i++){
		if  (list[i].state == UNALLOCATED){
			list[i].state = SESSION_ALLOCATED;
			s->active++;
			return(list+i);
		}
	}
	return(NULL);
}

void reader(void)
{
	static uint8_t		buffer[1024];
	ssize_t			packet_length;
	int			pollto=20;
	struct timeval		timestamp;
	struct pollfd		fds={sock,POLLIN,0};


	while (poll(&fds, 1, pollto) >  0){
		packet_length = recv(sock, buffer, sizeof(buffer), 0);
#ifdef linux
		ioctl(sock, SIOCGSTAMP, &timestamp);
#else
		gettimeofday(&timestamp, NULL);
#endif
		if (packet_length < 0 ){
			fprintf(logfp, "Packet receive error\n");
			return;
		}
		pollto = 0;
		//printf("Packet length %d\n", packet_length);
		process_packet(buffer, &timestamp, packet_length);
	}
	if (fds.revents & (POLLERR|POLLNVAL)) {
		fprintf(stderr,"reader: poll socket error\n");
	}
	return;
}

int log_open(void)
{
        if ( logfile == NULL )
                logfp = stderr;
	else {
        	logfp = fopen(logfile, "w");
		if ( logfp == NULL){
			fprintf(stderr,"Open %s failed\n", logfile);
			exit(1);
		}
		setvbuf(logfp, NULL, _IONBF, 0);
	}
        fprintf(logfp,"Begin: Version %s\n", version);
        return(0);
}

int
addoption(int option_no, char *hexdata)
{
    int i;
    int datalen = strlen(hexdata);
    char *data;
    struct option_st *opt;

    if (datalen % 2 != 0) {
	fprintf(stderr, "Odd number of hex chars\n");
	return -1;
    }

    datalen /= 2;
    data = malloc(datalen);

    if (strspn(hexdata, "0123456789abcdefABCDEF") != datalen * 2
	|| datalen > 255)
    {
	fprintf(stderr, "Illegal hex chars or too long (max 255)\n");
	return -1;
    }

    nextraoptions++;

    extraoptions = realloc(extraoptions,
			   nextraoptions * sizeof(struct option_st *));

    opt = malloc(sizeof(struct option_st));

    extraoptions[nextraoptions - 1] = opt;

    opt->option_no = option_no;
    opt->data_len = datalen;
    opt->data = data;

    for (i = 0; hexdata[i]; i += 2) {
	int x;

	sscanf(hexdata + i, "%2x", &x);

	*data++ = (char)(x & 0377);
    }

    return 0;
}
void getmac(uint8_t * s)
{
    //static uint8_t mac[6];
    int carry = 5;
    int i1, i2;

    if (use_sequential_mac) {
           if (s)
                  memcpy(s, firstmac, 6);
           while (carry >= 0) {
                  if (firstmac[carry] == 0xFF)
                         firstmac[carry--] = 0;
                  else {
                         firstmac[carry]++;
                         return;
                  }
           }
           return;
    }
    if (s) {
           i1 = rand();
           i2 = rand();
           memcpy(s, &i1, 4);
           memcpy(s + 4, &i2, 2);
    }
}
int get_tokens(char *cp, char **tokes, int maxtokes)
{
    int n = 0;
    if (cp == NULL)
           return (0);
    /* Eat any leading whitespace */
    while (*cp == ' ' && cp != '\0')
           cp++;

    if (*cp == '\0')
           return (0);
    tokes[0] = cp;
    while (*cp != '\0' && *cp != '\n') {
           if (*cp == ' ' || *cp == '\t') {
                  *(cp++) = '\0';
                  while (*cp != '\0' && (*cp == ' ' || *cp == '\t'))
                         cp++;
                  if (++n < maxtokes)
                         tokes[n] = cp;
                  else
                         return (0);
           }
           else
                  cp++;
    }
    *cp = '\0';
    return (n + 1);
}
void print_lease( FILE *fp, dhcp_session_t *session)
{

	int i;

        static char addr[INET6_ADDRSTRLEN];
        fprintf(fp,
                "%02x:%02x:%02x:%02x:%02x:%02x %u %s ",
                session->mac[0], session->mac[1],
                session->mac[2], session->mac[3],
                session->mac[4], session->mac[5], session->iaid,
		inet_ntop(AF_INET6, &session->ipaddr, addr, INET6_ADDRSTRLEN));
	if (session->serverid[0] != 0xff){
		for (i=0; i < DUID_LLT_LEN; i++)
			fprintf(outfp, "%02x", session->serverid[i]);
	}
	if (session->hostname[0])
		fprintf(outfp, " %s\n", session->hostname);
	else 
		fprintf(outfp, " -\n");
			
                
}
/* I guess this can be an FQDN or unqualified hostname */
int pack_client_fqdn(uint8_t *options, char *hostname)
{
	int offset=0;
	*((uint16_t *)(options + offset)) = htons(D6O_CLIENT_FQDN);
	offset += 4;
	options[offset] = 1; //S=1,O=0,N=0
	offset++;
	offset += encode_domain(hostname, options + offset);
	*((uint16_t *)(options + 2)) = htons(offset - 4);
	return(offset);
}
int encode_domain( char *domain, uint8_t *buf)
{
        uint8_t *plen;
        int len=1;

        plen = buf;
        buf++;
        *plen = 0;
        while (*domain != '\0'){
                if (*domain == '.'){
                        if (*(domain + 1) == '\0')
                                break;
                        else {
                                plen = buf++;
                                *plen = 0;
                                domain++;
                                len++;
                                continue;
                        }
                }
                *(buf++) = *(domain++);
                (*plen)++;
                len++;
        }
        *buf=0;
        return(len+1);
}
