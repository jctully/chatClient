#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>

#define MAX_LENGTH 1000


/* Used to read first a message's length, then the message itself. returns 0
if send successful, -1 if message too long, -2 if message blank. */
int fullRead(int sd, char* message) {
  int rv;
  uint16_t messageLen = 0;

  if ((rv = recv(sd, &messageLen, sizeof(messageLen), 0)) <0) {
    perror("recv");
    exit(EXIT_FAILURE);
  }
  if (messageLen == 0) {
    return -2;
  }
  if (messageLen > MAX_LENGTH) {
    return -1;
  }

  if ((rv = recv(sd, message, messageLen, 0)) <0) {
    perror("recv");
    exit(EXIT_FAILURE);
  }

  if (message[messageLen-1]=='\n') {
    message[messageLen-1] = '\0';
  }
  else {
    message[messageLen] = '\0';
  }
  return 0;
}

//returns 0 if non-whitespace characters present in string, else 1
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
  uint8_t nameLen;
  char letter;
  int activeState;
  int rv;

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

  //setup port
  port = atoi(argv[2]); /* convert to binary */
  if (port > 0) { /* test for legal value */
    sad.sin_port = htons((u_short)port);
  } else {
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

  if (connect(sd, (struct sockaddr*) &sad, sizeof(sad)) < 0) {
    if (errno != EINPROGRESS) {
      perror("connect");
      exit(EXIT_FAILURE);
    }
  }

  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(0, &fds);
  FD_SET(sd, &fds);

  // Check if theres room in server
  recv(sd, &letter, 1, 0);
  if (letter == 'Y') {
    activeState = 0;
    //prompt input
    while (activeState==0) {
      printf("Enter your affiliate\n");
      fgets(buf, 255, stdin);
      printf("\n");
      nameLen = strlen(buf);
      send(sd, &nameLen, sizeof(nameLen), 0);
      send(sd, buf, nameLen, 0);
      select(sd+1, &fds, NULL, NULL, NULL);

      if ((rv = recv(sd, &letter, sizeof(letter), 0)) <0) {
        perror("recv");
        exit(EXIT_FAILURE);
      }
      if(letter == 'Y') {
        printf("Affiliate found.\n");
        activeState = 1;
      }
      else if (letter == 'T') {
        printf("Affiliate taken.\n");
      }
      else if (letter == 'I') {
        printf("Affiliate invalid.\n");
      }
    }
    while(1) {
      //reset fdset
      FD_ZERO(&fds);
      FD_SET(0, &fds);
      FD_SET(sd, &fds);

      select(sd+1, &fds, NULL, NULL, NULL);
      if (FD_ISSET(0,&fds)) {
        fgets(buf, 255, stdin);
        if (strcmp("/quit\n", buf) == 0) {
          printf("Quit received. ");
          break;
        }
      }
      if (FD_ISSET(sd, &fds)) {
        rv = fullRead(sd, buf);
        if(rv == -2) {
          break;
        }
        if(rv==-1) {
          continue;
        }
        printf("%s\n", buf);
      }
    }

  } else if (letter=='N') {
    printf("Server full. ");
  }
  printf("Closing connection\n");
  close(sd);

  exit(EXIT_SUCCESS);
}
