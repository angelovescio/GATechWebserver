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
#include "stats.h"
#include "llist.h"
#include "optlist/optlist.h"

#define SOCKET_ERROR -1
#define QUEUE_SIZE 1000
#define BUFFER_SIZE 1024

int queue[QUEUE_SIZE];
pthread_mutex_t mtx;
pthread_cond_t *cond;
pthread_cond_t *cond2;

int getFileForBuffer(char* path,char* filename, uint8_t ** ppBuffer, int * cbBuffer);
char** str_split(char* a_str, const char a_delim,int * cElements);
//read the workload file and return a buffer containing the file
	int getFileForBuffer(char* path,char* filename, uint8_t ** ppBuffer, int * cbBuffer){
		char fullpath[1024];
		memset(fullpath,0,1024);
		if(strnlen(path,512) >=512)
		{
			return EXIT_FAILURE;
		}
		if(strnlen(filename,512) >=512)
		{
			return EXIT_FAILURE;
		}
		strncpy(fullpath,path,511);
		strncat(fullpath,"/",1);
		strncat(fullpath,filename,511);
		FILE *file;

	//Open file
		file = fopen(fullpath, "rb");
		if (!file)
		{
			fprintf(stderr, "Unable to open file %s", fullpath);
			return EXIT_FAILURE;
		}

	//Get file length
		fseek(file, 0, SEEK_END);
		*cbBuffer=ftell(file);
		fseek(file, 0, SEEK_SET);

	//Allocate memory
		*ppBuffer=(uint8_t *)malloc((*cbBuffer)+1);
		if (!ppBuffer)
		{
			fprintf(stderr, "Memory error!");
			fclose(file);
			return EXIT_FAILURE;
		}

	//Read file contents into buffer
		fread(ppBuffer, *cbBuffer, 1, file);
		fclose(file);

		return 0;
	}
