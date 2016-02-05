/* Contains all network/sockets-related code, connect, disconnect, send and recv functions; which are used by both server and client */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <assert.h>
#include <sys/time.h>

#include "clientserver.h"


#define BACKLOG					10


// Connect to IP:port, allocate any necessary resources
conn_t* connect_tcp(const char* serverIP, const char* serverPort) {
	struct addrinfo hints;
	struct addrinfo *result = NULL;
	struct sockaddr_in serv_addr; 
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	conn_t* serverconn = (conn_t*)malloc(sizeof(conn_t));
	
	// int status = getaddrinfo(serverIP, serverPort, &hints, &result);
	// if (status != 0) {
	// 	fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
	// 	return NULL;
	// }
	int status = 0;//getaddrinfo(serverIP, serverPort, &hints, &result);
	// if (status != 0) {
	// 	fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
	// 	return NULL;
	// }
	memset(&serv_addr, '0', sizeof(serv_addr)); 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8888); 
	serverconn->sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(inet_pton(AF_INET, serverIP, &serv_addr.sin_addr)<=0)
    {
        printf("\n inet_pton error occured\n");
        return 1;
    } 
	status = connect(serverconn->sockfd, 
		&serv_addr, 
		sizeof serv_addr);

	return serverconn;
}

// Disconnect socket, free any allocated resources
void disconnect_tcp(conn_t* connxn) {
	close(connxn->sockfd);
	free(connxn);
}

// Listen on given port
conn_t* listen_tcp(const char* listenPort) {
	int yes=1;
	struct addrinfo hints;
	struct addrinfo *result = NULL, *p;
	conn_t* listenconn = (conn_t*)malloc(sizeof(conn_t));  // listen on sock_fd, new connection on new_fd

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	
	int status = getaddrinfo(NULL, listenPort, &hints, &result);
	if (status != 0) {
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
		return NULL;
	}
	
	// loop through all the results and bind to the first we can
	for(p = result; p != NULL; p = p->ai_next) {
		if ((listenconn->sockfd = socket(p->ai_family, p->ai_socktype,
							 p->ai_protocol)) == -1) {
			perror("could not create socket with this address, retrying...\n");
			continue;
		}
		
		if (setsockopt(listenconn->sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
						sizeof(int)) == -1) {
			perror("setsockopt on this socket failed, retrying...\n");
			continue;//return -1;
		}

		if (bind(listenconn->sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(listenconn->sockfd);
			perror("bind on this address failed, retrying...\n");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "failed to bind\n");
		return NULL;
	}

	freeaddrinfo(result); // all done with this structure

	if (listen(listenconn->sockfd, BACKLOG) == -1) {
		perror("could not listen\n");
		return NULL;
	}

	return listenconn;
}

// wait for (blocking) and accept any incoming connections on the listening socket
conn_t* accept_tcp(conn_t* listenconn) {
	char s[INET6_ADDRSTRLEN];
	socklen_t sin_size;
	struct sockaddr_in their_addr;
	sin_size = sizeof(their_addr);

	conn_t* clientconn = (conn_t*)malloc(sizeof(conn_t));

	clientconn->sockfd = accept(listenconn->sockfd, (struct sockaddr*)&their_addr, &sin_size);
	if (clientconn->sockfd == -1) {
		perror("accept failed\n");
		return NULL;
	}
	clientconn->theiraddr = their_addr;

	inet_ntop(AF_INET, &(their_addr.sin_addr), s, sizeof(s));

	printf("accepted connection from %s\n", s);
	return clientconn;
}

// receive data on the socket
int recv_tcp(conn_t* connxn, char* buffer, unsigned int bufSize) {
	int retVal = recv(connxn->sockfd, buffer, bufSize, 0);
	if(retVal <= 0) {
		if(retVal == 0)
			printf("other party disconnected\n");
		else
			printf("Error in socket recv %s\n",gai_strerror(retVal));
	}

	return retVal;
}

// send data on the socket
int send_tcp(conn_t* connxn, char* buffer, unsigned int bytesToSend) {
	int ret;
	int retVal = 0;
	// all data in the buffer may not be sent on a single call to send(),
	// so loop until all data is sent
	while (retVal < (int)bytesToSend) {
		ret = send(connxn->sockfd, (char*)(buffer+retVal), bytesToSend-retVal, 0);
		if( ret <= 0 ) {
			printf("error in send\n");
			retVal = ret;
			break;
		}
		retVal += ret;
	}

	return retVal;
}

// Convenience function: Receive data into a provided buffer on the given connection
int recvData(conn_t* connxn, char* buffer, unsigned int bufSize, connxnstats_t* stats) {
	int retVal = recv_tcp(connxn, (char*)buffer, bufSize);
	if (retVal <= 0) {
		disconnect_tcp(connxn);
	}
	return retVal;
}

// Convenience function: Send data in a provided buffer on the given connection
int sendData(conn_t* connxn, char* buffer, unsigned int bytesToSend, connxnstats_t* stats) {
	int retVal = send_tcp(connxn, (char*)buffer, bytesToSend);
	if (retVal <= 0) {
		disconnect_tcp(connxn);
	}
	return retVal;
}