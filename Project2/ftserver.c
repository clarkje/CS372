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

#define MIN_DATA_PORT 20201     // The first port number we'll try to bind to when creating a listener for file data
#define MAX_PORT_LENGTH 6       // The number of digits we'll take in the commandline port parameter
#define MAX_FILENAME_LENGTH 255 // Maximum length accepted for a filename
#define MAX_COMMAND_LENGTH 256  // Maximum length accepted for client-side command
#define MAX_DIR_LENGTH 65536    // Maximum length accepted for directory listings
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
  if (commandSocketDescriptor == -1) {
    printf("Unable to bind to supplied socket.  Exiting");
    exit(EXIT_FAILURE);
  }

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
  int currentFd = -1;    // Socket descriptor for the current command connection
  int dataFd = -1;       // Socket descriptor for the data connection
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
* Negotiate a socket connection for data transfer on a client-supplied port
*/

int establishDataConnection(int socketFd) {

  int numbytes;
  socklen_t len;
  struct sockaddr_storage addr;
  struct sockaddr_storage clientAddr;   // Client socket info
  char* inBuffer[MAX_COMMAND_LENGTH];   // Client Input Buffer
  char* inPort[MAX_PORT_LENGTH];        // Client-supplied Port


  send(socketFd, "HELLO", 5, 0);

  // Beej's Guide to Network Programming, pp. 31
  if ((numbytes = recv(socketFd, inBuffer, MAX_COMMAND_LENGTH-1, 0 ) == -1)) {
    perror("establishDataConnection: recv() failed\n");
    exit(EXIT_FAILURE);
  }

  if (strncmp("DATA_PORT", *inBuffer, 9) == 0) {

    // Copy the characters after "_DATA_PORT" into the buffer for the filename
    // Pass in a pointer to the 9th character in inBuffer
    strncpy(*inBuffer, inPort[8], MAX_PORT_LENGTH);

    // I get that this truncates the string wherever the non-alpha character arrives.
    // It's good enough for now.
    // TODO: improve the whitespace-in-front case...
    // Inspriation from: http://stackoverflow.com/questions/16431858/removing-non-alpha-characters-in-c

    int i;
    for (i = 0; i < sizeof(inPort); i++) {
      if ((*inPort[i] < '0' || *inPort[i] > '9') && *inPort[i] != '\0') {
        *inPort[i] = '\0';
      }
    }

    // Get the client's IP
    // http://beej.us/guide/bgnet/output/html/multipage/getpeernameman.html
    len = sizeof addr;
    getpeername(socketFd, (struct sockaddr*)&addr, &len);
    struct sockaddr_in *clientAddr = (struct sockaddr_in *)&addr;

  }
  // return dataFd;
  return 0;
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
  char inFile[MAX_FILENAME_LENGTH];   // max length for a filename

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
    // Returns a directory listing of the current working directory on the server
    // Using strncmp to minimize issues with line endings and junk data
    if (strncmp("-l", inBuffer, 2) == 0) {
      // Populate dirString with the contents of the current directory
      getDirectoryListing(dirString, MAX_DIR_LENGTH-1);
      send(socketFd, dirString, MAX_DIR_LENGTH, 0);
    }

    // Client Command: -g <filename>
    // Retrieve a file from the current working directory on the server
    if (strncmp("-g", inBuffer, 2) == 0) {

      // Copy the characters after "-g " into the buffer for the filename
      // Pass in a pointer to the 5th character in inBuffer
      strncpy(inFile, &inBuffer[3], MAX_FILENAME_LENGTH);

      // I get that this truncates the string wherever the non-alpha character arrives.
      // It's good enough for now.
      // TODO: improve the whitespace-in-front case...
      // Inspriation from: http://stackoverflow.com/questions/16431858/removing-non-alpha-characters-in-c

      int i;
      for (i = 0; i < sizeof(inFile); i++) {
        if ((inFile[i] < 'A' || inFile[i] > 'z') && inFile[i] != '.' && inFile[i] != '\0') {
          inFile[i] = '\0';
        }
      }

      if (sendFile(socketFd, inFile) != 0) {
        // Something bad happened
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
* Transmits a file to the client
* Returns number of bytes transmitted, or -1 on error
*/

int sendFile(int socketFd, char* filename) {

  // If there's no file, we can't do anything anyway
  // Just send an error to the client and return an error code
  if (fileExists(filename) == 0) {
    send(socketFd, "ERROR_FILE_NOT_FOUND", 20, 0);
    return 1;
  }
}

/*
* Determines whether or not a file exists.
*/

int fileExists(char *filename) {

  DIR *dir_p;               // Pointer to the directory handle
  struct dirent *entry_p;    // Pointer to the directory entry_p

  dir_p = opendir("./");
  while ((entry_p = readdir(dir_p)) && entry_p != NULL) {

    printf("checking %s against %s...\n", entry_p->d_name, filename);

    if (strcmp(entry_p->d_name, filename ) == 0) {
      return 1;
    }
  }
  return 0;
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
* Otherwise, we return -1
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
    return -1;
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
       return -1;
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
    return -1;
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
