#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>

#define QLEN 6 /* size of request queue */
#define MAX_LENGTH 1000 //max msg len
#define MAX_PARTICIPANTS 255 //max clients
#define MAX_OBSERVERS 255
int numParticipants = 0; /* counts client connections */
int numObservers = 0;

/* Used to read first a message's length, then the message itself. returns 0
if send successful, -1 if message too long, -2 if message blank. */
int fullRead(int sd, char* message, int bytes) {
  int rv;
  uint16_t messageLen = 0;

  if ((rv = recv(sd, &messageLen, bytes, 0)) <0) {
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

// for debugging, prints all active participants' names
void listUsernames(char usedNames[255][11]) {
  printf("Usernames:\n");
  for(int i = 0; i < 255; i++)
  {
    if(strcmp(usedNames[i], "") != 0) {
      printf("%s\n", usedNames[i]);
    }
  }
}

/* intialize username array */
void initUsernames(char usedNames[255][11]) {
  for(int i = 0; i < 255; i++) {
    strncpy(usedNames[i], "", 1);
  }
}

/* Checks validity of a username. Returns -1 if username taken, 0 if username invalid,
1 if username is valid and available. */
int checkUsername(char usedNames[255][11], char *username) {
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

 /* resets the FDSet, called before a select. Returns the maximum socket + 1*/
int resetFDSet(fd_set *readfds, const int sd[255], const int sd2[255], int sdPart, int sdObs) {
  FD_ZERO(readfds);
  FD_SET(sdPart, readfds);
  FD_SET(sdObs, readfds);
  int maxsd = sdPart;
  if (sdObs>sdPart){
    maxsd=sdObs;
  }
  for (int i=0; i<255; i++) {
    if (sd[i] != -1) {
      FD_SET(sd[i], readfds);
      if (sd[i] > maxsd) {
        maxsd = sd[i];
      }
    }
    if (sd2[i] != -1) {
      FD_SET(sd2[i], readfds);
      if (sd2[i] > maxsd) {
        maxsd = sd2[i];
      }
    }
  }
  return ++maxsd;
}

/* Pulls the recipient name out of a private message */
void getPrivateUsername(char* message, char *recipient) {
  char *username;
  const char delim = ' ';
  const char delim2 = '\0';

  username = strtok(message, &delim);
  message = strtok(NULL, &delim2);

  for (int i=0; i<strlen(username)-1; i++) {
    recipient[i] = username[i+1];
  }
}

/* searches the username array for a given name, returns their index in array*/
int findParticipantIndex(char username[11], char usedUsernames[255][11]) {
  for(int i = 0; i < 255; i++) {
    if(strcmp(username, usedUsernames[i]) == 0) {
      return i;
    }
  }
  return -1;
}

/* returns the index in username array of the participant affiliated with the
  given observer index */
int findParticipantOfObserver(int j, const int observerMap[]) {
  for(int i = 0; i < 255; i++) {
    if (observerMap[i] == j) {
      return i;
    }
  }
  return -1;
}

/* formats and sends a private message to the given user*/
void sendPrivate(char *message, uint16_t messageLen, char *sender, int index, int sd[]) {
  char temp[1000];
  char privateMessage[14] = "-           : ";

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

  //send to observers
  messageLen = messageLen+14;
  send(sd[index], &messageLen, sizeof(messageLen), 0);
  send(sd[index], message, messageLen, 0);

}

/* formats a public message*/
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

/* sends given message to all observers */
void broadcastToObservers(char *message, int sd[]) {
  uint16_t messageLen = strlen(message);
  //send to observers
  for (int i=0; i<255; i++) {
    if (sd[i] != -1) {
      send(sd[i], &messageLen, sizeof(messageLen), 0);
      send(sd[i], message, messageLen, 0);
    }
  }
}

/* creates a message for when a participant disconnects */
void leaveMessage(char *buf, char *username) {
  char* left = " has left";
  strncpy(buf, username, 11);
  strncpy(&buf[strlen(username)], left, 10);
}

/* initialize listening sockets */
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
  int activeP[255];
  int activeO[255];
  uint16_t messageLen;
  int observerMap[255];

  //init arrays
  for (int i=0; i<255; i++) {
    sd2[i] = -1;
    sd3[i] = -1;
    observerMap[i] = -1;
    activeP[i] = -1;
    activeO[i] = -1;
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

  int i; //iterator for participant array
  int o; //for observer array

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

    alen = sizeof(cad);
    //run select, block clients until characters in buffer to be read
    sock = resetFDSet(&readfds, sd2, sd3, sd_p, sd_o);
    tv.tv_sec = 1;
    rv = select(sock, &readfds, NULL, NULL, &tv);
    if (rv == -1) {
      perror("select"); // error occurred in select()
      exit(EXIT_FAILURE);
    }

    // Accept observer connections
    if (FD_ISSET(sd_o, &readfds)) {
      if ((sd3[o] = accept(sd_o, (struct sockaddr *)&cad, &alen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
      }
      if (numObservers+1 >= MAX_OBSERVERS) {
        send(sd3[o], &n, 1, 0);
        close(sd3[o]);
        sd3[o] = -1;
        continue;
      }
      numObservers++;
      send(sd3[o], &y, 1, 0);
      activeO[o] = 0;

      // Start timer
      if (clock_gettime(CLOCK_REALTIME, &startO[o]) == -1)
      {
        perror("clock gettime");
        exit(EXIT_FAILURE);
      }
    }

    // Accept participant connections
    if (FD_ISSET(sd_p, &readfds)) {
      if ((sd2[i] = accept(sd_p, (struct sockaddr *)&cad, &alen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
      }

      if (numParticipants+1 >= MAX_PARTICIPANTS) {
        send(sd2[i], &n, 1, 0);
        close(sd2[i]);
        sd2[i] = -1;
        continue;
      }

      numParticipants++;
      send(sd2[i], &y, 1, 0);
      activeP[i] = 0;

      // Start timer
      if (clock_gettime(CLOCK_REALTIME, &startP[i]) == -1)
      {
        perror("clock gettime");
        exit(EXIT_FAILURE);
      }

    }

    // check for incoming  messages
    for (int j = 0; j<255; j++) {
      //Observer
      if(FD_ISSET(sd3[j], &readfds)) {
        rv = fullRead(sd3[j], username, 1);
        if (strcmp("/quit", username) == 0 || rv==-2) {
          printf("Quit received, closing connection sd3[%d]\n", j);
          close(sd3[j]);
          sd3[j] = -1;
          numObservers--;
          observerMap[findParticipantIndex(username, usedUsernames)] = -1;
          continue;
        }

        int affiliate = findParticipantIndex(username, usedUsernames);
        if(affiliate > -1) { //affiliate found
          // Check for valid active participant
          if (observerMap[findParticipantIndex(username, usedUsernames)] == -1) {
            send(sd3[j], &y, 1, 0);
            activeO[j] = 1;

            char *newObserver = "A new observer has joined.";
            broadcastToObservers(newObserver, sd3);
            observerMap[findParticipantIndex(username, usedUsernames)] = j;
          }
          else {
            send(sd3[j], &t, 1, 0);
          }

        } else {//affiliate not found
          send(sd3[j], &n, 1, 0);
          close(sd3[j]);
          sd3[j] = -1;
          numObservers--;
        }
      }

      // Participant messages
      if (FD_ISSET(sd2[j], &readfds)) {
        if (activeP[j] == 0) {//inactive
          int validName;
          fullRead(sd2[j], username, 1);

          // Validate username
          validName = checkUsername(usedUsernames, username);
          if (validName == 1) {
            strncpy(usedUsernames[j], username, 11);

          }

          if(validName == 1) {
            // Valid name
            send(sd2[j], &y, sizeof(y), 0);
            activeP[j]=1;
          } else if(validName == 0) {
            send(sd2[j], &invalid, sizeof(invalid), 0);
          } else if(validName == -1) {
            send(sd2[j], &t, sizeof(t), 0);
            // reset clock
            if (clock_gettime(CLOCK_REALTIME, &startP[j]) == -1) {
              perror("clock gettime");
              exit(EXIT_FAILURE);
            }
          }

        }
        else {//active
          rv = fullRead(sd2[j], message, 2);
          if (rv == -1) {
            printf("Message too long, closing connection sd2[%d]\n", j);
            close(sd2[j]);
            sd2[j] = -1;
            strncpy(usedUsernames[j], "", 1);
            numParticipants--;
            break;
          }
          if (strcmp("/quit", message) == 0 || rv==-2) {
            printf("Quit received, closing connection sd2[%d]\n", j);
            close(sd2[j]);
            sd2[j] = -1;
            numParticipants--;

            leaveMessage(buf, usedUsernames[j]);
            strncpy(usedUsernames[j], "", 1);
            broadcastToObservers(buf, sd3);



          }
          else if (message[0] == '@') {//private message
            char recipient[11];
            char privateMessage[1000];
            int iter;
            //pull out recipient username
            while(message[iter] != ' ') {
              recipient[iter-1] = message[iter];
              iter++;
            }
            iter++;
            messageLen = 0;
            //pull out message body
            for (int i=0; i<1000; i++) {
              if (message[iter] != '\0') {
                privateMessage[i] = message[iter++];
                messageLen++;
              }
              else {
                break;
              }
            }

            rv = findParticipantIndex(recipient, usedUsernames);
            if(rv == -1) {
              continue;
            }
            int obs = observerMap[rv];

            sendPrivate(privateMessage, messageLen, usedUsernames[j], obs, sd3);

          }
          else {//public message
            publicMessage(message, usedUsernames[j]);
            broadcastToObservers(message, sd3);
          }
        }
      }

      //timeout participants
      if(activeP[j] == 0 && sd2[j] != -1) {
        if (clock_gettime(CLOCK_REALTIME, &endP[j]) == -1) {
          perror("clock gettime");
          exit(EXIT_FAILURE);
        }
        //calculate time difference
        timeDiff = (endP[j].tv_sec - startP[j].tv_sec) + (endP[j].tv_nsec - startP[j].tv_nsec) / 1E9;
        if(timeDiff > 10) {
          printf("Client took too long, closing connection sd2[%d]. \n", j);
          close(sd2[j]);
          sd2[j]=-1;
          strncpy(usedUsernames[j], "", 1);
          numParticipants--;
        }
      }
      //timeout observers
      if(activeO[j] == 0 && sd3[j] != -1) {
        if (clock_gettime(CLOCK_REALTIME, &endO[j]) == -1) {
          perror("clock gettime");
          exit(EXIT_FAILURE);
        }
        timeDiff = (endO[j].tv_sec - startO[j].tv_sec) + (endO[j].tv_nsec - startO[j].tv_nsec) / 1E9;
        if(timeDiff > 10) {
          printf("Client took too long, closing connection sd3[%d]. \n", j);
          close(sd3[j]);
          sd3[j]=-1;
          strncpy(usedUsernames[j], "", 1);
          numObservers--;
          observerMap[findParticipantOfObserver(j,observerMap)] = -1;
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
