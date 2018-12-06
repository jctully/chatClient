#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>

#define MAX_LENGTH 1000



/*------------------------------------------------------------------------
* Program: demo_client
*
* Purpose: allocate a socket, connect to a server, and print all output
*
* Syntax: ./demo_client server_address server_port
*
* server_address - name of a computer on which server is executing
* server_port    - protocol port number server is using
*
*------------------------------------------------------------------------
*/

void fullRead(int sd, char* buffer, int length) {
  int m;
  int n =  recv(sd, buffer, length, 0);
  if (n<0) {
    perror("recv");
    exit(EXIT_FAILURE);
  }
  while (n < length) {
    m =  recv(sd, buffer, length - n, 0);
    if (m<0) {
      perror("recv");
      exit(EXIT_FAILURE);
    }
    n += m;
  }
  buffer[length] = '\0';
}

//0 if non-whitespace characters present, else 1
int whitespace(const char *str) {
  while (*str != '\0') {
    if (!isspace((unsigned char)*str)) {
      return 0;
    }
    str++;
  }
  return 1;
}

int main( int argc, char **argv) {
  struct hostent *ptrh; /* pointer to a host table entry */
  struct protoent *ptrp; /* pointer to a protocol table entry */
  struct sockaddr_in sad; /* structure to hold an IP address */
  int sd; /* socket descriptor */
  int port; /* protocol port number */
  char *host; /* pointer to host name */
  char buf[1000]; /* buffer for data from the server */
  uint16_t nameLen;
  uint16_t messageLen;
  char message[255];
  char letter;
  int rv;
  int activeState;

  fd_set readfds;
  FD_ZERO(&readfds);

  memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
  sad.sin_family = AF_INET; /* set family to Internet */

  if( argc != 3 ) {
    fprintf(stderr,"Error: Wrong number of arguments\n");
    fprintf(stderr,"usage:\n");
    fprintf(stderr,"./client server_address server_port\n");
    exit(EXIT_FAILURE);
  }

  port = atoi(argv[2]); /* convert to binary */
  if (port > 0) /* test for legal value */
  sad.sin_port = htons((u_short)port);
  else {
    fprintf(stderr,"Error: bad port number %s\n",argv[2]);
    exit(EXIT_FAILURE);
  }

  host = argv[1]; /* if host argument specified */

  /* Convert host name to equivalent IP address and copy to sad. */
  ptrh = gethostbyname(host);
  if ( ptrh == NULL ) {
    fprintf(stderr,"Error: Invalid host: %s\n", host);
    exit(EXIT_FAILURE);
  }

  memcpy(&sad.sin_addr, ptrh->h_addr, ptrh->h_length);

  /* Map TCP transport protocol name to protocol number. */
  if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
    fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
    exit(EXIT_FAILURE);
  }

  /* Create a socket. */
  sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
  if (sd < 0) {
    fprintf(stderr, "Error: Socket creation failed\n");
    exit(EXIT_FAILURE);
  }


  /* Connect the socket to the specified server.*/
  if (connect(sd, (struct sockaddr*) &sad, sizeof(sad)) < 0) {
    if (errno != EINPROGRESS) {
      perror("connect");
      exit(EXIT_FAILURE);
    }
  }

  // Check if theres room for another connection
  printf("waiting for server response\n");
  recv(sd, &letter, 1, 0);
  if (letter == 'Y') {
    activeState = 0;
    //prompt input
    while (activeState==0) {
      printf("Enter your username: ");
      fgets(buf, 255, stdin);
      printf("\n");
      //validate name, timer
      nameLen = strlen(buf);
      //nameLen = htons(nameLen);
      send(sd, &nameLen, sizeof(nameLen), 0);
      send(sd, buf, nameLen, 0);

      recv(sd, &letter, sizeof(letter), 0);
      //printf("Read letter: %c\n", letter);
      if(letter == 'Y') {
        activeState = 1;
      }
      else if (letter == 'T') {
        printf("Username taken.\n");
      }
      else if (letter == 'I') {
        printf("Username invalid.\n");
      }
    }//end inac state

    while(1) {//message loop
      printf("Enter message: ");
      fgets(message, 255, stdin);
      messageLen = strlen(message);
      //validate
      rv = whitespace(message);
      if (rv==1) {
        printf("Message can't be all whitespace.\n");
        continue;
      }
      send(sd, &messageLen, sizeof(messageLen), 0);
      send(sd, message, messageLen, 0);
      //printf("message sent: \"%s\"\n", message);
      if (strlen(message) > MAX_LENGTH) {
        printf("Message too long. ");
        break;
      }
      if (strcmp("quit\n", message) == 0) {
        printf("Quit received. ");
        break;
      }
    } //end message loop

  } else if (letter=='N') {
    printf("Server full. ");
  }
  printf("Closing connection\n");
  close(sd);

  exit(EXIT_SUCCESS);
}
