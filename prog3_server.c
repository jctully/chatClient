/* demo_server.c - code for example server program that uses TCP */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>

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
    //printf("converetd len %d\n", messageLen);

    if ((rv = recv(sd, message, messageLen, 0)) <0) {
      perror("recv");
      exit(EXIT_FAILURE);
    }
    printf("recv msg .%s.\n", message);

    //printf("message: \"%s\"\n", message);
    message[messageLen-1] = '\0';
    return 0;

}

void listUsernames(char usedNames[255][11]) {
    printf("Usernames:\n");
    for(int i = 0; i < 255; i++)
    {
        if(strcmp(usedNames[i], "") != 0) {
            printf("%s\n", usedNames[i]);
        }
    }
}

void initUsernames(char usedNames[255][11]) {
    for(int i = 0; i < 255; i++) {
        strncpy(usedNames[i], "", 1);
    }
}

/* -1 - T
0 - I
1 - Valid & free */
int checkUsername(char usedNames[255][11], char *username) {
    //int valid = 1;
    // Verify format of username
    int len = strlen(username);
    if(len > 10 || len < 1) {
        return 0;
    }
    char *cur = username;
    int i = 0;
    while(*cur && i < 11) {
        if(isalnum(*cur) || *cur == '_') {
            cur++;
        } else {
            return 0;
        }
        i++;
    }

    // Check if username is already in usedNames
    for(int i = 0; i < 255; i++)
    {
        if(strcmp(usedNames[i], username) == 0) {
            return -1;
        }
    }

    return 1;
}

//return 1 if name added, else -1
int addUsername (char usedNames[255][11], char *username, int i) {
  // Add username to first empty slot
  /* for(int i = 0; i < 255; i++)
  {
      if(strcmp(usedNames[i], "") == 0) {
          strcpy(usedNames[i], username);
          return 1;
      }
  } */
  strncpy(usedNames[i], username, 11);

  return 1;

}

