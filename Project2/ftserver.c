/**
* ftserver.c
* Project 2
* cs372_400_w2017
* Jeromie Clark <clarkje@oregonstate.edu>
*
* Really trivial file transfer server
* - Validates the following commandline-parameters:
*   + <server_port>
*
* - Establishes a TCP control connection P on <server_port>
* - Waits on connection P for ftclient to send a command
*   + if command is invalid:
*     - sends an error message to ftclient
*   + else
*     - Initiates a data connection Q on <data_port>
*     - -l - sends a directory list to client on Q
*     - -g <filename> - validate filename and:
*       + sends contents of filename on Q
*         - or
*            + sends appropriate error message
*   + ftserver closes connection Q
*   + ftserver runs until terminated by SIGINT
*/

#include <arpa/inet.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "ftserver.h"

#define MAX_PORT_LENGTH 6       // The number of digits we'll take in the commandline port parameter
#define MAX_COMMAND_LENGTH 256  // Maximum length accepted for client-side command
#define MAX_DIR_LENGTH 65536  // Maximum length accepted for directory listings
#define BACKLOG 10              // Number of pending connections the queue will hold
#define DEBUG 1                 // Print debug messages

int main ( int argc, char *argv[]) {

  // Disable buffering on stdout for more reliable printf debugging
  if(DEBUG) {
    setbuf(stdout, NULL);
  }

  int portNum = 0;  // Port number to listen on
  int commandSocketDescriptor = 0;    // Socket descriptor for the main "command" port
  struct sigaction sa;


  // Get the port number we want to listen on
  if(DEBUG) {
    printf("Calling parseCommandlineArgs()\n");
  }
  portNum = parseCommandlineArgs(argc, argv);
  printf("Listening on Port: %d\n", portNum);

  // reap all dead processes that appear as fork()ed child proccesses exit
  // Beej's guide to network programming, pp. 29
  sa.sa_handler = sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }

  // Start listening on the supplied port
  if(DEBUG) {
    printf("Calling openSocket(%d)\n",portNum);
  }
  // Bind to the command port, get the resulting socket descriptor
  commandSocketDescriptor = openSocket(portNum);

  // Listen to the socket for commands
  listenForCommands(commandSocketDescriptor);
}

void sigchld_handler(int s) {
  // waitpid() might overwrite errno, so we save and restore it:
  int saved_errno = errno;
  while(waitpid(-1, NULL, WNOHANG) > 0);
  errno = saved_errno;
}

/*
* listenForCommands(int socketFileDescriptor)
* Listens for commands on the supplied socket file descriptor
* Processes commands sent from the client
*/

void listenForCommands(int socketFileDescriptor) {

  // Modeled on example in Beej's Guide to Network Programming, pp. 23
  struct sockaddr_storage their_addr;
  socklen_t addr_size;
  int currentFd = -1;    // The socket file descriptor for the current accepted connection
  int status;
  if(DEBUG) {
    printf("listenForCommands: calling listen(%d)\n", socketFileDescriptor);
  }

  // Listen to the supplied socket file descriptor
  if((status = listen(socketFileDescriptor, BACKLOG ) != 0)) {
    fprintf(stderr, "listenForCommands:getaddrinfo: %s\n", gai_strerror(status));
    exit(EXIT_FAILURE);
  }

  printf("ftserver: listening for connections \n");

  while(1) { // main accept() loop

    // Accept an incoming connection
    addr_size = sizeof their_addr;
    if(DEBUG) {
      printf("listenForCommands: input loop - calling accept\n");
    }
    currentFd = accept(socketFileDescriptor, (struct sockaddr *)&their_addr, &addr_size);

    // If accept returns an error, show it and exit
    if(currentFd == -1 ) {
      fprintf(stderr, "listenForCommands:accept: %s\n", gai_strerror(currentFd));
      exit(EXIT_FAILURE);
    }

    // CHILD PROCESS BEGIN
    if (fork() == 0) {

      close(socketFileDescriptor); // child doesn't need the listener

      if(DEBUG) {
        printf("listenForCommands: in child - sending\n");
      }
      // Handle commands
      handleCommands(currentFd);

      // Exit
      exit(0);
    }
    // CHILD PROCESS END
    if(DEBUG) {
      printf("listenForCommands - returned to parent\n");
    }

  close(currentFd); // parent doesn't need this
  }
  printf("listenForCommands - completed");
  return;
}

/*
* Handles commands sent from client
*/

void handleCommands(int socketFd) {

  int listBytes;

  if(DEBUG) {
    printf("handleCommands() called\n");
  }

  char dirString[MAX_DIR_LENGTH];     // directory listing output
  char inBuffer[MAX_COMMAND_LENGTH];  // client command input

  // Initialize the memory for our buffer
  memset(inBuffer, '\0', MAX_COMMAND_LENGTH);
  int numbytes = 0;

  while (strncmp("EXIT", inBuffer, 4) != 0) {

    // Beej's Guide to Network Programming, pp. 31
    if ((numbytes = recv(socketFd, inBuffer, MAX_COMMAND_LENGTH-1, 0 ) == -1)) {
      perror("handleCommands: recv() failed\n");
      exit(EXIT_FAILURE);
    }

    if (DEBUG) {
      printf("handleCommands - Command Recieved: %s\n", inBuffer);
    }

    // Handle the LIST command
    // Returns a directory listing
    // Using strncmp to minimize issues with line endings and junk data
    if (strncmp("LIST", inBuffer, 4) == 0) {

      listBytes = send(socketFd, "HELLO", 6, 0);

      // Populate dirString with the contents of the current directory
      getDirectoryListing(dirString, MAX_DIR_LENGTH-1);
      printf("returned dirString: %s", dirString);
      listBytes = send(socketFd, dirString, MAX_DIR_LENGTH, 0);
      printf("bytes sent: %d", listBytes);

    }

    // Handle the OPEN_DATA command
    // Opens a new connection to the client
    if (strncmp("OPEN_DATA", inBuffer, 9) == 0)  {
      if (DEBUG) {
        printf("handleCommands: handling OPEN_DATA command\n");
      }
    }

  }
  // Initialize the buffer again
  inBuffer[MAX_COMMAND_LENGTH] = '\0';

  if(DEBUG) {
    printf("handleCommands() exited\n");
  }
}

