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
#include <sys/stat.h>
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
int getFileForBuffer(char* path,char* filename, uint8_t ** ppBuffer,int cbBuffer);
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
printf("%s\n", "Got here 1");
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
printf("%s\n", "Got here 2");
    /* Add space for trailing token. */
	count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
	count++;
printf("%s %d\n", "Got here 3",count);
	result = malloc(sizeof(char*) * count);
printf("%s\n", "Got here 4");
	if (result)
	{
		size_t idx  = 0;
		char* token = strtok(a_str, delim);
		printf("%s\n", "Got here 5");	
		while (token)
		{
			printf("%s\n", "Got here 6");	
			assert(idx < count);
			printf("%s\n", "Got here 7");
			printf("Token %s\n", token);
			*(result + idx) = strdup(token);
			printf("%s\n", "Got here 8");
			idx++;
			*cElements += 1;
			token = strtok(0, delim);

			if(!token)
			{
				break;
			}
			printf("%s\n", "Got here 9");
			
			
			printf("%s\n", "Got here 10");
		}
		printf("%s\n", "Got here 11");	
		assert(idx == count - 1);
		*(result + idx) = 0;
	}
printf("%s\n", "Got here 12");	
	return result;
}
//get file size
int getFileSize(char* path,char* filename)
{
	printf("Got here getFileSize\n");
	char cw[4096];
	memset(cw,0,4096);
	getcwd(cw,4096);
	int count = 0;
	strncat(cw,filename,2048);
	printf("Got here getFileSize %s\n",cw);
	struct stat sb;
	if (stat(cw, &sb) == -1) {
        perror("stat");
        exit(EXIT_FAILURE);
    }

	return sb.st_size;
}

