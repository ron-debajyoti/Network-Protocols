#include <stdio.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h> /// this is the header file for the ICMP structure
#include <netdb.h>
#include <setjmp.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define PACKET_SIZE     4096
#define MAX_WAIT_TIME   10
#define MAX_NO_PACKETS  8

char sendpacket[PACKET_SIZE];
char recvpacket[PACKET_SIZE];
int sockfd, datalen = 56;
int nsend = 0, nreceived = 0; 		/// nsend is the sequence number
struct sockaddr_in dest_addr;
pid_t pid;												// id of process or the packet in this case 
struct sockaddr_in from;					// sockaddr id 
struct timeval tvrecv;						// struct timeval 


void statistics(int signo);
unsigned short cal_chksum(unsigned short *addr, int len);
int pack(int pack_no);
void send_packet(void);
void recv_packet(void);
int unpack(char *buf, int len);
void tv_sub(struct timeval *out, struct timeval *in);

void statistics(int signo)
{
	// this fucntion tells the statistics of each signal number
    printf("\n--------------------PING statistics-------------------\n");
    printf("%d packets transmitted, %d received , %%%d lost\n", nsend,
        nreceived, (nsend - nreceived) / nsend * 100);
    close(sockfd); // closing the socket descriptor 
    exit(1);
} 



unsigned short cal_chksum(unsigned short *addr, int len)
{
    int nleft = len;
    int sum = 0;
    unsigned short *w = addr;
    unsigned short answer = 0;
    while (nleft > 1)
    {
        sum +=  *w++;
        nleft -= 2;
    }

    if (nleft == 1)
    {
        *(unsigned char*)(&answer) = *(unsigned char*)w;
        sum += answer;
    }
    sum = (sum >> 16) + (sum &0xffff);
    sum += (sum >> 16);
    answer = ~sum;
    return answer;
}





int pack(int pack_no)
{
    int i, packsize;
    // ICMP packet structure 
    struct icmp *icmp;
    /*
			u_char	icmp_type;		// type of message, see below 
			u_char	icmp_code;		// type sub code 
			u_short	icmp_cksum;		// ones complement cksum of struct 
			union 
			{
				u_char ih_pptr;			// ICMP_PARAMPROB 
				struct in_addr ih_gwaddr;	/* ICMP_REDIRECT 
				struct ih_idseq {
					n_short	icd_id;
					n_short	icd_seq;
				} ih_idseq;
				int ih_void;
    */
    struct timeval *tval;			 // structure for time val 
    icmp = (struct icmp*)sendpacket;
    icmp->icmp_type = ICMP_ECHO;  // ICMP_ECHO = 8 indicates an echo request
    icmp->icmp_code = 0; /* the next two parameters are initialized to 0 **/
    icmp->icmp_cksum = 0;
    icmp->icmp_seq = pack_no; // the sequence no is the packet number
    icmp->icmp_id = pid;  // pid becomes the packet id 
    packsize = 8+datalen; // header
    tval = (struct timeval*)icmp->icmp_data;
    gettimeofday(tval, NULL); 
    icmp->icmp_cksum = cal_chksum((unsigned short*)icmp, packsize); // calculating checksum
    return packsize;
}





