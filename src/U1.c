#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include "utils.h"
#include "registers.h"

#define BUFSIZE     256
#define THREADS_MAX 100

// Limit Values for Random user usage times
#define UPPERB 1000
#define LOWERB 1
// Interval betweeen User Requests (miliseconds)
#define INTMS 50

//global variables
int i;  /**< número sequencial do pedido */
bit serverOpen;

pthread_mutex_t mut=PTHREAD_MUTEX_INITIALIZER; /**<  mutex para aceder a i*/

/**
 * Thread Function that creates requests
 */
void * thread_func(void *arg){
  // updating i with mutex's
  pthread_mutex_lock(&mut); 
  i++;
  pthread_mutex_unlock(&mut);

  int fd_pub; /**< file descriptor of public fifo */
  char request[BUFSIZE]; /**< Request string to send to public fifo*/
  char * fifopath = (char *) arg; /**< public fifo path*/
  int useTime = (rand() % (UPPERB - LOWERB + 1)) + LOWERB; /**< Determine the use time for this client */

  // opening Public fifo to be able to write
  fd_pub = open(fifopath,O_WRONLY);
  if (fd_pub==-1){
    // In case there's an error, the Server is Offline (Closed)
    printRegister(elapsedTime(), i, (int)getpid(), (long int)pthread_self(), useTime, -1, CLOSD);
    serverOpen.x = 0;
    pthread_exit(NULL);
  }

  // Making of message to send, writing of message and closing of fifo
  sprintf(request,"[ %d, %d, %lu, %d, -1 ]", i, getpid(), pthread_self(), useTime);
  if (write(fd_pub, &request, BUFSIZE)<0){perror("Error writing request: "); exit(1);}
  printRegister(elapsedTime(), i, getpid(), pthread_self(), useTime, -1, IWANT);
  close(fd_pub);
  
  // Making of pathname of private fifo 
  char privateFifo[BUFSIZE]="tmp/";
  char temp[BUFSIZE];
  sprintf(temp,"%d",(int)getpid());
  strcat(privateFifo,temp);
  strcat(privateFifo,".");
  sprintf(temp,"%ld",(long int)pthread_self());
  strcat(privateFifo,temp);

  //Create private fifo to read message from server
  if(mkfifo(privateFifo,0660)<0){perror("Error creating private FIFO:"); exit(1);}

  // Opening private fifo to read the response
  int fd_priv = open(privateFifo, O_RDONLY);
  if (fd_priv < 0) {perror("[Client]Error opening private FIFO: "); exit(1);}

  // Attempting to read the response
  char receivedMessage[BUFSIZE];
  int tmpresult = read(fd_priv,&receivedMessage,BUFSIZE);
  // Attempts to read from private fifo until there's a response
  while(tmpresult==0){tmpresult = read(fd_priv,&receivedMessage,BUFSIZE);}
  if(tmpresult<0) {printRegister(elapsedTime(), i, getpid(), pthread_self(), useTime, -1, FAILD);}

  // If there's a response, parse the response to different variables
  int threadi, pid, dur, place;
  long int tid;
  sscanf(receivedMessage,"[ %d, %d, %ld, %d, %d ]",&threadi, &pid, &tid, &dur, &place);

  // check if there's a place available or the server is closed
  if (place >= 0)
    printRegister(elapsedTime(), threadi, getpid(), pthread_self(), dur, place, IAMIN);
  else{
    printRegister(elapsedTime(), threadi, getpid(), pthread_self(), dur, place, CLOSD);
  }
  
  // cleanup
  close(fd_priv);
  unlink(privateFifo);
  pthread_exit(NULL);
}

int main(int argc, char* argv[], char *envp[]) {
  char fifoname[BUFSIZE];   /**< public fifo file name */
  char fifopath[BUFSIZE]="tmp/";  /**< public fifo path */
  double nsecs;  /**< numbers of seconds the program will be running */
  pthread_t threads[THREADS_MAX];  /**< array to store thread id's */
  int thr=0; /**< index for thread id / nº od threads created */
  serverOpen.x = 1;

  //check arguments
  if (argc!=4) {
    printf("Usage: U1 <-t secs> fifoname\n");
    exit(1);}
  
  //read arguments
  strcpy(fifoname,argv[3]);
  nsecs=atoi(argv[2])*1000;
  strcat(fifopath,fifoname);

  //start counting time
  startTime();
  srand(time(NULL));

  //ciclo de geracao de pedidos
  while(serverOpen.x == 1 && elapsedTime() < (double) nsecs){
    pthread_create(&threads[thr],NULL, thread_func, (void *)fifopath);
    pthread_detach(threads[thr]);
    thr++;
    usleep(INTMS*1000);
  }
  pthread_exit((void*)0);
}
