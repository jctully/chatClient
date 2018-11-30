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

    // Check for valid active participant
		send(sd2, &y, 1, 0);
		int validName = 0;
		while (validName == 0) {
			//fullRead(sd2, &nameLen, sizeof(nameLen));
      recv(sd2, &nameLen, sizeof(nameLen), 0);
			nameLen = ntohs(nameLen);
			fullRead(sd2, username, nameLen);
			printf("len = %d, name = .%s.\n", nameLen, username);

			//validate username

			//rv = search(activeUsers, username);
      // Validate username
      rv = 0;

			if (rv == 1) {

				send(sd2, &t, sizeof(t), 0);
			}
			else {

				validName = 1;
				send(sd2, &y, sizeof(y), 0);
			}
		}	//use select here?
        while(1) {
            // active participant messages
            //fullRead(sd2, &messageLen, sizeof(messageLen));
            printf("Waiting to recv\n");
            recv(sd2, &messageLen, sizeof(messageLen), 0);
						messageLen = ntohs(messageLen);
            printf("messagelen: %d\n", messageLen);
            fullRead(sd2, message, sizeof(message));
            printf("message: %s, len: %d\n", message, messageLen);
        }

		close(sd2);
	}
}
