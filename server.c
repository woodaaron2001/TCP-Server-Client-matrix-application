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
#include <pthread.h>

#define PORTNUM "49829"    /* Port number for server */
#define BACKLOG 128
#define BUFSIZE 32
void printMatrix(int matrix[20][20],int mSize);
int threads,mSize,count;

void*
handleRequest(void* input)
{
    int cfd = *(int*)input;
      printf("Handling row client\n");
      fflush(stdout);
      //matrices needed for computation
      int matrixB[20][20];
      int computeRow[20][20]={0},resultRow[20][20] ={0};
      
      //receives all the rows needed for computation
      int i = 0;
      for(; i<mSize/threads;i++){
      recv(cfd,&computeRow[i],sizeof(computeRow[i]),0);
      }
      recv(cfd,&matrixB,sizeof(matrixB),0);
     
      //i once again deals with the amount of rows depending on thread partition size
      for(i=0; i<mSize/threads;i++){
      for(int computeNum = 0;computeNum < mSize;computeNum++){
         for(int inner = 0; inner<mSize; inner++){
            resultRow[i][computeNum] += computeRow[i][inner] * matrixB[inner][computeNum];
         }
      }
      }
      //send computed back 
      for(i=0; i<mSize/threads;i++){
      send(cfd,&resultRow[i],sizeof(resultRow[i]),0);
      }
      

    if (close(cfd) == -1) /* Close connection */
    {
       fprintf(stderr, "close error. errno %d.\n", errno);
    }

    free(input);

    pthread_exit(NULL);
    return NULL;
}

int main(int argc, char *argv[]) 
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_canonname = NULL; 
    hints.ai_addr = NULL;
    hints.ai_next = NULL; 
    hints.ai_family = AF_UNSPEC; 
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;

    if (getaddrinfo(NULL, PORTNUM, &hints, &result) != 0)
       exit(-1);

    int lfd, optval = 1;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
         lfd = socket(rp->ai_family, rp->ai_socktype, 
                      rp->ai_protocol);

         if (lfd == -1)
            continue;   /* On error, try next address */

         if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, 
                        &optval, sizeof(optval)) == -1)
            exit(-1);

         if (bind(lfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break; /* Success */

         /* bind() failed, close this socket, try next address */
         close(lfd);
    }

    if (rp == NULL)
       exit(-1);

    {
       char host[NI_MAXHOST];
       char service[NI_MAXSERV];
       if (getnameinfo((struct sockaddr *)rp->ai_addr, rp->ai_addrlen,
                 host, NI_MAXHOST, service, NI_MAXSERV, 0) == 0)
          fprintf(stdout, "Listening on (%s, %s)\n", host, service);
       else
          exit(-1);
    }

    freeaddrinfo(result);

    if (listen(lfd, BACKLOG) == -1)
       exit(-1);

    pthread_attr_t tattr;
    pthread_attr_init(&tattr);
    pthread_attr_setdetachstate(&tattr,PTHREAD_CREATE_DETACHED);
     
   count = 0;

    for (;;)
    {
        int cfd = accept(lfd, NULL, NULL);
        if (cfd == -1) {
          continue;   /* Print an error message */
        }


      //first client is taken iteratively as it is needed for all subsequent clients
      if(count== 0){  
      mSize,threads = 0;  
      recv(cfd,&mSize,sizeof(int),0);
      recv(cfd,&threads,sizeof(int),0);
      printf("\nReceived matrix of size %d with %d threads\n",mSize,threads);
      fflush(stdout);
      count++;
      continue;
    }

   
   if(count != 0){

        int* argtot = (int*)malloc(sizeof(int));
        *argtot = cfd;
        pthread_t t;
        pthread_create(&t, &tattr, handleRequest, argtot);
        count++;

        //if we have computed all threads we set count back to 0 so we can accept another complete program
        //allows us to do multiple instances of client program
        //although first matrix needs to be done computing first
        if(count > threads){count = 0;}
      }
   
   }
        
    pthread_attr_destroy(&tattr); 

    exit(EXIT_SUCCESS);
}
