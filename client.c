#define DEFAULT_SOURCE /* For NI_MAXHOST and NI_MAXSERV */
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>


#define PORTNUM "49829"    /* Port number for server */

void genMatrix(int matrix[20][20], int mSize);
void printMatrix(int matrix[20][20], int mSize);
pthread_mutex_t lock;
pthread_t callThd[20];
int matrixA[20][20],matrixB[20][20],matrixC[20][20];
int count[20];
int mSize,threads;


void* sendPortion(void* input)
{
   //making copy then unlocking-makes sure that each row/s is computed
   int x = *(int*)input;
   pthread_mutex_unlock(&lock);

    //prewritten code for server
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_canonname = NULL; 
    hints.ai_addr = NULL;
    hints.ai_next = NULL; 
    hints.ai_family = AF_UNSPEC; 
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICSERV;

    if (getaddrinfo(NULL, PORTNUM, &hints, &result) != 0){
       exit(-1);
    }
    int cfd;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
         cfd = socket(rp->ai_family, rp->ai_socktype, 
                      rp->ai_protocol);

         if (cfd == -1)
            continue;   /* On error, try next address */

         if (connect(cfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break; /* Success */

         /* close() failed, close this socket, try next address */
         close(cfd);
    }

    if (rp == NULL)
    {
       fprintf(stderr, "No socket to bind...\n");
       exit(-1);
    }

   freeaddrinfo(result);
   //the first client will send info to the server about the size of the matrix etc
   if(x == 0){
   send(cfd,&mSize,sizeof(int),0);
   send(cfd,&threads,sizeof(int),0);
   }

   else{

   //i goes from current row up to the number of rows in a thread partition
   int i = (mSize/threads*x)-mSize/threads;
  
   for(; i<mSize/threads*x;i++){
   send(cfd,&matrixA[i],sizeof(matrixA[i]),0);
   }

   //then matrix is sent
   send(cfd,&matrixB,sizeof(matrixB),0);

   //computed on server and filled back into each row 
   i = (mSize/threads*x)-mSize/threads;
   for(; i<mSize/threads*x;i++){
   recv(cfd,&matrixC[i],sizeof(matrixC[i]),0);
   }
   }


    if (close(cfd) == -1) /* Close connection */
    {
        fprintf(stderr, "close error.\n");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) 
{

   mSize = 5;
   threads= 1;
   //if user doesnt enter command line arguments we run this way instead
   if(argv[1] == NULL){
   printf("Program using a TCP socket server:\nPartitions matrix into threads and computes Multiplication\n");
	printf("Please enter the size of the matrix(must be no greater than 14: ");
	scanf("%d",&mSize);
	printf("Please enter the number of threads used to compute(note matrix size mus be divisible by thread no.): ");
	scanf("%d",&threads);  
   }

   //otherwise we run through command line arguments
   else{
      int opt;
      while((opt = getopt(argc,argv,"m:t:h")) != -1){
         switch(opt){
         case 'm':
            mSize = atoi(optarg);
            break;
         case 't':
            threads = atoi(optarg);
            break;
         case 'h':
            fprintf(stderr,"Program using tcp socket server and multithreaded program to compute matrix\n");
            fprintf(stderr,"Maximum size of matrix is 14\n");
            fprintf(stderr,"The size of the matrix must be divisible by number of threads\n");
            fprintf(stderr,"If no arguments are given then the program will be run from stdIn\n");
            fprintf(stderr,"-m Is size of matrix and -t is num threads\n");
            fprintf(stderr,"If one is not entered default matrix size is 5 and default thread num is 1\n");
            exit(EXIT_SUCCESS);
         
         default :
         fprintf(stderr,"Not valid enter -h for help\n");
         exit(EXIT_FAILURE);
         }
      }
   }
   
   //error checking
   if(mSize> 14 || threads > 14 || mSize% threads != 0){
      printf("Error occured run program with -h for info on how to use program\n");
      exit(EXIT_FAILURE);
   }

   printf("\nMatrices of size %d,With each thread working on %d rows\n\n",mSize,mSize/threads);

   //generating then print matrix
  	 srand(time(0));
  	 genMatrix(matrixA,mSize);
	genMatrix(matrixB,mSize);
	printf("Matrix A:\n");
	printMatrix(matrixA,mSize);
	printf("Matrix B:\n");
	printMatrix(matrixB,mSize);

   //if we were to do this iteratively by creating a client then sending then receiving
   //there would be no point in having a concurrent server as clients would be sent iteratively anyways
   //as such clients must also be created with threads as well

   //setting up joinable threads as we need to wait for all threads to finish before printing
   pthread_attr_t attr;
   pthread_attr_init(&attr);
   pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

   //First thread sends info about thread num+matrix size so it must be completed before all other threads start
   int* argtot = (int*)malloc(sizeof(int));
   *argtot = 0;
   pthread_create(&callThd[0], &attr, sendPortion, argtot);
   pthread_join(callThd[0], NULL);


   /*lock used as each thread needs to make a copy of x 
    *otherwise x may update before a copy is made and 
    *a row will not be sent to server*/

   pthread_mutex_init(&lock, NULL);
   for(int x = 1;x<threads+1;x++){
      pthread_mutex_lock(&lock); 
      *argtot = x;
      pthread_create(&callThd[x], &attr, sendPortion, argtot);
   }

   //waiting for all threads to finish
   for(int x = 1;x<threads+1;x++){
         pthread_join(callThd[x], NULL);
   }
  
   printf("Matrix result:\n");
   printMatrix(matrixC,mSize);

    exit(EXIT_SUCCESS);
}

//generating function for matrix 
void genMatrix(int matrix[20][20],int mSize){
    
    int i,j;
    for (i = 0; i <  mSize; i++)
      for (j = 0; j < mSize; j++)
         matrix[i][j] = rand()%9+1;
}

//printing function for matrix
void printMatrix(int matrix[20][20],int mSize){
    int i,j;
    for (i = 0; i <  mSize; i++){
      for (j = 0; j < mSize; j++){
         printf("[%d]",matrix[i][j]);
         }
         printf("\n");
         }
         printf("\n\n");
}


