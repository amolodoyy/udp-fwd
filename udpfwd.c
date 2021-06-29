#define _GNU_SOURCE 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <signal.h>
#include <netdb.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/types.h>
#define ERR(source) (perror(source),\
		fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		exit(EXIT_FAILURE))
#define BACKLOG 3
#define BUF_SIZE 300
#define MAX_UDP_BUF 1024
#define ADDR_NUM 10
#define MAX_UDP_SOCKETS 10
volatile sig_atomic_t last_signal = 0;

void sigint_handler(int sig) { 
	last_signal = sig;
}

int sethandler( void (*f)(int), int sigNo) {
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = f;
	if (-1==sigaction(sigNo, &act, NULL))
		return -1;
	return 0;
}

// for udp connections

struct sockaddr_in make_address(char *address, char *port){
	int ret;
	struct sockaddr_in addr;
	struct addrinfo *result;
	struct addrinfo hints = {};
	hints.ai_family = AF_INET;
	if((ret=getaddrinfo(address,port, &hints, &result))){
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
		exit(EXIT_FAILURE);
	}
	addr = *(struct sockaddr_in *)(result->ai_addr);
	freeaddrinfo(result);
	return addr;
}

void closeSpecificPort(int port, int sockets[], int ports[])
{
	int index = 0;
	for(; index < MAX_UDP_SOCKETS; index++)
	{
		if(port == ports[index])
			break;
	}
	int tmp = ports[index];
	ports[index] = 0;
	if(TEMP_FAILURE_RETRY(close(sockets[index]))<0) ERR("close"); printf("Port %d of fd %d was closed.\n", tmp, sockets[index]);
	sockets[index] = 0;
}

int RetrievePortLFromFwd(char* buf)
{
	char udpport[6];
	int i = 0;
	for(; buf[i] != ' '; i++)
		;
	i++;
	int j = 0;
	for(; buf[i] != ' ' && buf[i] != '\n'; i++, j++)
	{
		udpport[j] = buf[i];
	}
	udpport[++j] = '\0';
	return atoi(udpport);
}
int CountSpaces(char* buf)
{
	int counter;
	for(int i = 0; buf[i] != '\0'; i++)
	{
		if(buf[i]==' ')
			{counter++;}
	}

	return counter;
}
void usage(char * name){
	fprintf(stderr,"USAGE: %s port\n",name);
}

int make_socket(int domain, int type){
	int sock;
	sock = socket(domain,type,0);
	if(sock < 0) ERR("socket");
	return sock;
}
int bind_inet_socket(uint16_t port,int type){
	struct sockaddr_in addr;
	int socketfd,t=1;
	socketfd = make_socket(PF_INET,type);
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR,&t, sizeof(t))) ERR("setsockopt");
	if(bind(socketfd,(struct sockaddr*) &addr,sizeof(addr)) < 0)  ERR("bind");
	if(SOCK_STREAM==type)
		if(listen(socketfd, BACKLOG) < 0) ERR("listen");
	return socketfd;
}

int add_new_client(int sfd){

	if((sfd=TEMP_FAILURE_RETRY(accept(sfd,NULL,NULL)))<0) {
		if(EAGAIN==errno||EWOULDBLOCK==errno) return -1;
		ERR("accept");
	}
	return sfd;
}
ssize_t bulk_read(int fd, char *buf, size_t count){
	int c;
	size_t len=0;
	do{
		c=TEMP_FAILURE_RETRY(read(fd,buf,count));
		if(c<0) return c;
		if(0==c) return len;
		buf+=c;
		len+=c;
		count-=c;
	}while(count>0);
	return len ;
}
ssize_t bulk_write(int fd, char *buf, size_t count){
	int c;
	size_t len=0;
	do{
		c=TEMP_FAILURE_RETRY(write(fd,buf,count));
		if(c<0) return c;
		buf+=c;
		len+=c;
		count-=c;
	}while(count>0);
	return len ;
}
void communicate(int fd, int flag)
{
	if(flag == 1)
	{
		char* hMes = "Hello!\n";
		if(bulk_write(fd, hMes, 8) < 0) ERR("write");
	}
	else
	{
		char* goodbye = "Sorry, can't connect to server, too much connections.";
		if(bulk_write(fd,goodbye,strlen(goodbye))<0) ERR("write");
		if(TEMP_FAILURE_RETRY(close(fd))<0)ERR("close");	
	}
}