int main(int argc, char *argv[])
{
	int sockfd = 0, n = 0;
	char recvBuff[1024];
	struct sockaddr_in serv_addr; 

	//char* host;//[64] = "0.0.0.0";
	char host[64] = "0.0.0.0";
	char workPath[1024];
	char downloadPath[1024];
	char metricPath[1024];
	int cThreads = 1;
	int port = 8888;
	int cRequests = 10;

	memset(workPath,0,sizeof(workPath));
	memset(downloadPath,0,sizeof(downloadPath));
	memset(metricPath,0,sizeof(metricPath));
	option_t *optList, *thisOpt;

    /* get list of command line options and their arguments */
	optList = NULL;
	optList = GetOptList(argc, argv, "s:ptw:drmh");

	uint8_t buf[BUFFER_SIZE];
	memset(buf,0,BUFFER_SIZE);
	int outSize = 0;
	int outElems = 0;
	getFileForBuffer(".","workload-0.txt",&buf,&outSize);
	char** splitStr = str_split(buf,'\n',&outElems);
    /* display results of parsing */
	while (optList != NULL)
	{
		thisOpt = optList;
		optList = optList->next;

		if ('h' == thisOpt->option)
		{
        	/*
			usage:
			webclient [options]
			options:
			printf("  -s : server address (Default: 0.0.0.0)\n");
			printf("  -p : server port (Default: 8888)\n");
			printf("  -t : number of worker threads (Default: 1, Range: 1-100)\n");
			printf("  -w : path to workload file (Default: workload.txt)\n");
			printf("  -d : path to downloaded file directory (Default: null)\n");
			printf("  -r : number of total requests (Default: 10, Range: 1-1000)\n");
			printf("  -m : path to metrics file (Default: metrics.txt)\n");
			printf("  -h : show help message\n");
        	*/
			printf("Usage: %s <options>\n\n", FindFileName(argv[0]));
			printf("options:\n");
			printf("  -s : server address (Default: 0.0.0.0)\n");
			printf("  -p : server port (Default: 8888)\n");
			printf("  -t : number of worker threads (Default: 1, Range: 1-100)\n");
			printf("  -w : path to workload file (Default: workload.txt)\n");
			printf("  -d : path to downloaded file directory (Default: null)\n");
			printf("  -r : number of total requests (Default: 10, Range: 1-1000)\n");
			printf("  -m : path to metrics file (Default: metrics.txt)\n");
			printf("  -h : show help message\n");

            FreeOptList(thisOpt);   /* free the rest of the list */
			return EXIT_SUCCESS;
		}
		else if('s' == thisOpt->option)
		{
			if (thisOpt->argument != NULL)
			{
				//host = thisOpt->argument;
				memset(host,0,sizeof host);
				strncpy(host,thisOpt->argument,sizeof(host)-1);
			}
		}
		else if('p' == thisOpt->option)
		{
			if (thisOpt->argument != NULL)
			{
				port = atoi(thisOpt->argument);
			}
		}
		else if('t' == thisOpt->option)
		{
			if (thisOpt->argument != NULL)
			{
				cThreads = atoi(thisOpt->argument);
			}
		}
		else if('w' == thisOpt->option)
		{
			if (thisOpt->argument != NULL)
			{
				strncpy(workPath, thisOpt->argument,sizeof(workPath)-1);
			}
		}
		else if('d' == thisOpt->option)
		{
			if (thisOpt->argument != NULL)
			{
				strncpy(downloadPath, thisOpt->argument,sizeof(downloadPath)-1);
			}
		}
		else if('r' == thisOpt->option)
		{
			if (thisOpt->argument != NULL)
			{
				cRequests = atoi(thisOpt->argument);
			}
		}
		else if('m' == thisOpt->option)
		{
			if (thisOpt->argument != NULL)
			{
				strncpy(metricPath,thisOpt->argument,sizeof(metricPath)-1);
			}
		}
		fd_set readfds;
		struct timeval tv;

		// use server IP:port from cmd line args if supplied, otherwise use default values
		char buffer [33];
		snprintf(buffer,sizeof buffer,"%d",10);
		const char* serverIP = host;
		const char* serverPort = buffer;
		
		
		printf("Connecting to server %s:%d...\n", host, port);

		conn_t* serverconn = connect_tcp(serverIP, serverPort);
		if (!serverconn)
			return -1;

		// connection stats
		connxnstats_t* stats = stats_initialize();

		//"data" to send, and to receive and verify
		unsigned char recvCounter=0; // counter to check received values are as expected and in order
		unsigned char recvBuf[2000];
		unsigned char sendBuf[2000];
		sendBuf[0] = 0;

		while (1) {
			tv.tv_sec = 0;
			tv.tv_usec = 10000; // 10ms
			FD_ZERO(&readfds);
			FD_SET(serverconn->sockfd, &readfds);
			
			//TODO: abstract the select logic out of the main application
			if (select(serverconn->sockfd + 1, &readfds, NULL, NULL, &tv) == -1) {
				perror("select failed\n");
				disconnect_tcp(serverconn);
				break;
			}
			
			// recv data if available
			if (FD_ISSET(serverconn->sockfd, &readfds)) {

				// recv data
				int retVal = recvData(serverconn, (char*)recvBuf, (int)sizeof(recvBuf));
				if (retVal <= 0)
					break;
				if(retVal>0)
				{
					printf("%s\n", recvBuf);
				}
				// report stats
				stats_reportBytesRecd(stats, retVal); //stats reporting

				// ensure integrity of data received
				// for(int i=0; i<retVal; i++) {
				// 	assert(recvBuf[i] == recvCounter++);
				// }
			}
			snprintf(sendBuf,sizeof sendBuf, "Hello server %d\r\n",time(NULL));
			
			// send data
			if (sendBuf[0]%4 == 0) {
				int retVal = sendData(serverconn, (char*)sendBuf, (int)sizeof(sendBuf));
				if (retVal <= 0)
					break;

				// report stats
				stats_reportBytesSent(stats, retVal); //stats reporting
			}
			sendBuf[0] = sendBuf[0] + 1;
		}

		// print final stats before exiting
		stats_finalize(stats);

        free(thisOpt);    /* done with this item, free it */
	}

	return EXIT_SUCCESS;
}
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