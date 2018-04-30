#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <bits/stdc++.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <openssl/md5.h>

using namespace std;

#define BUFSIZE 1024

sem_t full,empty;
pthread_mutex_t mutex;
int N=8;
struct sockaddr_in clientaddr;
int clientlen,serverlen;

//datagram structure
typedef struct data{
  int seq,len,type;//0-ack,1-data
  char buf[BUFSIZE];
}datagram;

// buffer structure
typedef struct
{
    std::vector<datagram> array;
    FILE* fd;
    int nop;
}buffer;

// the window for go back and arq
typedef struct
{
    int curr,base,cwnd,rwnd,windsize,ssthresh,state,prev,count,ack,flag;
}window;


void error(char *msg)
{
  perror(msg);
  exit(1);
}

// modular functions some imported from client
// receive function
int  UDP_receive(datagram* d,int sockfd,struct sockaddr * &clientaddr,int clientlen)
{
	int n = recvfrom(sockfd, d, sizeof(datagram), 0,(struct sockaddr *) &clientaddr, (socklen_t*)&clientlen);
	if (n < 0)	error("ERROR in recvfrom");
	return n;
}

// send function
int  UDP_send(datagram* d,int n,int sockfd,struct sockaddr * &clientaddr,int clientlen)
{
	int n1 = sendto(sockfd, d, sizeof(*d), 0,(struct sockaddr *) &clientaddr, clientlen);
	if (n1 < 0)  error("ERROR in sendto");
}

// creating the packet
void create_packet(datagram *d,char* buf,int i,int j,int k)
{
    bzero(d->buf, BUFSIZE);
    d->seq=i;
    d->type=j;
    if(k==-1) d->len=strlen(buf);
    else d->len=k;
    memcpy(d->buf,buf,d->len);
}

// writing from buffer into file
void * mbuf_func(void * ptr)
{
    buffer *k=(buffer *)ptr;
    int fs_block_sz;
    char buf[BUFSIZE];
    datagram temp;
    do
    {
    	        sem_wait(&full);
                pthread_mutex_lock(&mutex);
                int write_sz = fwrite(k->array[0].buf,sizeof(char),k->array[0].len, k->fd);
                k->array.erase(k->array.begin());
		k->nop--;
		if(write_sz < k->array[0].len)
		{
			error("File write failed on server.\n");
		}
		pthread_mutex_unlock(&mutex);
        sem_post(&empty);
		if(k->nop==0)
        {
        	fclose(k->fd);
        	break;
        }
    }while(1);


}

// creating file based on details
void server_details(int* sockfd1,char* filename,int* nochunks1,int* size1,char** argv)
{

	int portno,sockfd,size,nochunks;
	struct sockaddr_in serveraddr;
	char buf[BUFSIZE];
	int optval=1,n;
	portno = atoi(argv[1]);
	(sockfd) = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)  error("ERROR opening socket");
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,(const void *)&optval , sizeof(int));
	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short)portno);
	if (bind(sockfd, (struct sockaddr *) &serveraddr,sizeof(serveraddr)) < 0)
                        error("ERROR on binding");
	datagram d;
	bzero(d.buf, BUFSIZE);
	n=UDP_receive(&d,sockfd,(struct sockaddr * &)clientaddr,clientlen);
	printf("server received %d/%d bytes: %s\n", strlen(d.buf), n, d.buf);

	strcpy(filename,d.buf);
	int i;
	for(i=0;filename[i]!='\n';i++);
	filename[i]='\0';

	n=UDP_send(&d,n,sockfd,(struct sockaddr * &)clientaddr,clientlen);

	bzero(d.buf, BUFSIZE);
	n=UDP_receive(&d,sockfd,(struct sockaddr * &)clientaddr,clientlen);
	printf("server received %d/%d bytes: %s\n", strlen(d.buf), n, d.buf);
	char sizename[BUFSIZE];
	strcpy(sizename,d.buf);
	(size) = atoi(sizename);

	n=UDP_send(&d,n,sockfd,(struct sockaddr * &)clientaddr,clientlen);
	bzero(d.buf, BUFSIZE);
	n=UDP_receive(&d,sockfd,(struct sockaddr * &)clientaddr,clientlen);
	printf("server received %d/%d bytes: %s\n", strlen(d.buf), n, d.buf);

	char chunks[BUFSIZE];
	strcpy(chunks,d.buf);
	(nochunks)= atoi(chunks);
	n=UDP_send(&d,n,sockfd,(struct sockaddr * &)clientaddr,clientlen);

	(*sockfd1)=sockfd;
	(*size1)=size;
	(*nochunks1)=nochunks;


}