//read the requested file and return a buffer containing the file
int getFileForBuffer(char* path,char* filename, uint8_t ** ppBuffer, int cbBuffer){
	char cw[1024];
	memset(cw,0,1024);
	getcwd(cw,1024);
	
	strncat(cw,filename,511);
	printf("%s %s\n", "Got here 1",cw);
	// if(*ppBuffer != NULL)
	// {
	// 	printf("%s\n", "[Info] webserver:getFileForBuffer Freeing ppBuffer");
	// 	free(*ppBuffer);
	// }
	// printf("%s %d\n", "[Info] webserver:getFileForBuffer Freeing ppBuffer with size", cbBuffer);
	// *ppBuffer = calloc(cbBuffer,sizeof(uint8_t));
	printf("%s\n", "Got here 2");
    uint8_t* data = *ppBuffer;
	FILE *file;

//Open file
	file = fopen(cw, "rb");
	
	if (!file)
	{
		perror("Open file");
		return EXIT_FAILURE;
	}
	if (!data)
	{
		fprintf(stderr, "Memory error!");
		fclose(file);
		return EXIT_FAILURE;
	}

//Read file contents into buffer
	printf("%s\n", "Got here 5");
	fread(data, 1, cbBuffer, file);
	printf("%s\n", "Got here 6");
	
	fclose(file);
	for (int i = 0; i < cbBuffer && i < 20; i++)
	{
	    printf("%02X", data[i]);
	}
	printf("\n");
	printf("%s\n", "returning from getFileForBuffer");
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

//	   accept_clients("8888");

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
	int cmdLength =0;
	// use listen port from cmd line args if supplied, otherwise use default value
	const char* listenPort = (char *)args;

	conn_t* listenconn = listen_tcp(listenPort);
	if (!listenconn)
		return -1;
	struct stat s;
	int err = stat("data", &s);
	if(-1 == err) {
	    if(ENOENT == errno) {
	        mkdir("data",0777);
	    } else {
	        perror("webserver::accept_clients::stat");
	        exit(1);
	    }
	} else {
	    if(S_ISDIR(s.st_mode)) {
	        /* it's a dir */
	    } else {
	        /* exists but is no dir */
	    }
	}
	printf("\nWaiting for connection on %s...\n", listenPort);
	while (1) {
		char* b64Ch_ptr = NULL;
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
			uint8_t * b64Ch = NULL;
			uint8_t * buf = NULL;
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
				int retVal = recvData(clientconn, (char*)recvBuf, (int)sizeof(recvBuf),&stats);
				if (retVal <= 0)
					break;
				printf("%s\n", recvBuf);
				if(strstr(recvBuf,"GetFile GET") != NULL)
				{
					printf("%s\n", "Got here A");
					int testElems = 0;
					char testPath[1024];
					char** testStrSplit = str_split(recvBuf,' ',&testElems);
					printf("element count %d\n", testElems);
					printf("%s\n", "Got here B");
					if(testElems >=3)
					{
						printf("Getting file %s\n", testStrSplit[2]);
						sizer = getFileSize(".",testStrSplit[2]);
						printf("Size was %d\n", sizer);
						
						//memset(buf,0,sizer); 
						buf = calloc(sizer,sizeof(uint8_t));
						getFileForBuffer(".",testStrSplit[2],&buf,sizer);
						printf("Got %d with pointer %p\n", sizer, &buf);
						uint8_t * tempBuf = buf;
						for (int z = 0; z < sizer && z < 20; z++)
						{
						    printf("%02X", tempBuf[z]);
						}
						printf("\n");
						printf("[Info] Webserver::accept_clients::while::test_elems\n");
						if(sizer > 0)
						{
							printf("[Info] Webserver::accept_clients::while::test_elems::0\n"); 
							/*
							int base64_decode(	const char *data,
    											uint8_t ** decoded_data_ptr,
                             					size_t input_length,
                             					size_t *output_length)

                            int base64_encode(	const unsigned char *data,
												char ** encoded_data_ptr,
												int input_length,
												int *output_length)

							*/
							int b64sizeCalc = 4 * ((sizer+ 2) / 3);
							b64Ch = (uint8_t*)calloc(b64sizeCalc,sizeof(uint8_t));
							printf("[Info] Webserver::accept_clients::base64_encode(%p,%p,%d,%d)\n",
								buf,b64Ch, sizer,outB64); 
							base64_encode(buf,&b64Ch, sizer,&outB64);
							printf("[Info] Webserver::accept_clients Allocing %d for outB64\n",outB64);
							printf("[Info] Webserver::accept_clients::while::test_elems::1\n");
							b64Ch_ptr = b64Ch;
							printf("Here is the message:\n");
							for (int x = 0; (x < outB64) && (x < 20); x++)
						    {
						        printf("%02X", b64Ch_ptr[x]);
						    }
						    printf("%s\n", "");
							int realSize = (outB64);
							printf("[Info] Webserver::accept_clients Allocing %d for decoded buf\n",realSize);
							char newPath[/*1][*/realSize];// = //(char*)malloc(realSize*sizeof(char*));
							char newPathCmd[100];// = (char*)malloc(100);
							printf("[Info] Webserver::accept_clients::while::test_elems::3\n");
							memset(newPath,0,100);
							memset(newPathCmd,0,realSize*sizeof(char));
							printf("[Info] Webserver::accept_clients::while::test_elems::4\n");
							snprintf(newPathCmd,100,"GetFile OK %s %d ",testStrSplit[2],
								outB64);
							printf("GetFile OK %s %d \n",testStrSplit[2],
								outB64);
							printf("[Info] Webserver::accept_clients::while::test_elems::5\n");
							cmdLength = strlen(newPathCmd);
							printf("Sending cmdLength %d\n", cmdLength);
							snprintf(newPath,outB64+cmdLength,"%s%s",newPathCmd,
								b64Ch_ptr);
							printf("[Info] Webserver::accept_clients::while::test_elems::6\n");
							printf("Sending outB64+cmdLength %d\n", outB64+cmdLength);
							//printf("%s\n", "Freeing newPath");
							//free(newPath);
							//printf("%s\n", "Freeing newPathCmd");
							//free(newPathCmd);
						}
						else
						{
							printf("[Info] Webserver::accept_clients::while::test_elems::else\n");
							snprintf(testPath,sizeof testPath,"GetFile FILE_NOT_FOUND %s 0",
								testStrSplit[2]);
							b64Ch_ptr = testPath;
							outB64 = 1;
							//printf("Sending %s\n", testPath);
						}
						if(buf != NULL)
						{
							free(buf);
							buf = NULL;
						}
						if(b64Ch != NULL)
						{
							free(b64Ch);
							b64Ch = NULL;
						}

					}
					// if(testStrSplit)
					// {
					// 	printf("freeing string\n");
					// 	for(int q =0;q<testElems;q++)
					// 	{
					// 		printf("freeing string %n\n",q);
					// 		free(testStrSplit[q]);
					// 	}
					// 	free(testStrSplit);
					// }
					
				}
				stats_reportBytesRecd(stats, retVal); //report bytes received for stats   
			}
			//snprintf(sendBuf,sizeof sendBuf, "Hello client %d\n",time(NULL));
			// send data
			if(outB64 > 0 && b64Ch != NULL)
			{
				char sBuf[4096];
				int retVal = 0;
				for(int x=0;x<outB64+cmdLength;x+=4096)
				{
					memset(sBuf,0,sizeof(sBuf));
					int sendLeft = (outB64+cmdLength)-x;
					if( sendLeft<=4096)
					{
						memcpy(sBuf,&(b64Ch_ptr[x]),sendLeft);
						printf("Sending %d: %s\n",sendLeft, sBuf);
						retVal = sendData(clientconn, sBuf, sendLeft,&stats);

						break;
					}
					memcpy(sBuf,&(b64Ch_ptr[x]),sizeof(sBuf));
					printf("Sending %d: %s\n",sizeof(sBuf), sBuf);
					retVal = sendData(clientconn, sBuf, sizeof(sBuf),&stats);
				}
				
				printf("Sending %d bytes\n", outB64+cmdLength);
				//retVal = sendData(clientconn, '\0', 1);
				if(outB64 > 1)
				{
					//free(b64Ch_ptr);
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