/* demo_client.c - code for example client program that uses TCP */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <errno.h>


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

int main( int argc, char **argv) {
	struct hostent *ptrh; /* pointer to a host table entry */
	struct protoent *ptrp; /* pointer to a protocol table entry */
	struct sockaddr_in sad; /* structure to hold an IP address */
	int sd; /* socket descriptor */
	int port; /* protocol port number */
	char *host; /* pointer to host name */
	int n; /* number of characters read */
	char buf[1000]; /* buffer for data from the server */
	uint16_t nameLen;
    uint16_t messageLen;
	char username[10], message[255];
	char letter;
	int on = 1;
	int sock;
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
	rv = ioctl(sd, FIONBIO, (char *)&on);
	if (rv < 0)
	{
	  perror("ioctl() failed");
	  close(sd);
	  exit(EXIT_FAILURE);
	}

	FD_SET(sd, &readfds);
	sock = sd+1;

	/* TODO: Connect the socket to the specified server. You have to pass correct parameters to the connect function.*/
	if (connect(sd, (struct sockaddr*) &sad, sizeof(sad)) < 0) {
		if (errno != EINPROGRESS) {
			perror("connect");
			exit(EXIT_FAILURE);
		}
	}

	/* Repeatedly read data from socket and write to user's screen. */
	rv = select(sock, &readfds, NULL, NULL, NULL);
	if (rv == -1) {
			perror("select"); // error occurred in select()
	}

    // Check if theres room for another connection
	fullRead(sd, &letter, sizeof(letter));
	if (letter == 'Y') {
		//prompt input
		printf("Enter your username: ");
		fgets(username, 10, stdin);
		printf("\n");
		//validate name, timer
		nameLen = strlen(username);
		printf("len = %d, name = .%s.\n", nameLen, username);
		//nameLen = htons(nameLen);
		send(sd, &nameLen, sizeof(nameLen), 0);
		send(sd, username, nameLen, 0);
		printf("username sent\n");

        recv(sd, &letter, sizeof(letter), 0);
        printf("Read letter: %c\n", letter);
        if(letter == 'Y') {
            while(1) {
                printf("Enter message: ");
                fgets(message, 255, stdin);
                messageLen = strlen(message);
                send(sd, &messageLen, sizeof(messageLen), 0);
								send(sd, message, messageLen, 0);
                printf("message sent: \"%s\"\n", message);
            }
        }
	}


	close(sd);

	exit(EXIT_SUCCESS);
}
