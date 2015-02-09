#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include "llist.h"
#include "clientserver.h"
#include "stats.h"
#include "optlist/optlist.h"

#define SOCKET_ERROR -1
#define QUEUE_SIZE 1000
#define BUFFER_SIZE 1024


const char *port = "8888";

/* We will use this struct to pass parameters to one of the threads */
struct workerArgs
{
	int socket;
};

void *accept_clients(void *args);
void *service_single_client(void *args);

int main(int argc, char *argv[])
{
	
	
	int nHostPort = 8888;
	int numThreads = 1;
	char* filePath = ".";
	option_t *optList, *thisOpt;

    /* get list of command line options and their arguments */
	optList = NULL;
	optList = GetOptList(argc, argv, "p:t:fh");

    /* display results of parsing */
	while (optList != NULL)
	{
		thisOpt = optList;
		optList = optList->next;

		if ('h' == thisOpt->option)
		{
        	/*
			usage:
			webserver [options]
			options:
			printf("  -p : port (Default: 8888)\n");
			printf("  -t : number of worker threads (Default: 1, Range: 1-1000)\n");
			printf("  -f : path to static files (Default: .)\n");
			printf("  -h : show help message\n");
        	*/
			printf("Usage: %s <options>\n\n", FindFileName(argv[0]));
			printf("options:\n");
			printf("  -p : port (Default: 8888)\n");
			printf("  -t : number of worker threads (Default: 1, Range: 1-1000)\n");
			printf("  -f : path to static files (Default: .)\n");
			printf("  -h : show help message\n");

            FreeOptList(thisOpt);   /* free the rest of the list */
			return EXIT_SUCCESS;
		}
		else if('p' == thisOpt->option)
		{
			if (thisOpt->argument != NULL)
			{
				nHostPort = atoi(thisOpt->argument);
			}
		}
		else if('t' == thisOpt->option)
		{
			if (thisOpt->argument != NULL)
			{
				numThreads = atoi(thisOpt->argument);
			}
		}
		else if('f' == thisOpt->option)
		{
			if (thisOpt->argument != NULL)
			{
				filePath = thisOpt->argument;
			}
		}
        free(thisOpt);    /* done with this item, free it */
        /* The pthread_t type is a struct representing a single thread. */
		pthread_t server_thread;

	/* If a client closes a connection, this will generally produce a SIGPIPE
           signal that will kill the process. We want to ignore this signal, so
	   send() just returns -1 when this happens. */
           sigset_t new;
           sigemptyset (&new);
           sigaddset(&new, SIGPIPE);
           if (pthread_sigmask(SIG_BLOCK, &new, NULL) != 0) 
           {
           	perror("Unable to mask SIGPIPE");
           	exit(-1);
           }
		if (pthread_create(&server_thread, NULL, accept_clients, port) < 0)
	   	{
	   		perror("Could not create server thread");
	   		exit(-1);
	   	}

	   	pthread_join(server_thread, NULL);

	   	pthread_exit(NULL);
	   }
	   return EXIT_SUCCESS;
	}

/* This is the function that is run by the "server thread".
   The socket code is similar to oneshot-single.c, except that we will
   use getaddrinfo() to get the sockaddr (instead of creating it manually)
   and we will use sockaddr_storage when accepting a client connection
   (instead of using sockaddr_in, which assumes that the incoming connection
   is coming from an IPv4 host).
   Additionally, this function will spawn a new thread for each new client
   connection.
   See oneshot-single.c and client.c for more documentation on how the socket
   code works.
 */
