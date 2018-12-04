/* demo_server.c - code for example server program that uses TCP */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>

struct Trie *getNewTrieNode();
void insert(struct Trie **head, char *str);
int search(struct Trie *head, char *str);
int haveChildren(struct Trie *curr);
int deletion(struct Trie **curr, char *str);

#define QLEN 6 /* size of request queue */
#define MAX_LENGTH 1000 //max msg len
#define MAX_PARTICIPANTS 255 //max participants
int numParticipants = 0; /* counts client connections */

/*------------------------------------------------------------------------
* Program: demo_server
*
* Purpose: allocate a socket and then repeatedly execute the following:
* (1) wait for the next connection from a client
* (2) send a short message to the client
* (3) close the connection
* (4) go back to step (1)
*
* Syntax: ./demo_server port
*
* port - protocol port number to use
*
*------------------------------------------------------------------------
*/

int fullRead(int sd, char* message) {
  uint16_t messageLen = 0;

  recv(sd, &messageLen, sizeof(messageLen), 0);
  if (messageLen > MAX_LENGTH) {
    return -1;
  }
  //printf("converetd len %d\n", messageLen);
  recv(sd, message, messageLen, 0);
  //printf("message: \"%s\"\n", message);
  message[messageLen-1] = '\0';
  return 0;

}

int resetFDSet(fd_set *readfds, int sd[255], int sdl) {
  printf("inside reset fds function\n");
  FD_ZERO(readfds);
  FD_SET(sdl, readfds);
  int maxsd = sdl;
  for (int i=0; i<255; i++) {
    if (sd[i] != -1) {
      printf("sd %d in use\n", i);
      FD_SET(sd[i], readfds);
      if (sd[i] > maxsd) {
        maxsd = sd[i];
      }
    }
  }
  return ++maxsd;
}

void sendPrivate(char *message) {
  printf("sending private\n");
  char *username, *copy, *token;
  const char delim[2] = " ";

  token = strtok(message, delim);
  username = token++;
  printf("user = .%s.\n", username);
  //search for username, validate

  //TODO: pull message from input str
  message = message + strlen(username);
  printf("message = .%s.\n", message);


}

