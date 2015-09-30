#ifndef __CLIENTSERVER_H
#define __CLIENTSERVER_H
#include "stats.h"

// a convenience struct that contains the socket and remote address for a connection
typedef struct {
	int sockfd;
	struct sockaddr_in theiraddr;
} conn_t;

typedef int bool;
#define true 1
#define false 0

conn_t* connect_tcp(const char* serverIP, const char* serverPort);
void disconnect_tcp(conn_t* connxn);
conn_t* listen_tcp(const char* listenPort);
conn_t* accept_tcp(conn_t* listenconn);
int recv_tcp(conn_t* connxn, char* buffer, unsigned int bufSize);
int send_tcp(conn_t* connxn, char* buffer, unsigned int bytesToSend);
int recvData(conn_t* connxn, char* buffer, unsigned int bufSize, connxnstats_t* stats);
int sendData(conn_t* connxn, char* buffer, unsigned int bytesToSend, connxnstats_t* stats);

#endif