void send_packet()
{
    int packetsize;
    while (nsend < MAX_NO_PACKETS)
    {
        nsend++;
        packetsize = pack(nsend); 
        if (sendto(sockfd, sendpacket, packetsize, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0)
        {
            perror("sendto error");
            continue;
        } sleep(1); 
    }
}





void recv_packet()
{
    int n, fromlen;
    extern int errno;
    signal(SIGALRM, statistics);
    fromlen = sizeof(from);
    while (nreceived < nsend)
    {
        alarm(MAX_WAIT_TIME);
        if ((n = recvfrom(sockfd, recvpacket, sizeof(recvpacket), 0, (struct
            sockaddr*) &from, &fromlen)) < 0)
        {
            if (errno == EINTR)
                continue;
            perror("recvfrom error");
            continue;
        } gettimeofday(&tvrecv, NULL); 
        if (unpack(recvpacket, n) ==  - 1)
            continue;
        nreceived++;
    }
}


int unpack(char *buf, int len)
{
    int i, iphdrlen;
    struct ip *ip;
    struct icmp *icmp;
    struct timeval *tvsend;
    double rtt; // round trip time 
    ip = (struct ip*)buf;
    iphdrlen = ip->ip_hl << 2; 
    icmp = (struct icmp*)(buf + iphdrlen);
    len -= iphdrlen; 

    if (len < 8)
    {
        printf("ICMP packets\'s length is less than 8\n");
        return  - 1;
    } 

    if ((icmp->icmp_type == ICMP_ECHOREPLY) && (icmp->icmp_id == pid))
    {
        tvsend = (struct timeval*)icmp->icmp_data;
        tv_sub(&tvrecv, tvsend); 
        rtt = tvrecv.tv_sec * 1000+tvrecv.tv_usec / 1000;
        printf("%d byte from %s: icmp_seq=%u ttl=%d rtt=%.3f ms\n", len, inet_ntoa(from.sin_addr), icmp->icmp_seq, ip->ip_ttl, rtt);
    }
    else
        return  - 1;
}



int main(int argc, char *argv[])
{
    struct hostent *host;//this data type is used to represent an entry in the hosts database
    struct protoent *protocol;//contains the name and protocol numbers that correspond to a given protocol name
    unsigned long inaddr = 0l;//defines 0 to be of long data type upon appending long
    int waittime = MAX_WAIT_TIME;//initialised as 10
    int size = 50 * 1024;
    if (argc < 2)//if the number of parameters is less than 2
    {
        printf("usage:%s hostname/IP address\n", argv[0]);
        exit(1);
    } 

    if ((protocol = getprotobyname("icmp")) == NULL)//if there's no protoent with the name "icmp"
    {
        perror("getprotobyname");
        exit(1);
    }//otherwise returns the protoent structure of "icmp" 
    //p_proto is the protocol number
    //af_inet referes to addresses from the internet
    if ((sockfd = socket(AF_INET, SOCK_RAW, protocol->p_proto)) < 0)//create raw socket,make sockfd as socket file descriptor
    {
        perror("socket error");
        exit(1);
    }
    //getuid returns the real user ID of the calling process
    //setuid sets the effective user ID of the current process
    //setuid is used to elevate the privilege and here ping command must send and listen
    //for control packets on the network interface

    setuid(getuid());
    
    //so_rcvbuf sets the maximum socket receive buffer in bytes and is assigned to size
    //sockfd is the socket file descriptor
    //sol_socket is used to set the options at socket level
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
    //this function places sizeof(dest_addr) zero valued bytes in the area pointed to by dest_addr
    bzero(&dest_addr, sizeof(dest_addr));
    //af_inet indicates address from internet
    dest_addr.sin_family = AF_INET;


    //if input is invalid inet_addr returns inaddr_none
    //inet_addr converts argv[1] to standard ipv4 decimal notation
    if (inaddr = inet_addr(argv[1]) == INADDR_NONE)
    {
        if ((host = gethostbyname(argv[1])) == NULL)//returns hostent type struct for given hostname
        {
            perror("gethostbyname error");
            exit(1);
        }
        //otherwise copy h_addr into sin_addr
        memcpy((char*) &dest_addr.sin_addr, host->h_addr, host->h_length);
    }
    else
        dest_addr.sin_addr.s_addr = inet_addr(argv[1]);//if valid return at inet_addr
    pid = getpid();//returns the process id of the calling process
    //inet_ntoa converts network byte order to dotted decimal formal
    printf("PING %s(%s): %d bytes data in ICMP packets.\n", argv[1], inet_ntoa(dest_addr.sin_addr), datalen);
    send_packet(); 
    recv_packet();
    statistics(SIGALRM); 
    return 0;
}



void tv_sub(struct timeval *out, struct timeval *in)
{
    if ((out->tv_usec -= in->tv_usec) < 0)
    {
        --out->tv_sec;
        out->tv_usec += 1000000;
    } out->tv_sec -= in->tv_sec;
}