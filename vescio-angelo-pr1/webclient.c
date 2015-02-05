#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>
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
void worker(void *threadarg);
char** str_split(char* a_str, const char a_delim,int * cElements);

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
		memset(recvBuff, '0',sizeof(recvBuff));
		if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		{
			printf("\n Error : Could not create socket \n");
			return 1;
		} 

		memset(&serv_addr, '0', sizeof(serv_addr)); 

		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(port); 

		if(inet_pton(AF_INET, host, &serv_addr.sin_addr)<=0)
		{
			printf("\n inet_pton error occured\n");
			return 1;
		} 

		if( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
		{
			printf("\n Error : Connect Failed \n");
			return 1;
		} 
		for(int i =0; i<outElems;i++)
		{
			sprintf(recvBuff,"GetFile GET %s",splitStr[i]);
			printf("Request was: %s\n",recvBuff);
			n = write(sockfd, recvBuff, sizeof(recvBuff)-1);
			if(n<=0)
			{
				printf("\n Error : Writing to socket\n");
			}
			memset(recvBuff, '0',sizeof(recvBuff));
		}
		
		while ( (n = read(sockfd, recvBuff, sizeof(recvBuff)-1)) > 0)
		{
			recvBuff[n] = 0;
			if(fputs(recvBuff, stdout) == EOF)
			{
				printf("\n Error : Fputs error\n");
			}
		} 

		if(n < 0)
		{
			printf("\n Read error \n");
		} 

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
	/*
	boss()
	{
		create array for workload paths
		char paths[cFiles][1024];
		put path into array
		for(int i =0 //line in workload)
		{
			//add to file list
			paths[i] = createFileFullPath(path,filename)
		}
		for(int i = 0,j=0;i<cRequests;i++;j++)
		{
			if(cFiles <= j)
			{
				j =0;
			}
			//add path to queue - global
			enqueue(paths[j]);
		}
		for(int i =0; i< threadCount;i++)
		{
			create_thread(ip,port
		}
	}
*/
	int boss(int cThreads,int hPort,char* chIpAddress)
	{

		while(1) {

			pthread_mutex_lock(&mtx);
			printf("\nWaiting for a connection");

			while(!empty()) {
				pthread_cond_wait (&cond2, &mtx);
			}


			//enqueue(hSocket);

			pthread_mutex_unlock(&mtx);
    	pthread_cond_signal(&cond);     // wake worker thread
    }
}
/*
	worker(ip,port,downloadPath)
	{
		char * path = "";
		lock(mutex)
		{
			path = popItemFromQueue();
		}
		char* message formatMessageIntoGetFile(path);
		int hSocket = create_socket(ip,port);
		send_request(hSocket);
		uint8_t * pBuffer;
		listen_and_read(pBuffer)
		save_to_downloads(pBuffer)
	}
	*/
	void worker(void *threadarg) {

		pthread_mutex_lock(&mtx);
		int totalBytesSent = 0;
		while(empty(head,tail)) {
			pthread_cond_wait(&cond, &mtx);
		}
		int hSocket = dequeue((struct node*)threadarg);
		char allText[BUFFER_SIZE];
		unsigned nSendAmount, nRecvAmount;
		char line[BUFFER_SIZE];

		nRecvAmount = read(hSocket,line,sizeof line);
		printf("\nReceived %s from client\n",line);


//***********************************************
//DO ALL HTTP PARSING (Removed for the sake of space; I can add it back if needed)
//*********************************************** 


		nSendAmount = write(hSocket,allText,sizeof(allText));

		if(nSendAmount != -1) {
			totalBytesSent = totalBytesSent + nSendAmount;
		}
		printf("\nSending result: \"%s\" back to client\n",allText);

		printf("\nClosing the socket");
/* close socket */
		if(close(hSocket) == SOCKET_ERROR) {
			printf("\nCould not close socket\n");
			return 0;
		}


		pthread_mutex_unlock(&mtx);
		pthread_cond_signal(&cond2);
	}
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