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
#include "base64.h"
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
int getFileForBuffer(char* path,char* filename, uint8_t ** ppBuffer,int cbBuffer, bool justTheSizeMaam);
char** str_split(char* a_str, const char a_delim,int * cElements);
void *accept_clients(void *args);
void *service_single_client(void *args);
//split string
char** str_split(char* a_str, const char a_delim,int * cElements)
{
	char** result    = 0;
	size_t count     = 0;
	char* tmp        = a_str;
	char* last_comma = 0;
	char delim[2];
	delim[0] = a_delim;
	delim[1] = 0;

    /* Count how many elements will be extracted. */
	while (*tmp)
	{
		if (a_delim == *tmp)
		{
			count++;
			last_comma = tmp;
		}
		tmp++;
	}

    /* Add space for trailing token. */
	count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
	count++;

	result = malloc(sizeof(char*) * count);

	if (result)
	{
		size_t idx  = 0;
		char* token = strtok(a_str, delim);
		while (token)
		{
			assert(idx < count);
			*(result + idx++) = strdup(token);
			token = strtok(0, delim);
			*cElements += 1;
		}
		assert(idx == count - 1);
		*(result + idx) = 0;
	}
	
	return result;
}
//get file size
int getFileSize(char* path,char* filename)
{
	char cw[1024];
	memset(cw,0,1024);
	getcwd(cw,1024);
	int count = 0;
	strncat(cw,filename,511);
	printf("%s\n", "Got here 1");
	FILE *file;

//Open file
	file = fopen(cw, "rb");
	if (!file)
	{
		perror("Open file");
		return EXIT_FAILURE;
	}
	printf("%s\n", "Got here 2");
	//Get file length
	fseek(file, 0, SEEK_END);
	count=ftell(file);
	fclose(file);
	return count;
}

//read the requested file and return a buffer containing the file
int getFileForBuffer(char* path,char* filename, uint8_t ** ppBuffer, int cbBuffer, bool justTheSizeMaam){
	char cw[1024];
	memset(cw,0,1024);
	getcwd(cw,1024);
	
	strncat(cw,filename,511);
	printf("%s\n", "Got here 1");
#ifdef DEBUG_BUILD
	printf("Here is the message:n\n");
	for (int i = 0; i < 1023; i++)
	{
	    printf("%02X", cw[i]);
	}
#endif
	FILE *file;

//Open file
	file = fopen(cw, "rb");
	if (!file)
	{
		perror("Open file");
		return EXIT_FAILURE;
	}
	if (!ppBuffer)
	{
		fprintf(stderr, "Memory error!");
		fclose(file);
		return EXIT_FAILURE;
	}

//Read file contents into buffer
	if(!justTheSizeMaam)
	{
		printf("%s\n", "Got here 5");
		fread(ppBuffer, 1, cbBuffer, file);
		printf("%s\n", "Got here 6");
	}
	fclose(file);
	printf("%s\n", "returning from allocs");
	return 0;
}
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
        
	   }
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

		while (1) {
			int outB64 = 0;
			int sizer = 0;
			char** b64Ch[64];
			//memset(recvBuf,0,sizeof recvBuf);
			tv.tv_sec = 10;
			tv.tv_usec = 10000; //10ms
			FD_ZERO(&readfds);
			FD_SET(clientconn->sockfd, &readfds);

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
				printf("%s\n", recvBuf);
				if(strstr(recvBuf,"GetFile GET") != NULL)
				{
					//int getFileForBuffer(char* path,char* filename, uint8_t ** ppBuffer, int * cbBuffer){
					
					int testElems = 0;
					char testPath[1024];
					char** testStrSplit = str_split(recvBuf,' ',&testElems);
					if(testElems ==3)
					{
						printf("Getting file %s\n", testStrSplit[2]);
						sizer = getFileSize(".",testStrSplit[2]);
						printf("Size was %d\n", sizer);
						uint8_t buf[sizer];
						//memset(buf,0,sizer); 
						
						getFileForBuffer(".",testStrSplit[2],&buf,sizer, false);
						//printf("Got %d with pointer %p\n", testOut, buf);
						if(sizer > 0)
						{
							//b64Ch = 
							
							base64_encode(buf,&b64Ch, sizer,&outB64);
							char* newPath = (char*)malloc(outB64+40);
							snprintf(newPath,outB64+40,"GetFile OK %s %d %s",testStrSplit[2],
								outB64,*b64Ch);
							//printf("Sending %s\n", newPath);
							*b64Ch = newPath;
						}
						else
						{
							snprintf(testPath,sizeof testPath,"GetFile FILE_NOT_FOUND %s 0 \0",
								testStrSplit[2]);
							*b64Ch = testPath;
							outB64 = 1;
							//printf("Sending %s\n", testPath);
						}
					}
					
				}
				stats_reportBytesRecd(stats, retVal); //report bytes received for stats   
			}
			//snprintf(sendBuf,sizeof sendBuf, "Hello client %d\n",time(NULL));
			// send data
			if(outB64 > 0 && b64Ch != NULL)
			{
				int retVal = sendData(clientconn, b64Ch, outB64+40);
				printf("Sending %d bytes\n", outB64+40);
				if(outB64 > 1)
				{
					free(b64Ch);
				}
				if (retVal <= 0)
					break;
				printf("Sent %d bytes\n", retVal);
				stats_reportBytesSent(stats, retVal); //report bytes sent for stats
			}
		}
		//print final stats before exiting and reset stats for next connection
		stats_finalize(stats);

		printf("\nWaiting for connection on %s...\n", listenPort);
	}
	
	disconnect_tcp(listenconn);
	pthread_exit(NULL);
}