int resetFDSet(fd_set *readfds, int sd[255], int sdl) {
    //printf("inside reset fds function\n");
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

/* int sendPrivate(char *message, char *sender, char usedUsernames[255][11]) {
    printf("sending private\n");
    char *username, *token;
    char temp[1000];
    char privateMessage[14] = "-           : ";
    const char delim = ' ';
    const char delim2 = '\0';
    int rv;

    strncpy(sender, strtok(message, &delim), 11);
    ++username;
    //printf("user = .%s.\n", username);

    //search for username, validate
    rv = checkUsername(usedUsernames, username);
    if (rv >= 0) {
      printf("Could not find user .%s.\n", username);
      return -1;
    }

    token = strtok(NULL, &delim2);
    strncpy(message, token, strlen(token));

    //prepend
    int j=strlen(sender)-1;
    for (int i=11; i>0; i--) {
      if (j>=0) {
        privateMessage[i] = sender[j];
      }
      j--;
    }
    strncpy(temp, privateMessage, 14);
    strncpy(&temp[14], message, 986);
    strncpy(message, temp, 1000);
    printf("message = .%s.\n", message);

    return 1;


} */

void publicMessage(char *message, char *username) {
  char temp[1000];
  char publicMessage[14] = ">           : ";

  int j=strlen(username)-1;
  for (int i=11; i>0; i--) {
    if (j>=0) {
      publicMessage[i] = username[j];
    }
    j--;
  }
  strncpy(temp, publicMessage, 14);
  strncpy(&temp[14], message, 986);
  strncpy(message, temp, 1000);
}

void leaveMessage(char *buf, char *username) {
  char* left = " has left";
  strncpy(buf, username, 11);
  strncpy(&buf[strlen(username)], left, 10);
  printf("leavemsg = .%s.\n", buf);
}

int main(int argc, char **argv) {
    struct protoent *ptrp; /* pointer to a protocol table entry */
    struct sockaddr_in sad_p, sad_o; /* structure to hold server's address */
    struct sockaddr_in cad; /* structure to hold client's address */
    int sd_p, sd_o, sd2[255]; /* socket descriptors */
    int port_p, port_o; /* protocol port number */
    socklen_t alen; /* length of address */
    int optval = 1; /* boolean value when we set socket option */
    char buf[1000]; /* buffer for string the server sends */
    char n = 'N', y = 'Y', t='T', invalid = 'I';

    char username[11];
    char message[MAX_LENGTH];
    struct timespec start, end;
    double timeDiff = 0;
    char usedUsernames[255][11];
    uint16_t messageLen;
    int rv, sock;
    int on = 1;

    for (int i=0; i<255; i++) {
        sd2[i] = -1;
    }

    fd_set readfds;
    FD_ZERO(&readfds);

    if( argc != 3 ) {
        fprintf(stderr,"Error: Wrong number of arguments\n");
        fprintf(stderr,"usage:\n");
        fprintf(stderr,"./server participant_port observer_port\n");
        exit(EXIT_FAILURE);
    }

    port_p = atoi(argv[1]);
    port_o = atoi(argv[2]);

    memset((char *)&sad_p,0,sizeof(sad_p)); /* clear sockaddr structure */
    memset((char *)&sad_p,0,sizeof(sad_o));

    //TODO: Set socket family to AF_INET
    sad_p.sin_family = AF_INET;
    sad_o.sin_family = AF_INET;

    //TODO: Set local IP address to listen to all IP addresses this server can assume. You can do it by using INADDR_ANY
    sad_p.sin_addr.s_addr = INADDR_ANY;
    if (port_p > 0) { /* test for illegal value */
        //TODO: set port number. The data type is u_short
        sad_p.sin_port = htons(port_p);
    } else { /* print error message and exit */
        fprintf(stderr,"Error: Bad port number %d\n",port_p);
        exit(EXIT_FAILURE);
    }

    sad_o.sin_addr.s_addr = INADDR_ANY;
    if (port_o > 0) { /* test for illegal value */
        //TODO: set port number. The data type is u_short
        sad_o.sin_port = htons(port_o);
    } else { /* print error message and exit */
        fprintf(stderr,"Error: Bad port number %d\n",port_o);
        exit(EXIT_FAILURE);
    }

    /* Map TCP transport protocol name to protocol number */
    if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
        fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
        exit(EXIT_FAILURE);
    }

    /* TODO: Create a socket with AF_INET as domain, protocol type as SOCK_STREAM, and protocol as ptrp->p_proto. This call returns a socket descriptor named sd. */
    sd_p = socket(AF_INET, SOCK_STREAM, ptrp->p_proto);
    if (sd_p < 0) {
        fprintf(stderr, "Error: Socket creation failed\n");
        exit(EXIT_FAILURE);
    }

    sd_o = socket(AF_INET, SOCK_STREAM, ptrp->p_proto);
    if (sd_o < 0) {
        fprintf(stderr, "Error: Socket creation failed\n");
        exit(EXIT_FAILURE);
    }

    /* Allow reuse of port - avoid "Bind failed" issues */
    if( setsockopt(sd_p, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ) {
        fprintf(stderr, "Error Setting socket option failed\n");
        exit(EXIT_FAILURE);
    }
    /* Allow reuse of port - avoid "Bind failed" issues */
    if( setsockopt(sd_o, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ) {
        fprintf(stderr, "Error Setting socket option failed\n");
        exit(EXIT_FAILURE);
    }

    /* TODO: Bind a local address to the socket. For this, you need to pass correct parameters to the bind function. */
    if (bind(sd_p, (struct sockaddr*) &sad_p, sizeof(sad_p)) < 0) {
        fprintf(stderr,"Error: Bind failed\n");
        exit(EXIT_FAILURE);
    }
    if (bind(sd_o, (struct sockaddr*) &sad_o, sizeof(sad_o)) < 0) {
        fprintf(stderr,"Error: Bind failed\n");
        exit(EXIT_FAILURE);
    }

    /* Makes sd (and children) nonblocking sockets*/
    rv = ioctl(sd_p, FIONBIO, (char *)&on);
    if (rv < 0)
    {
        perror("ioctl() failed");
        close(sd_p);
        exit(EXIT_FAILURE);
    }
    rv = ioctl(sd_o, FIONBIO, (char *)&on);
    if (rv < 0)
    {
        perror("ioctl() failed");
        close(sd_p);
        exit(EXIT_FAILURE);
    }

    /* TODO: Specify size of request queue. Listen take 2 parameters -- socket descriptor and QLEN, which has been set at the top of this code. */
    if (listen(sd_p, QLEN) < 0) {
        fprintf(stderr,"Error: Listen failed\n");
        exit(EXIT_FAILURE);
    }
    if (listen(sd_o, QLEN) < 0) {
        fprintf(stderr,"Error: Listen failed\n");
        exit(EXIT_FAILURE);
    }

    int i;
    // Init active usernames array
    initUsernames(usedUsernames);

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
        sock = resetFDSet(&readfds, sd2, sd_p);
        //printf("sock = %d\n", sock);
        rv = select(sock, &readfds, NULL, NULL, NULL);
        if (rv == -1) {
            perror("select"); // error occurred in select()
            exit(EXIT_FAILURE);
        }
        //printf("after select\n");

        // Accept client connections

        if (FD_ISSET(sd_p, &readfds)) {
            printf("running accept\n");
            if ((sd2[i] = accept(sd_p, (struct sockaddr *)&cad, &alen)) < 0) {
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

            // Start timer
            if (clock_gettime(CLOCK_REALTIME, &start) == -1)
            {
                perror("clock gettime");
                exit(EXIT_FAILURE);

            }
            int validName = 0;
            while (validName < 1) {
                //fullRead(sd2, &nameLen, sizeof(nameLen));
                fullRead(sd2[i], username);
                printf("name = .%s.\n", username);

                // Validate username
                validName = checkUsername(usedUsernames, username);
                if (validName == 1) {
                  addUsername(usedUsernames, username, i);
                }

                if(validName == 1) {
                    // Valid name
                    send(sd2[i], &y, sizeof(y), 0);
                    printf("sent y\n");
                } else if(validName == 0) {
                    send(sd2[i], &invalid, sizeof(invalid), 0);
                    printf("sent i\n");
                } else if(validName == -1) {
                    printf("sent t\n");
                    send(sd2[i], &t, sizeof(t), 0);
                    // reset clock
                    if (clock_gettime(CLOCK_REALTIME, &start) == -1) {
                        perror("clock gettime");
                        exit(EXIT_FAILURE);
                    }
                }
            } //end validate loop
            if (clock_gettime(CLOCK_REALTIME, &end) == -1) {
                perror("clock gettime");
                exit(EXIT_FAILURE);
            }
            timeDiff = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1E9;
            printf("Received %s in %.2lf\n", username, timeDiff);
            if(timeDiff > 10) {
                printf("Client took too long, closing connection.\n");
                close(sd2[i]);
                sd2[i]=-1;
                strncpy(usedUsernames[i], "", 1);
                numParticipants--;
                continue;
            }
            listUsernames(usedUsernames);

        } //end if new client

        for (int k=0; k<255; k++) {
          if (sd2[k]!=-1) {
              printf("sd2[%d] = %d\n", k, sd2[k]);
          }
        }
        // active participant messages
        //printf("Waiting to recv\n");
        for (int j = 0; j<255; j++) {
            if (FD_ISSET(sd2[j], &readfds)) {
                printf("sd2[%d] set\n", j);
                rv = fullRead(sd2[j], message);
                if (rv == -1) {
                    printf("Message too long, closing connection sd[%d]\n", j);
                    close(sd2[j]);
                    sd2[j] = -1;
                    strncpy(usedUsernames[j], "", 1);
                    numParticipants--;
                    break;
                }
                //printf("terminated message: .%s.\n", message);
                if (strcmp("quit", message) == 0 || rv==-2) {
                    printf("Quit received, closing connection sd[%d]\n", j);
                    close(sd2[j]);
                    sd2[j] = -1;
                    numParticipants--;

                    leaveMessage(buf, usedUsernames[j]);
                    strncpy(usedUsernames[j], "", 1);
                    messageLen = strlen(buf);

                    //send to observers
                      //send(sd, &messageLen, sizeof(messageLen), 0);
                      //send(sd, buf, messageLen, 0);


                }
                else if (message[0] == '@') {//private message
                    //rv = sendPrivate(message, usedUsernames[j], usedUsernames);
                    if (rv==1) {
                      messageLen = strlen(message);
                      //send to observers
                        //send(sd, &messageLen, sizeof(messageLen), 0);
                        //send(sd, message, messageLen, 0);
                    }

                }
                else {//public message
                  publicMessage(message, usedUsernames[j]);
                  printf("publicMessage output: \"%s\"\n", message);
                  messageLen = strlen(message);
                  //send to observers
                    //send(sd, &messageLen, sizeof(messageLen), 0);
                    //send(sd, message, messageLen, 0);


                }
            }
        }
    }
    printf("Closing connection\n");
    for (int i = 0; i<255; i++) {
      close(sd2[i]);
      sd2[i] = -1;
      strncpy(usedUsernames[i], "", 1);
      numParticipants--;
    }
}
