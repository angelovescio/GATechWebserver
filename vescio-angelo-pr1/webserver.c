#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "llist.h"
#include "optlist/optlist.h"

#define SOCKET_ERROR -1
#define QUEUE_SIZE 1000
#define BUFFER_SIZE 1024

int queue[QUEUE_SIZE];
pthread_mutex_t mtx;
pthread_cond_t *cond;
pthread_cond_t *cond2;

void worker(void *threadarg);
int getFileForBuffer(char* path,char* filename, uint8_t * ppBuffer, int * cbBuffer);

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
	}

	return EXIT_SUCCESS;
}
int boss(int cThreads,int hPort)
{
	int hSocket, hServerSocket;  /* handle to socket */
	struct hostent* pHostInfo;   /* holds info about a machine */
	struct sockaddr_in Address; /* Internet socket address stuct */
	int nAddressSize = sizeof(struct sockaddr_in);
	int nHostPort = 8888;
	int numThreads = 1;
	int i;

	//init(&head,&tail);
	init();

	printf("\nStarting server");

	printf("\nMaking socket");
/* make a socket */
	hServerSocket=socket(AF_INET,SOCK_STREAM,0);

	if(hServerSocket == SOCKET_ERROR)
	{
		printf("\nCould not make a socket\n");
		return 0;
	}

/* fill address struct */
	Address.sin_addr.s_addr = INADDR_ANY;
	Address.sin_port = htons(hPort);
	Address.sin_family = AF_INET;

	printf("\nBinding to port %d\n",hPort);

/* bind to a port */
	if(bind(hServerSocket,(struct sockaddr*)&Address,sizeof(Address)) == SOCKET_ERROR) {
		printf("\nCould not connect to host\n");
		return 0;
	}
/*  get port number */
	getsockname(hServerSocket, (struct sockaddr *) &Address,(socklen_t *)&nAddressSize);

	printf("Opened socket as fd (%d) on port (%d) for stream i/o\n",hServerSocket, ntohs(Address.sin_port));

	printf("Server\n\
		sin_family        = %d\n\
		sin_addr.s_addr   = %d\n\
		sin_port          = %d\n"
		, Address.sin_family
		, Address.sin_addr.s_addr
		, ntohs(Address.sin_port)
		);

	pthread_t tid[cThreads];

	for(i = 0; i < cThreads; i++) {
		pthread_create(&tid[i],NULL,&worker,NULL);
	}

	printf("\nMaking a listen queue of %d elements",QUEUE_SIZE);
/* establish listen queue */
	if(listen(hServerSocket,QUEUE_SIZE) == SOCKET_ERROR) {
		printf("\nCould not listen\n");
		return 0;
	}

	while(1) {

		pthread_mutex_lock(&mtx);
		printf("\nWaiting for a connection");

		while(!empty()) {
			pthread_cond_wait (&cond2, &mtx);
		}

    /* get the connected socket */
		hSocket = accept(hServerSocket,(struct sockaddr*)&Address,(socklen_t *)&nAddressSize);

		printf("\nGot a connection");

		enqueue(hSocket);

		pthread_mutex_unlock(&mtx);
    	pthread_cond_signal(&cond);     // wake worker thread
    }
}
void worker(void *threadarg) {

	pthread_mutex_lock(&mtx);
	char allText[BUFFER_SIZE];
	int totalBytesSent = 0;
	while(empty(head,tail)) {
		pthread_cond_wait(&cond, &mtx);
	}
	struct node* ptr = (struct node*)threadarg;
	int hSocket = dequeue(ptr);
	uint8_t * buffer = NULL;
	int cbBuffer = 0;
	getFileForBuffer(ptr->path,ptr->filename,&buffer,&cbBuffer);
	if(cbBuffer <=0)
	{
		return EXIT_FAILURE;
	}
	unsigned nSendAmount, nRecvAmount;
	char line[BUFFER_SIZE];

	nRecvAmount = read(hSocket,line,sizeof(line));
	printf("\nReceived %s from client\n",line);

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
int getFileForBuffer(char* path,char* filename, uint8_t * ppBuffer, int * cbBuffer){
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
	strncat("/",fullpath,1);
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
	ppBuffer=(uint8_t *)malloc((*cbBuffer)+1);
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