int main(int argc, char **argv) {
  struct protoent *ptrp; /* pointer to a protocol table entry */
  struct sockaddr_in sad; /* structure to hold server's address */
  struct sockaddr_in cad; /* structure to hold client's address */
  int sd, sdt, sd2[255]; /* socket descriptors */
  int port; /* protocol port number */
  int alen; /* length of address */
  int optval = 1; /* boolean value when we set socket option */
  char buf[1000]; /* buffer for string the server sends */
  char n = 'N', y = 'Y', t='T', invalid = 'I', *token;

  char username[10];
  char message[MAX_LENGTH];
  uint16_t nameLen, messageLen;
  int rv, sock;
  int on = 1;
  struct Trie *activeUsers;;;

  for (int i=0; i<255; i++) {
    sd2[i] = -1;
  }

  fd_set readfds;
  FD_ZERO(&readfds);

  activeUsers = getNewTrieNode();

  if( argc != 3 ) {
    fprintf(stderr,"Error: Wrong number of arguments\n");
    fprintf(stderr,"usage:\n");
    fprintf(stderr,"./server participant_port observer_port\n");
    exit(EXIT_FAILURE);
  }

  int pPort = atoi(argv[1]);
  int oPort = atoi(argv[2]);

  memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */

  //TODO: Set socket family to AF_INET
  sad.sin_family = AF_INET;

  //TODO: Set local IP address to listen to all IP addresses this server can assume. You can do it by using INADDR_ANY
  sad.sin_addr.s_addr = INADDR_ANY;
  port = atoi(argv[1]); /* convert argument to binary */
  if (port > 0) { /* test for illegal value */
    //TODO: set port number. The data type is u_short
    sad.sin_port = htons(port);
  } else { /* print error message and exit */
    fprintf(stderr,"Error: Bad port number %s\n",argv[1]);
    exit(EXIT_FAILURE);
  }

  /* Map TCP transport protocol name to protocol number */
  if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
    fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
    exit(EXIT_FAILURE);
  }

  /* TODO: Create a socket with AF_INET as domain, protocol type as SOCK_STREAM, and protocol as ptrp->p_proto. This call returns a socket descriptor named sd. */
  sd = socket(AF_INET, SOCK_STREAM, ptrp->p_proto);
  if (sd < 0) {
    fprintf(stderr, "Error: Socket creation failed\n");
    exit(EXIT_FAILURE);
  }

  /* Allow reuse of port - avoid "Bind failed" issues */
  if( setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ) {
    fprintf(stderr, "Error Setting socket option failed\n");
    exit(EXIT_FAILURE);
  }

  /* TODO: Bind a local address to the socket. For this, you need to pass correct parameters to the bind function. */
  if (bind(sd, (struct sockaddr*) &sad, sizeof(sad)) < 0) {
    fprintf(stderr,"Error: Bind failed\n");
    exit(EXIT_FAILURE);
  }

  /* Makes sd (and children) nonblocking sockets*/
  rv = ioctl(sd, FIONBIO, (char *)&on);
  if (rv < 0)
  {
    perror("ioctl() failed");
    close(sd);
    exit(EXIT_FAILURE);
  }

  /* TODO: Specify size of request queue. Listen take 2 parameters -- socket descriptor and QLEN, which has been set at the top of this code. */
  if (listen(sd, QLEN) < 0) {
    fprintf(stderr,"Error: Listen failed\n");
    exit(EXIT_FAILURE);
  }

  int i;

  /* Main server loop - accept and handle requests */
  while (1) {
    //update next open slot
    for (i=0; i<255; i++) {
      if (sd2[i]==-1) {
        break;
      }
    }
    printf("next open ind = %d\n", i);

    alen = sizeof(cad);
    sock = resetFDSet(&readfds, sd2, sd);
    printf("sock = %d\n", sock);
    rv = select(sock, &readfds, NULL, NULL, NULL);
    if (rv == -1) {
      perror("select"); // error occurred in select()
      exit(EXIT_FAILURE);
    }
    //printf("after select\n");

    // Accept client connections

    if (FD_ISSET(sd, &readfds)) {
      printf("running accept\n");
      if ((sd2[i] = accept(sd, (struct sockaddr *)&cad, &alen)) < 0) {
         perror("accept");
         exit(EXIT_FAILURE);
      }

      if (numParticipants+1 >= MAX_PARTICIPANTS) {
        printf("numP = %d, no room\n", numParticipants);
        send(sd2[i], &n, 1, 0);
        close(sd2[i]);
        sd2[i] = -1;
        continue;
      }

      numParticipants++;
      // Check for valid active participant
      send(sd2[i], &y, 1, 0);
      int validName = 0;
      while (validName == 0) {
        //fullRead(sd2, &nameLen, sizeof(nameLen));
        fullRead(sd2[i], username);
        printf("name = .%s.\n", username);

        // Validate username
        rv = 0;

        if (rv == 1) {

          send(sd2[0], &t, sizeof(t), 0);
        }
        else {

          validName = 1;
          send(sd2[0], &y, sizeof(y), 0);
        }
      } //end validate loop

   } //end if new client

    printf("sd2[0] = %d\n", sd2[0]);

    // active participant messages
    //printf("Waiting to recv\n");
    for (int j = 0; j<255; j++) {
      if (FD_ISSET(sd2[j], &readfds)) {
        printf("sd2[%d] set\n", j);
        if (rv = fullRead(sd2[j], message) < 0) {
          printf("Message too long, closing connection sd[%d]\n", j);
          close(sd2[j]);
          sd2[j] = -1;
          numParticipants--;
          break;
        }
        printf("terminated message: .%s.\n", message);
        /*if (strlen(message) > MAX_LENGTH) {
          printf("Message too long, closing connection sd[%d]\n", j);
          close(sd2[j]);
          sd2[j] = -1;
          numParticipants--;
          break;
        }*/
        if (strcmp("quit", message) == 0) {
          printf("Quit received, closing connection sd[%d]\n", j);
          close(sd2[j]);
          sd2[j] = -1;
          numParticipants--;
        }
        else if (message[0] == '@') {
          sendPrivate(message);
        }
      }
    }

  }
  printf("Closing connection\n");
  close(sd2[0]);
  sd2[0] = -1;
  numParticipants--;
}