void *accept_clients(void *args)
{
	struct timeval tv;
	fd_set readfds;
	
	// use listen port from cmd line args if supplied, otherwise use default value
	const char* listenPort = (char *)args;

	conn_t* listenconn = listen_tcp(listenPort);
	if (!listenconn)
		return -1;

	printf("\nWaiting for connection on %s...\n", listenPort);
	while (1) {
		// Accept a client socket
		conn_t* clientconn = accept_tcp(listenconn);
		if (!clientconn)
			continue;

		// connection stats - track how many bytes are sent/received and rates
		connxnstats_t* stats = stats_initialize();

		//"data" to send, and to receive and verify
		unsigned char recvCounter=0;
		unsigned char sendCounter=0;
		unsigned char recvBuf[2000];
		unsigned char sendBuf[2000];
		sendBuf[0] = 0;

		while (1) {
			tv.tv_sec = 0;
			tv.tv_usec = 10000; //10ms
			FD_ZERO(&readfds);
			FD_SET(clientconn->sockfd, &readfds);

			//populate buffer to send
			for (int i=0; i<(int)sizeof(sendBuf); i++) {
				sendBuf[i] = sendCounter++;
			}
			
			// check configured sockets
			if (select(clientconn->sockfd + 1, &readfds, NULL, NULL, &tv) == -1) {
				perror("select failed\n");
				disconnect_tcp(clientconn);
				break;
			}

			// recv data if available
			if (FD_ISSET(clientconn->sockfd, &readfds)) {
				int retVal = recvData(clientconn, (char*)recvBuf, (int)sizeof(recvBuf));
				if (retVal <= 0)
					break;
				stats_reportBytesRecd(stats, retVal); //report bytes received for stats
				
				//ensure integrity of data received
				// for (int i=0; i<retVal; i++) {
				// 	assert(recvBuf[i] == recvCounter);
				// 	recvCounter += 4;
				// }
			}

			// send data
			int retVal = sendData(clientconn, (char*)sendBuf, (int)sizeof(sendBuf));
			if (retVal <= 0)
				break;
			stats_reportBytesSent(stats, retVal); //report bytes sent for stats
		}
		//print final stats before exiting and reset stats for next connection
		stats_finalize(stats);

		printf("\nWaiting for connection on %s...\n", listenPort);
	}
	
	disconnect_tcp(listenconn);
	pthread_exit(NULL);
}


/* This is the function that is run by the "worker thread".
   It is in charge of "handling" an individual connection and, in this case
   all it will do is send a message every five seconds until the connection
   is closed.
   See oneshot-single.c and client.c for more documentation on how the socket
   code works.
 */
void *service_single_client(void *args) {
   	struct timeval tv;
   	fd_set readfds;

	// use listen port from cmd line args if supplied, otherwise use default value
   	const char* listenPort = args;

   	conn_t* listenconn = listen_tcp(listenPort);
   	if (!listenconn)
   		return -1;

   	printf("\nWaiting for connection on %s...\n", listenPort);

   	while (1) {
		// Accept a client socket
   		conn_t* clientconn = accept_tcp(listenconn);
   		if (!clientconn)
   			continue;

		// connection stats - track how many bytes are sent/received and rates
   		connxnstats_t* stats = stats_initialize();

		//"data" to send, and to receive and verify
   		unsigned char recvCounter=0;
   		unsigned char sendCounter=0;
   		unsigned char recvBuf[2000];
   		unsigned char sendBuf[2000];
   		sendBuf[0] = 0;

   		while (1) {
   			tv.tv_sec = 0;
			tv.tv_usec = 10000; //10ms
			FD_ZERO(&readfds);
			FD_SET(clientconn->sockfd, &readfds);

			//populate buffer to send
			for (int i=0; i<(int)sizeof(sendBuf); i++) {
				sendBuf[i] = sendCounter++;
			}
			
			// check configured sockets
			if (select(clientconn->sockfd + 1, &readfds, NULL, NULL, &tv) == -1) {
				perror("select failed\n");
				disconnect_tcp(clientconn);
				break;
			}

			// recv data if available
			if (FD_ISSET(clientconn->sockfd, &readfds)) {
				int retVal = recvData(clientconn, (char*)recvBuf, (int)sizeof(recvBuf));
				if (retVal <= 0)
					break;
				stats_reportBytesRecd(stats, retVal); //report bytes received for stats
				
				//ensure integrity of data received
				for (int i=0; i<retVal; i++) {
					assert(recvBuf[i] == recvCounter);
					recvCounter += 4;
				}
			}

			// send data
			int retVal = sendData(clientconn, (char*)sendBuf, (int)sizeof(sendBuf));
			if (retVal <= 0)
				break;
			stats_reportBytesSent(stats, retVal); //report bytes sent for stats
		}
		//print final stats before exiting and reset stats for next connection
		stats_finalize(stats);

		printf("\nWaiting for connection on %s...\n", listenPort);
	}
	
	disconnect_tcp(listenconn);
	return 1;
}