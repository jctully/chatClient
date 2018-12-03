/* demo_server.c - code for example server program that uses TCP */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctype.h>
#include <netdb.h>
#include <stdio.h>
#include <time.h>
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

void fullRead(int sd, char* message) {
    uint16_t messageLen = 0;

    recv(sd, &messageLen, sizeof(messageLen), MSG_WAITALL);
    //printf("converetd len %d\n", messageLen);
    recv(sd, message, messageLen, MSG_WAITALL);
    //printf("message: \"%s\"\n", message);
    message[messageLen-1] = '\0';

}

void listUsernames(char usedNames[255][10]) {
    printf("Usernames:\n");
    for(int i = 0; i < 255; i++)
    {
        if(strcmp(usedNames[i], "") != 0)
        printf("%s\n", usedNames[i]);
    }
}

void initUsernames(char usedNames[255][10]) {
    char blank = '\0';
    for(int i = 0; i < 255; i++)
    {
        strcpy(usedNames[i], &blank);
    }
}

/* -1 - T
0 - I
1 - Y */
int addUsername(char usedNames[255][10], char *username) {
    //int valid = 1;
    // Verify format of username
    char *cur = username;
    int i = 0;
    while(*cur && i < 10) {
        if(isalnum(*cur) || *cur == '_') {
            *cur++;
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

    // Add username to first empty slot
    for(int i = 0; i < 255; i++)
    {
        if(strcmp(usedNames[i], "") == 0) {
            strcpy(usedNames[i], username);
            break;
        }
    }

    return 1;
}

int main(int argc, char **argv) {
    struct protoent *ptrp; /* pointer to a protocol table entry */
    struct sockaddr_in sad; /* structure to hold server's address */
    struct sockaddr_in cad; /* structure to hold client's address */
    int sd, sd2; /* socket descriptors */
    int port; /* protocol port number */
    int alen; /* length of address */
    int optval = 1; /* boolean value when we set socket option */
    char buf[1000]; /* buffer for string the server sends */
    int MAX_PARTICIPANTS = 255;
    char n = 'N', y = 'Y', t='T', invalid = 'I';
    char username[10];
    char message[255];
    struct timespec start, end;
    double timeDiff = 0;
    char usedUsernames[255][10];
    uint16_t nameLen, messageLen;
    int rv, sock;
    int on = 1;
    struct Trie *activeUsers;

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
    FD_SET(sd, &readfds);

    /* Allow reuse of port - avoid "Bind failed" issues */
    if( setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ) {
        fprintf(stderr, "Error Setting socket option failed\n");
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

    /* TODO: Bind a local address to the socket. For this, you need to pass correct parameters to the bind function. */
    if (bind(sd, (struct sockaddr*) &sad, sizeof(sad)) < 0) {
        fprintf(stderr,"Error: Bind failed\n");
        exit(EXIT_FAILURE);
    }

    /* TODO: Specify size of request queue. Listen take 2 parameters -- socket descriptor and QLEN, which has been set at the top of this code. */
    if (listen(sd, QLEN) < 0) {
        fprintf(stderr,"Error: Listen failed\n");
        exit(EXIT_FAILURE);
    }

    /* Main server loop - accept and handle requests */
    sock = sd+1;
    while (1) {
        alen = sizeof(cad);
        rv = select(sock, &readfds, NULL, NULL, NULL);
        if (rv == -1) {
            perror("select"); // error occurred in select()
        }

        // Accept client connections
        if ((sd2 = accept(sd, (struct sockaddr *)&cad, &alen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        FD_SET(sd2, &readfds);
        sock = sd2+1;

        numParticipants++;
        if (numParticipants > MAX_PARTICIPANTS) {
            send(sd2, &n, 1, 0);
            close(sd2);
            numParticipants--;
        }

        // Init active usernames array
        initUsernames(usedUsernames);
        // Check for valid active participant
        send(sd2, &y, 1, 0);

        // Start timer
        if (clock_gettime(CLOCK_REALTIME, &start) == -1)
        {
            perror("clock gettime");
            exit(EXIT_FAILURE);
        }

        int validName = 0;
        while (validName < 1) {
            //fullRead(sd2, &nameLen, sizeof(nameLen));
            fullRead(sd2, username);
            printf("name = .%s.\n", username);

            //validate username

            /* -1 - T
            0 - I
            1 - Y */
            validName = addUsername(usedUsernames, username);
            if(validName == 1) {
                // Valid name
                send(sd2, &y, sizeof(y), 0);
                printf("sent y\n");
            } else if(validName == 0) {
                send(sd2, &invalid, sizeof(invalid), 0);
                printf("sent i\n");
            } else if(validName == -1) {
                send(sd2, &t, sizeof(t), 0);
                // reset clock
                if (clock_gettime(CLOCK_REALTIME, &start) == -1) {
                    perror("clock gettime");
                    exit(EXIT_FAILURE);
                }
                printf("sent t\n");
            }
        }	//use select here?
        if (clock_gettime(CLOCK_REALTIME, &end) == -1) {
            perror("clock gettime");
            exit(EXIT_FAILURE);
        }
        timeDiff = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1E9;
        printf("Received %s in %.2lf\n", username, timeDiff);
        if(timeDiff > 10) {
            printf("Client took too long, closing connection.\n");
            close(sd2);
            numParticipants--;
            continue;
        }
        listUsernames(usedUsernames);
        while(1) {
            // active participant messages
            //fullRead(sd2, &messageLen, sizeof(messageLen));
            printf("Waiting to recv\n");
            //recv(sd2, &messageLen, sizeof(messageLen), 0);
            //messageLen = ntohs(messageLen);
            //printf("messagelen: %d\n", messageLen);
            fullRead(sd2, message);
            printf("terminated message: .%s.\n", message);
        }

        close(sd2);
    }
}