//receiving
void app_recv(char* filename,int nochunks,float dp,int sockfd,struct sockaddr * &clientaddr,int clientlen)
{
    datagram d;
    char buf[BUFSIZE];
    int n;
    FILE *fr = fopen(filename, "wb");
    if(fr == NULL)
         printf("File %s Cannot be opened file on server.\n", filename);
    buffer recv_buffer;
    pthread_t mbuf;
    sem_init(&full,0,0);
    sem_init(&empty,0,N);
    recv_buffer.fd=fr;
    recv_buffer.nop=nochunks;
    pthread_create(&mbuf,NULL,mbuf_func,(void *)&recv_buffer);
    int count=0;
    int rwnd=0,left=recv_buffer.nop;
    while(count != nochunks )
	{
		bzero(d.buf, BUFSIZE);
		n=UDP_receive(&d,sockfd,(struct sockaddr * &)clientaddr,clientlen);
		float r = ((double) rand() / (RAND_MAX));
		if(d.seq == count)
		{
		        sem_wait(&empty);
	                pthread_mutex_lock(&mutex);
        	        recv_buffer.array.push_back(d);
	    	        rwnd=N-recv_buffer.array.size();
	    	        left=recv_buffer.nop;
    		        pthread_mutex_unlock(&mutex);
	  		sem_post(&full);
	  		count++;
		}

		if(r<=dp)
		{
			bzero(d.buf, BUFSIZE);
		        create_packet(&d,buf,count-1,0,rwnd);
			n=UDP_send(&d,n,sockfd,(struct sockaddr * &)clientaddr,clientlen);
		}

	}
	while(1)
	{
		bzero(d.buf, BUFSIZE);
		n=UDP_receive(&d,sockfd,(struct sockaddr * &)clientaddr,clientlen);
		if(d.seq==-1) return;
		bzero(d.buf, BUFSIZE);
	        create_packet(&d,buf,count-1,0,0);
		n=UDP_send(&d,n,sockfd,(struct sockaddr * &)clientaddr,clientlen);
	}
	pthread_join(mbuf,NULL);
}

// checksum
void server_file_check(char* filename,int sockfd,struct sockaddr * &clientaddr,int clientlen)
{
	datagram d;
	int n,i;
	bzero(d.buf, BUFSIZE);
	FILE *inFile = fopen (filename, "rb");
	MD5_CTX mdContext;
	if (inFile == NULL)
	{
	    printf ("%s can't be opened.\n", filename);
	    return;
	}
	int bytes;
	char data[BUFSIZE];
	unsigned char c[MD5_DIGEST_LENGTH];
	MD5_Init (&mdContext);
	while ((bytes = fread (data, 1, BUFSIZE, inFile)) != 0)
	{
	    MD5_Update (&mdContext, data, bytes);
	}
	MD5_Final (c,&mdContext);
	printf("MD5 check sum is : ");
	for(i = 0; i < MD5_DIGEST_LENGTH; i++) printf("%02x", c[i]);
	printf (" %s\n", filename);
	fclose (inFile);
	n = sendto(sockfd, c, MD5_DIGEST_LENGTH, 0,(struct sockaddr *) &clientaddr,(socklen_t) clientlen);
	if (n < 0)  error("ERROR in sendto");
	return;
}

int main(int argc, char **argv)
{


	int nochunks,size,sockfd;
	char filename[BUFSIZE];
	if (argc != 3)
	{
		fprintf(stderr, "usage: %s <port_for_server> <drop-probability>\n", argv[0]);
		exit(1);
	}
	float dp=atof(argv[2]);
	clientlen = sizeof(clientaddr);
	server_details(&sockfd,filename,&nochunks,&size,argv);
	app_recv(filename,nochunks,dp,sockfd,(struct sockaddr * &)clientaddr,clientlen);
	server_file_check(filename,sockfd,(struct sockaddr * &)clientaddr,clientlen);
	return 0;
}
