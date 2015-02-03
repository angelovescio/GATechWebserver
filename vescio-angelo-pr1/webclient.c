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

int main(int argc, char *argv[])
{
	option_t *optList, *thisOpt;

    /* get list of command line options and their arguments */
	optList = NULL;
	optList = GetOptList(argc, argv, "a:bcd:ef?");

    /* display results of parsing */
	while (optList != NULL)
	{
		thisOpt = optList;
		optList = optList->next;

		if ('?' == thisOpt->option)
		{
        	/*
			usage:
			webclient [options]
			options:
			-s server address (Default: 0.0.0.0)
			-p server port (Default: 8888)
			-t number of worker threads (Default: 1, Range: 1-100)
			-w path to workload file (Default: workload.txt)
			-d path to downloaded file directory (Default: null)
			-r number of total requests (Default: 10, Range: 1-1000)
			-m path to metrics file (Default: metrics.txt)
			-h show help message
        	*/
			printf("Usage: %s <options>\n\n", FindFileName(argv[0]));
			printf("options:\n");
			printf("  -a : option excepting argument.\n");
			printf("  -b : option without arguments.\n");
			printf("  -c : option without arguments.\n");
			printf("  -d : option excepting argument.\n");
			printf("  -e : option without arguments.\n");
			printf("  -f : option without arguments.\n");
			printf("  -? : print out command line options.\n\n");

            FreeOptList(thisOpt);   /* free the rest of the list */
			return EXIT_SUCCESS;
		}

		printf("found option %c\n", thisOpt->option);

		if (thisOpt->argument != NULL)
		{
			printf("\tfound argument %s", thisOpt->argument);
			printf(" at index %d\n", thisOpt->argIndex);
		}
		else
		{
			printf("\tno argument for this option\n");
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
	int nHostPort;
	int numThreads;

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
//Up to this point is boring server set up stuff. I need help below this.
//**********************************************

//instantiate all threads
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