/* demo_server.c - code for example server program that uses TCP */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/select.h>

#define QLEN 6 /* size of request queue */
#define MAX_LENGTH 1000 //max msg len
#define MAX_PARTICIPANTS 255 //max clients
#define MAX_OBSERVERS 255
int numParticipants = 0; /* counts client connections */
int numObservers = 0;

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

int fullRead(int sd, char* message, int bytes) {
    int rv;
    uint16_t messageLen = 0;

    if ((rv = recv(sd, &messageLen, bytes, 0)) <0) {
      perror("recv");
      exit(EXIT_FAILURE);
    }
    printf("msg len: %d\n", messageLen);
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
    if (message[messageLen-1]=='\n') {
      message[messageLen-1] = '\0';
    }
    else {
      message[messageLen] = '\0';
    }
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

int resetFDSet(fd_set *readfds, int sd[255], int sd2[255], int sdPart, int sdObs) {
    //printf("inside reset fds function\n");
    FD_ZERO(readfds);
    FD_SET(sdPart, readfds);
    FD_SET(sdObs, readfds);
    int maxsd = sdPart;
    if (sdObs>sdPart){
      maxsd=sdObs;
    }
    for (int i=0; i<255; i++) {
        if (sd[i] != -1) {
            printf("sd %d in use\n", i);
            FD_SET(sd[i], readfds);
            if (sd[i] > maxsd) {
                maxsd = sd[i];
            }
        }
        if (sd2[i] != -1) {
            printf("observer sd %d in use\n", i);
            FD_SET(sd2[i], readfds);
            if (sd2[i] > maxsd) {
                maxsd = sd2[i];
            }
        }
    }
    return ++maxsd;
}

char* getPrivateUsername(char* message) {
  char username[11], *realUser;
  const char delim = ' ';

  strncpy(username, strtok(message, &delim), 11);
  realUser = &username[1];
  return realUser;
}

 int sendPrivate(char *message, char *sender, char usedUsernames[255][11], int sd[], int observerMap[]) {
    printf("sending private\n");
    char username[11], *realUser, *token;
    char temp[1000];
    char privateMessage[14] = "-           : ";
    const char delim = ' ';
    const char delim2 = '\0';
    int rv;
    uint16_t messageLen;

    realUser = getPrivateUsername(message);
    //printf("user = .%s.\n", realUser);

    //search for username, validate
    rv = checkUsername(usedUsernames, realUser);
    if (rv >= 0) {
      printf("Could not find user .%s.\n", realUser);
      return -1;
    }

    token = strtok(NULL, &delim2);
    strncpy(message, token, strlen(token));
    message[strlen(token)]='\0';

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

    int i = observerMap[findParticipantIndex(realUser, usedUsernames)];

    messageLen = strlen(message);
    //send to observers
    send(sd[i], &messageLen, sizeof(messageLen), 0);
    send(sd[i], message, messageLen, 0);

    return 1;


}

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

void broadcastToObservers(char *message, int sd[]) {
  uint16_t messageLen = strlen(message);
  //send to observers
  for (int i=0; i<255; i++) {
    if (sd[i] != -1) {
      printf("sending to observer @ sd[%d]\n", i);
      send(sd[i], &messageLen, sizeof(messageLen), 0);
      send(sd[i], message, messageLen, 0);
    }
  }
}

void leaveMessage(char *buf, char *username) {
  char* left = " has left";
  strncpy(buf, username, 11);
  strncpy(&buf[strlen(username)], left, 10);
  printf("leavemsg = .%s.\n", buf);
}

void setupSocket(int *sd, int port, struct sockaddr_in *sad) {
    struct protoent *ptrp; /* pointer to a protocol table entry */
    int optval = 1; /* boolean value when we set socket option */
    int on = 1, rv;
    memset((char *)sad,0,sizeof(*sad)); /* clear sockaddr structure */

    sad->sin_family = AF_INET;

    sad->sin_addr.s_addr = INADDR_ANY;
    if (port > 0) { /* test for illegal value */
        sad->sin_port = htons(port);
    } else { /* print error message and exit */
        fprintf(stderr,"Error: Bad port number %d\n",port);
        exit(EXIT_FAILURE);
    }

    /* Map TCP transport protocol name to protocol number */
    if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
        fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
        exit(EXIT_FAILURE);
    }

    *sd = socket(AF_INET, SOCK_STREAM, ptrp->p_proto);
    if (*sd < 0) {
        fprintf(stderr, "Error: Socket creation failed\n");
        exit(EXIT_FAILURE);
    }

    /* Allow reuse of port - avoid "Bind failed" issues */
    if( setsockopt(*sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ) {
        fprintf(stderr, "Error Setting socket option failed\n");
        exit(EXIT_FAILURE);
    }

    if (bind(*sd, (struct sockaddr*) sad, sizeof(*sad)) < 0) {
        fprintf(stderr,"Error: Bind failed\n");
        exit(EXIT_FAILURE);
    }

    /* Makes sd (and children) nonblocking sockets*/
    rv = ioctl(*sd, FIONBIO, (char *)&on);
    if (rv < 0)
    {
        perror("ioctl() failed");
        close(*sd);
        exit(EXIT_FAILURE);
    }

    if (listen(*sd, QLEN) < 0) {
        fprintf(stderr,"Error: Listen failed\n");
        exit(EXIT_FAILURE);
    }
}

int findParticipantIndex(char *username, char usedUsernames[255][11]) {
    for(int i = 0; i < 255; i++) {
        //printf("cmp %s and %s\n", username, usedUsernames[i]);
        if(strcmp(username, usedUsernames[i]) == 0) {
            return i;
        }
    }
    return -1;
}

int main(int argc, char **argv) {
    struct sockaddr_in sad_p, sad_o; /* structure to hold server's address */
    struct sockaddr_in cad; /* structure to hold client's address */
    int sd_p, sd_o, sd2[255], sd3[255]; /* socket descriptors */
    int port_p, port_o; /* protocol port number */
    socklen_t alen; /* length of address */
    char buf[1000]; /* buffer for string the server sends */
    char n = 'N', y = 'Y', t='T', invalid = 'I';
    struct timeval tv;

    char username[11];
    char message[MAX_LENGTH];
    struct timespec startP[255], endP[255], startO[255], endO[255];
    double timeDiff = 0;
    char usedUsernames[255][11];
    int rv, sock;
    int active[255];
    uint16_t messageLen;
    int observerMap[255] = { -1 };

    for (int i=0; i<255; i++) {
        sd2[i] = -1;
        sd3[i] = -1;
        observerMap[i] = -1;
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

    setupSocket(&sd_p, port_p, &sad_p);
    setupSocket(&sd_o, port_o, &sad_o);

    int i;
    int o;
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
        for (o=0; o<255; o++) {
            if (sd3[o]==-1) {
                break;
            }
        }
        printf("next open p ind = %d\n", i);
        printf("next open o ind = %d\n", o);

        alen = sizeof(cad);
        sock = resetFDSet(&readfds, sd2, sd3, sd_p, sd_o);
        //printf("sock = %d\n", sock);
        tv.tv_sec = 1;
        rv = select(sock, &readfds, NULL, NULL, &tv);
        if (rv == -1) {
            perror("select"); // error occurred in select()
            exit(EXIT_FAILURE);
        }
        //printf("after select\n");

        // Accept observer connections
        if (FD_ISSET(sd_o, &readfds)) {
          printf("running accept observer\n");
          if ((sd3[o] = accept(sd_o, (struct sockaddr *)&cad, &alen)) < 0) {
              perror("accept");
              exit(EXIT_FAILURE);
          }
          if (numObservers+1 >= MAX_OBSERVERS) {
              printf("numO = %d, no room\n", numObservers);
              send(sd3[o], &n, 1, 0);
              close(sd3[o]);
              sd3[o] = -1;
              continue;
          }
          numObservers++;
          send(sd3[o], &y, 1, 0);
          printf("Observer sd[%d] added\n", o);

          // Start timer
          if (clock_gettime(CLOCK_REALTIME, &startO[o]) == -1)
          {
              perror("clock gettime");
              exit(EXIT_FAILURE);
          }

        }

        // Accept participant connections
        if (FD_ISSET(sd_p, &readfds)) {
            printf("running accept participant\n");
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
            if (clock_gettime(CLOCK_REALTIME, &startP[i]) == -1)
            {
                perror("clock gettime");
                exit(EXIT_FAILURE);
            }

        } //end if new participant

        for (int k=0; k<255; k++) {
          if (sd2[k]!=-1) {
              printf("sd2[%d] = %d\n", k, sd2[k]);
          }
        }
        // active participant messages
        //printf("Waiting to recv\n");
        for (int j = 0; j<255; j++) {
            //Observer
            if(FD_ISSET(sd3[j], &readfds)) {
                rv = fullRead(sd3[j], username, 1);
                if (rv==-2) {
                    printf("Quit received, closing connection sd2[%d]\n", j);
                    close(sd3[j]);
                    sd3[j] = -1;
                    numObservers--;
                    observerMap[findParticipantIndex(username, usedUsernames)] == -1;
                }
                printf("username: .%s.\n", username);
                int affiliate = findParticipantIndex(username, usedUsernames);
                if(affiliate > -1) {
                    printf("Participant found!\n");
                    // Check for valid active participant
                    if (observerMap[findParticipantIndex(username, usedUsernames)] == -1) {
                      send(sd3[j], &y, 1, 0);
                      char *newObserver = "A new observer has joined.";
                      broadcastToObservers(newObserver, sd3);
                      observerMap[findParticipantIndex(username, usedUsernames)] == j;
                    }
                    else {
                      send(sd3[j], &t, 1, 0);
                    }

                    // Recv all messages send to/from sd2[affiliate]

                } else {
                    printf("Participant not found!\n");
                    send(sd3[j], &n, 1, 0);
                    close(sd3[j]);
                    sd3[j] = -1;
                    numObservers--;
                }
            }

            // Participant
            if (FD_ISSET(sd2[j], &readfds)) {
                if (active[j] == 0) {//inactive
                    printf("top of inactive\n");
                    int validName;
                    //fullRead(sd2, &nameLen, sizeof(nameLen));
                    fullRead(sd2[j], username, 1);
                    printf("name = .%s.\n", username);

                    // Validate username
                    validName = checkUsername(usedUsernames, username);
                    if (validName == 1) {
                      addUsername(usedUsernames, username, j);
                    }

                    if(validName == 1) {
                        // Valid name
                        send(sd2[j], &y, sizeof(y), 0);
                        printf("sent y\n");
                        active[j]=1;
                    } else if(validName == 0) {
                        send(sd2[j], &invalid, sizeof(invalid), 0);
                        printf("sent i\n");
                    } else if(validName == -1) {
                        printf("sent t\n");
                        send(sd2[j], &t, sizeof(t), 0);
                        // reset clock
                        if (clock_gettime(CLOCK_REALTIME, &startP[j]) == -1) {
                            perror("clock gettime");
                            exit(EXIT_FAILURE);
                        }
                    }
                    //end validate loop
                    if (clock_gettime(CLOCK_REALTIME, &endP[j]) == -1) {
                        perror("clock gettime");
                        exit(EXIT_FAILURE);
                    }
                    timeDiff = (endP[j].tv_sec - startP[j].tv_sec) + (endP[j].tv_nsec - startP[j].tv_nsec) / 1E9;
                    printf("Received %s in %.2lf\n", username, timeDiff);
                    if(timeDiff > 10) {
                        printf("Client took too long, closing connection sd[%d]. \n", j);
                        close(sd2[j]);
                        sd2[j]=-1;
                        strncpy(usedUsernames[j], "", 1);
                        numParticipants--;
                    }
                    listUsernames(usedUsernames);
                }
                else {//active
                    printf("sd2[%d] set\n", j);
                    rv = fullRead(sd2[j], message, 2);
                    if (rv == -1) {
                        printf("Message too long, closing connection sd2[%d]\n", j);
                        close(sd2[j]);
                        sd2[j] = -1;
                        strncpy(usedUsernames[j], "", 1);
                        numParticipants--;
                        break;
                    }
                    //printf("terminated message: .%s.\n", message);
                    if (strcmp("quit", message) == 0 || rv==-2) {
                        printf("Quit received, closing connection sd2[%d]\n", j);
                        close(sd2[j]);
                        sd2[j] = -1;
                        numParticipants--;

                        leaveMessage(buf, usedUsernames[j]);
                        strncpy(usedUsernames[j], "", 1);
                        broadcastToObservers(buf, sd3);



                    }
                    else if (message[0] == '@') {//private message
                        rv = sendPrivate(message, usedUsernames[j], usedUsernames, sd3, observerMap);
                        if (rv==1) {//sent
                          printf("pm sent\n");
                        }

                    }
                    else {//public message
                      publicMessage(message, usedUsernames[j]);
                      printf("publicMessage output: \"%s\"\n", message);
                      broadcastToObservers(message, sd3);
                    }
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