// buffer = "fwd 123 122.122.122.122:45"
void doUDPsubServer(int fd, char* buffer, int size, struct sockaddr_in client_address[])
{
	char message[MAX_UDP_BUF];
	int c=0;

	// completing client_addresses
	int index = 4;
	while(buffer[index] != ' ')
	{
		index++;
	}
	index++;
	int i = index;
	for(; buffer[i] != '\n' && buffer[i] != '\0'; index=(i-index)+1)
	{
		int j = 0;
		char IP[16];
		char port[6];
		while(buffer[i] != ':')
		{
			IP[j] = buffer[i];
			i++; j++;
		}
		IP[j] = '\0'; j = 0; i++;
		printf("IP=%s\n",IP);
		while(buffer[i] != ' ' && buffer[i] != '\n' && buffer[i] != '\0')
		{
			port[j] = buffer[i];
			i++; j++;
		}
		port[j] = '\0';
		i++;
		printf("PORT=%s\n", port);

		client_address[c].sin_addr.s_addr = inet_addr((const char*)IP);
		client_address[c].sin_family = AF_INET;
		client_address[c].sin_port = htons(atoi(port));
		c++;
	}

		if(fork() == 0)
		{
		while(1)
		{
			int n;
			if((n = recv(fd,message,MAX_UDP_BUF,MSG_WAITALL)) < 0) 
			{
				if(errno==EINTR) break;
				else ERR("recv");
			}
			message[n] = '\0';
			for(int it = 0; it < c; it++)
			{
				int len = sizeof(client_address[it]);
				if(TEMP_FAILURE_RETRY(sendto(fd,message,n,MSG_CONFIRM,(const struct sockaddr*)&(client_address[it]),len)) < 0) ERR("sendto");
				
			}
		}
	}
}
void doServer(int fdTCP, int child_sockets[3])
{
	int UDPports[MAX_UDP_SOCKETS] = {0,0,0,0,0,0,0,0,0,0};
	int UDPsockets[MAX_UDP_SOCKETS] = {0,0,0,0,0,0,0,0,0,0};
	int sock_counter = 0;
	char buffer[BUF_SIZE];
	int readvalue;
	int flag = 0;
	int curFD;
	int new_socket;
	fd_set rfd;
	struct sockaddr_in client_address[ADDR_NUM];
	int max_socket = fdTCP;
    while(1)
    {
		FD_ZERO(&rfd);
		FD_SET(fdTCP, &rfd);
		// add child sockets to set
		for(int i = 0; i < 3; i++)
		{
			curFD = child_sockets[i];
			if(curFD > 0)
				FD_SET(curFD, &rfd);
			
			if(curFD > max_socket)
				max_socket = curFD;
		}
        if(select(max_socket+1, &rfd, NULL, NULL, NULL) > 0)
        {
			// checking actions on main server socket
			if(FD_ISSET(fdTCP, &rfd))
			{
            	new_socket = add_new_client(fdTCP);
				// add socket to our list of connections
				for(int i = 0; i < 3; i++)
				{
					if(child_sockets[i] == 0)
					{
						child_sockets[i] = new_socket;
						flag = 1;
						break;
					}
				}
				// if success, flag = 1, connect and hold, of flag = 0, send goodbye message and close connection
				communicate(new_socket, flag);
				flag = 0;
			}
			// checking actions on child sockets
			for(int i = 0; i < 3; i++)
			{
				curFD = child_sockets[i];
				if(FD_ISSET(curFD, &rfd))
				{
					// check closing
					if((readvalue = read(curFD, buffer, BUF_SIZE)) == 0)
					{
						printf("Some host disconnected.\n");
						close(curFD);
						child_sockets[i] = 0;
					}
					else if(readvalue > 0) // some message received
					{
						
						// message has form fwd <portL> <IP1:port1> <IP2:port2>
						if(!strncmp("fwd", buffer, 3))
						{
							
								
							if(sock_counter >= MAX_UDP_SOCKETS){
								printf("Too much rules. Command rejected.\n");
								continue;
							}
							// fwd message received, parse arguments
							printf("fwd command from tcp client caught.\n");
							buffer[readvalue] = '\0';
							printf("Received command is: %s\n", buffer);
							int fd;
							int portL = RetrievePortLFromFwd(buffer);
							printf("Port, opened for listening is %d\n", portL);
							// creating udp socket for incoming messages to portL
							int new_flags;
							new_flags = fcntl(fdTCP, F_GETFL) | O_NONBLOCK;
							fcntl(fdTCP, F_SETFL, new_flags);
							fd = bind_inet_socket(portL, SOCK_DGRAM);
							for(int i = 0; i < MAX_UDP_SOCKETS; i++)
							{
								if(UDPsockets[i] == 0)
								{
									UDPsockets[i] = fd;
									printf("fd %d added\n", UDPsockets[i]);
									break;
								}
							}
							for(int i = 0; i < MAX_UDP_SOCKETS; i++)
							{
								if(UDPports[i] == 0)
								{
									UDPports[i] = portL;
									printf("port %d added\n", UDPports[i]);
									break;
								}
							}
							sock_counter++;
							doUDPsubServer(fd,buffer,readvalue,client_address);

							
						}
						if(!strncmp("close", buffer,5))
						{
							printf("close command caught.\n");
							for(int i = 0; i < MAX_UDP_SOCKETS; i++)
								printf("%d ", UDPsockets[i]);
							printf("\n");
							int portToDelete = RetrievePortLFromFwd(buffer);
							printf("portToDelete=%d\n", portToDelete);
							closeSpecificPort(portToDelete, UDPsockets, UDPports);
						}
					
					}
				}
			}
        }
		else if(EINTR == errno && last_signal == SIGINT)
		{
			printf("\nSIGINT caught, terminating...\n");
			for(int i = 0; i < 3; i++)
			{
				if(child_sockets[i]>0)
				{
					if(TEMP_FAILURE_RETRY(close(child_sockets[i]))<0)ERR("close");
				}
			}
			if(TEMP_FAILURE_RETRY(close(fdTCP))<0)ERR("close");
			break;
		}
		else ERR("select");
		if(last_signal == SIGINT) break;
		
    }
}
int main(int argc, char** argv)
{
	pid_t waitpid;
	int child_status = 0;
	int connections[3] = {0,0,0};
    if(argc != 2)
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
	if(sethandler(sigint_handler,SIGINT)) ERR("Seting SIGINT:");
    int fdTCP;
    int new_flags;
    fdTCP = bind_inet_socket(atoi(argv[1]), SOCK_STREAM);
	new_flags = fcntl(fdTCP, F_GETFL) | O_NONBLOCK;
	fcntl(fdTCP, F_SETFL, new_flags);
    doServer(fdTCP, connections);
	while((waitpid = wait(&child_status)) > 0);
    return EXIT_SUCCESS;
}