/*
* Writes a directory listing of the current working directory to the supplied buffer
*/

// References example code from:
// http://www.gnu.org/savannah-checkouts/gnu/libc/manual/html_node/Simple-Directory-Lister.html
// http://stackoverflow.com/questions/12489/how-do-you-get-a-directory-listing-in-c
// http://stackoverflow.com/questions/2674312/how-to-append-strings-using-sprintf

void getDirectoryListing(char* buf, int maxLen) {

  if (DEBUG) {
    printf("getDirectoryListing() called\n");
  }

  char tmpBuf[maxLen];

  // Initialize the memory for our buffer
  memset(buf, '\0', maxLen);
  memset(tmpBuf, '\0', maxLen);

  DIR *dir_p;               // Pointer to the directory handle
  struct dirent *entry_p;   // Pointer to the directory entry
  struct stat dirStat;      // stat info for the directory entry

  // Open a handle for the current working directory
  dir_p = opendir("./");

  // Loop over the results of readdir and assemble the output
  while((entry_p = readdir(dir_p)) && entry_p != NULL) {

    // Populate the dirStat struct for the current directory entry
    stat(entry_p->d_name, &dirStat);

    // Builds the string up by concatenating the contents of buf with what
    // we want to add, up to the supplied buffer lenght, maxLen to prevent overflows
    if(strncmp(buf, "\0", 1) == 0) {
      snprintf(buf, maxLen-1, "%d\t%s\n", dirStat.st_size, entry_p->d_name);
    } else {
      snprintf(tmpBuf, maxLen-1, "%s%d\t%s\n", buf, dirStat.st_size, entry_p->d_name);
      strcpy(buf, tmpBuf);
    }
  }

  if (DEBUG) {
    printf("getDirectoryListing():output\n %s\n",buf);
    printf("exited loop\n");
  }
  return;
}

/*
* Opens a socket on the supplied port
* If we can bind to the supplied port, we return a socket file pointer
* Otherwise, we just exit directly.
* TODO: Be a little more elegant about passing up error codes or something to the caller.
*/

int openSocket(int portNum) {

  // Borrowed from Beej's Guide to Network Programming, pp 16,17
  int status, sfd;
  int yes = 1;
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  char port[MAX_PORT_LENGTH];

  memset(&hints, 0, sizeof hints);  // make sure the struct is empty

  hints.ai_family = AF_UNSPEC;        // I'm not worrying about IPV6
  hints.ai_socktype = SOCK_STREAM;  // We need TCP sockets here
  hints.ai_flags = AI_PASSIVE;      // Populate the local IP automatically

  // Convert port number to string
  // Credit: http://stackoverflow.com/questions/190229/where-is-the-itoa-function-in-linux
  snprintf(port, ((MAX_PORT_LENGTH - 1) * sizeof(int)), "%d", portNum);

  // Based on example at http://man7.org/linux/man-pages/man3/getaddrinfo.3.html

  // Attempt to populate the addrinfo struct for our main listener port
  if ((status = getaddrinfo(NULL, port, &hints, &result ) != 0)) {
    // If the attempt to populate the struct fails, print the error
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    exit(EXIT_FAILURE);
  }

  // getaddrinfo() returns a list of address structures.
  // Try each address until we successfully bind().
  // If socket() or bind() fails, we close the socket and try the next address.

  for (rp = result; rp != NULL; rp = rp->ai_next) {
    sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sfd == -1)
      continue;

    // Set the socket to be reusable so it's less annoying to test.
    // Beej's guide to network programming, pp.28
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
       perror("setsockopt failed");
       exit(1);
     }

    if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
      if(DEBUG) {
        printf("openSocket(): bind was successfull\n");
      }
      break;      // ** Success **

    close(sfd);
  }

  if (rp == NULL) {  // Could not bind to any available port
    if(DEBUG) {
      printf("openSocket(): could not bind. exiting\n");
    }
    exit(EXIT_FAILURE);
  }

  // Free result struct, since it's no longer needed
  freeaddrinfo(result);

  if(DEBUG) {
    printf("openSocket(): returning file descriptor %d\n", sfd);
  }

  return sfd;
}

int parseCommandlineArgs(int argc, char* argv[]) {

  char port[MAX_PORT_LENGTH];     // Port number supplied from commandline
  int portNum = 0;

  // If the number of commandline arguments is wrong, print usage instructions
  if (argc != 2) {
    printf("Usage: ftserver.c <port>\n");
    exit(0);
  }

  // Convert the supplied value for port to an integer
  // If the conversion fails, throw an assert
  portNum = atoi(argv[1]);
  if(portNum == 0) {
    assert("The supplied value for port was not valid.  Please use a valid integer in [1024..65535] and try again.\n");
  }

  return portNum;
}
