/*
 * chatclient.c
 * CS372_400_W2016 - Project 1
 * Jeromie Clark <clarkje@oregonstate.edu>
 *
 * 1.) Connects to a chat server
 * 2.) Takes hostname and port number on the commandline
 * 3.) On launch, prompts the user for a "handle"
 * 4.) Handle is displayed in a prompt and will be pre-pended to all messages
 *     e.g. "SteveO> Hi!!"
 * 5.) chatclient sends a message to chatserve on supplied host and port
 * 6.) \quit closes the connection to the server (and exits)
 *
 * Other Requirements:
 * - Should be able to send up to 500 chars
 * - Must run on flip
 * - Run in arbitrary directories
 * - Appropriate Citations
 * - Include a detailed README wth instructions on how to compile, execute  and run program
 * - Combine all files into a .zip file with no folders.
 *
 * Loosely modeled some of the parameter handling code after:
 * https://en.wikibooks.org/wiki/A_Little_C_Primer/C_Command_Line_Arguments
 *
 * Client socket code is modeled on:
 * http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
 */

#include "chatclient.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define DEBUG 0
#define MAX_ARG_LENGTH 256
#define MAX_RESPONSE_LENGTH 512
#define MAX_MESSAGE_LENGTH 500
#define MAX_HANDLE_LENGTH 11
#define HANDSHAKE "HELLO"
#define GOODBYE "\\quit"

int main( int argc, char *argv[] )
{
  // Socket File Descriptor
  int sockfd;
  // Prepares socket address structs for subsequent use
  struct addrinfo hints;
  // Will point to the results
  struct addrinfo *servinfo;
  // Will point to the selected socket
  struct addrinfo *p;
  // Return Value of socket()
  int returnValue;
  // Number of bytes in response
  int numbytes = 0;

  // User's Handle (display name for chat transcript)
  char userHandle[MAX_HANDLE_LENGTH];
  // Remote Handle
  char remoteHandle[MAX_HANDLE_LENGTH];

  // Server Hostname
  char hostname[MAX_ARG_LENGTH];
  // Server Port
  char port[MAX_ARG_LENGTH];
  // Buffer for incoming data
  char buf[MAX_RESPONSE_LENGTH];
  // Stores the address of the sender
  struct sockaddr addr;
  socklen_t fromlen;


  // If the number of commandline arguments is wrong, barf
  if (argc != 3) {
    printf("Usage: chatclient <hostname> <port>");
    return(1);
  }

  // Copy the commandline args into named strings, just because
  strncpy(hostname, argv[1], MAX_ARG_LENGTH);
  strncpy(port, argv[2], MAX_ARG_LENGTH);

  // Let's connect to the remote server
  printf("Connecting to %s:%s\n", hostname, port);

  // Make sure the hints struct is empty
  memset(&hints, 0, sizeof hints);

  // We want an IPv4 connection, and TCP DGRAM sockets
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  // Attempt to populate the servinfo struct for the supplied host and port
  // If we don't get a valid result back, object and exit
  if ((returnValue = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
     fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(returnValue));
     return 1;
  }

  // getaddrinfo() returns a pointer to a linked list of one or more struct addrinfos
  // Loop through the linked list until we successfully create a socket
   for(p = servinfo; p != NULL; p = p->ai_next) {
       // Returns a socket descriptor that we can use
       if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
           perror("chatclient: invalid socket, retrying");
           continue;
       }
       // Try to connect to it
       if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
         close(sockfd);
         perror("chatclient: connect failed");
         continue;
       }
       // We have a valid connection.  Proceed.
       break;
   }

   // If we didn't end up with a valid socket, object and exit
   if (p == NULL) {
       fprintf(stderr, "chatclient: failed to create socket\n");
       return 2;
   }

   // Perform a handshake with the chat client
   if (doHandshake(&sockfd) != 0) {
     // Handshake Failed - Shutdown gracefully
     freeaddrinfo(servinfo);
     close(sockfd);
     return 1;
   }

   // Get the user's handle
   getHandleFromKeyboard(userHandle);

   // Exchange Handles
   doHandleExchange(&sockfd, userHandle, remoteHandle);

   // Do the chat loop.
   doChat(&sockfd, userHandle, remoteHandle);

   // End connection with server gracefully
   if (doGoodbye(&sockfd) != 0) {
     freeaddrinfo(servinfo);
     close(sockfd);
     return 1;
   }

   // Release the servinfo struct
   freeaddrinfo(servinfo);
   close(sockfd);
   return 0;
}

// getHandleFromKeyboard(char* handle)
// Prompts the user for a handle

void getHandleFromKeyboard(char* handle) {

  // Allocate a much larger buffer than we need
  // It can theoretically still overflow, but this is trivial homework code...
  char temp[MAX_RESPONSE_LENGTH] = "";

  if(DEBUG) {
    printf("getHandleFromKeyboard - Prompting for input\n");
  }

   while(strlen(temp) < 1 || strlen(temp) > MAX_HANDLE_LENGTH-1) {
    printf("Please enter your handle (up to 10 chars):\n");
    scanf("%s", &temp);
  }
  strncpy(handle, temp, MAX_HANDLE_LENGTH-1);
  handle[MAX_HANDLE_LENGTH] = 0;

  if(DEBUG) {
    printf("getHandleFromKeyboard - Handle Received: %s\n");
  }
}

// doGoodbye(int* sockfd)
// Says goodbye to the chat server

int doGoodbye(int* sockfd) {

  if(DEBUG) {
    printf("\ndoGoodbye - Say bye y'all\n");
  }

  // Buffer for incoming data
  char buf[MAX_RESPONSE_LENGTH];
  // Number of returned bytes
  int numbytes = 0;
  char goodbye[] = GOODBYE;

  // Send Handshake to Server
  if ((numbytes = send(*sockfd, goodbye, strlen(goodbye), 0)) == -1) {
      perror("chatclient: handshake sendto() failed");
      close(*sockfd);
      exit(1);
  }

  if(DEBUG) {
    printf("\ndoGoodbye - Bye Y'all\n");
  }
}

// doHandshake(int* sockfd)
// Peforms an initial handshake with the chat server

int doHandshake(int* sockfd) {

  // Buffer for incoming data
  char buf[MAX_RESPONSE_LENGTH];
  // Number of returned bytes
  int numbytes = 0;

  char handshake[] = HANDSHAKE;

  if (DEBUG) {
    printf("doHandshake() - Sending Handshake \n");
  }

  // Send Handshake to Server
  if ((numbytes = send(*sockfd, handshake, strlen(handshake), 0)) == -1) {
      perror("chatclient: handshake sendto() failed");
      close(*sockfd);
      exit(1);
  }

  if (DEBUG) {
    printf("doHandshake() - Listening for Handshake \n");
  }

  // Validate the response
  numbytes = 0;
  memset(&buf, 0, sizeof(buf));
  if((numbytes = recv(*sockfd, buf, sizeof buf, 0)) == -1) {
    perror("chatclient: handshake recv() failed");
    exit(1);
  }

  if (DEBUG) {
    printf("doHandshake() - Handshake Received \n");
  }

  // Confirm that the client returned "HELLO"
  if (strcmp(handshake, buf) == 0) {
    if (DEBUG) {
      printf("Handshake Successful \n");
    }
    return 0;
  } else {
    perror("chatclient: handshake failed - unexpected response");
    return 1;
  }

  return 0;
}


// doHandleExchange(int* sockfd, char* userHandle, char* clientHandle)
// Peforms an initial handshake with the chat server

int doHandleExchange(int* sockfd, char* userHandle, char* remoteHandle) {

  // Buffer for incoming data
  char buf[MAX_RESPONSE_LENGTH];

  // Number of returned bytes
  int numbytes = 0;

  if (DEBUG) {
    printf("doHandleExchange() - Sending Handle \n");
  }

  // Send Handle to Server
  if ((numbytes = send(*sockfd, userHandle, strlen(userHandle), 0)) == -1) {
      perror("chatclient: doHandleExchange send() failed");
      close(*sockfd);
      exit(1);
  }

  if (DEBUG) {
    printf("doHandleExchange() - Listening for remote handle \n");
  }

  // Validate the response
  numbytes = 0;
  memset(&buf, 0, sizeof(buf));
  if((numbytes = recv(*sockfd, buf, sizeof buf, 0)) == -1) {
    perror("chatclient: doHandleExchange - recv() failed");
    exit(1);
  }

  // Truncate the remote user's handle to 10 chars
  strncpy(remoteHandle, buf, 10);
  remoteHandle[11] = 0;

  if(DEBUG) {
    printf("doHandleExchange() - Handle Received: %s\n");
  }

  return 0;
}

// Loop on the conversation until one side or the other supplies '\quit'
void doChat(int* sockfd, char* userHandle, char* remoteHandle) {

  // A buffer to store the user's keyboard input
  // Max message length is 500 chars.  Overallocating a little to prevent overflow.
  char keyboardInput[MAX_RESPONSE_LENGTH];
  memset(&keyboardInput, 0, MAX_RESPONSE_LENGTH);

  // A buffer to store the next remote message
  char msgBuffer[MAX_RESPONSE_LENGTH];
  memset(&msgBuffer, 0, MAX_RESPONSE_LENGTH);

  printf("Now chatting with %s.  Type \\quit to exit.\n", remoteHandle);

  while(1) {
     // Prompt the user for a message
     // Exit on '\quit'
     getMessageFromKeyboard(keyboardInput, userHandle);
     if (strcmp(keyboardInput, GOODBYE) == 0) {
        return;
     }
     if (sendMessage(sockfd, keyboardInput) == -1) {
       perror("\nchatclient: doChat - sendMessage() failed. Exiting.\n");
       return;
     }
     if (strcmp(keyboardInput, "\\quit\n") == 0) {
       return;
     }

     if (readMessage(sockfd, msgBuffer) == -1) {
       perror("\nchatclient: doChat - readMessage() failed. Exiting.\n");
       return;
     }
     if (strcmp(msgBuffer, "\\quit\n") == 0) {
       return;
     }
     printf("%s", msgBuffer);
  }
  return;
}

// getMessageFromKeyboard(char* handle)
// Prompts the user for a message

void getMessageFromKeyboard(char* message, char* userHandle) {

  // Allocate a much larger buffer than we need
  // It can theoretically still overflow, but this is trivial homework code...
  char temp[MAX_RESPONSE_LENGTH];
  memset(&temp, 0, MAX_RESPONSE_LENGTH);
  memset(message, 0, MAX_RESPONSE_LENGTH);
  char c = 0;
  int i = 0;

  if(DEBUG) {
    printf("getMessageFromKeyboard - Prompting for input\n");
  }


  printf("%s>",userHandle);
  fflush(stdout);
  // fgets technique for reading stdin from http://bit.ly/1GzJi38
  fgets(temp, sizeof(temp), stdin);
  while(strlen(temp) <= 1 || strlen(temp) > MAX_MESSAGE_LENGTH && temp != "") {

    // HACK
    // No matter what I do, I can't seem to get rid of the initial double-prompt.
    // I've tried a bunch of techniques for flushing stdin, with zero success
    // When I *can* eat the first character that causes the double prompt, then
    // subsequent prompts wait for a keypress before prompting... ugh.
    // I'm masking it in the UI, but you do need two character responses as a minimum.
    if (strlen(temp) != 1) {
      printf("%s>",userHandle);
    }
    // If fgets is null, just set our temp buffer to null and re-prompt
    if (fgets(temp, sizeof(temp), stdin) == NULL) {
      temp[0] = 0;
    };
    fflush(stdin);
  }
  // Truncate input to 500 chars
  strncpy(message, temp, sizeof(temp));
  message[MAX_MESSAGE_LENGTH+1] = 0; // Null terminate string

  if(DEBUG) {
    printf("getMessageFromKeyboard - Message Received: %s\n", message);
  }
  return;
}

// Reads message from remote server into buffer

int readMessage(int* sockfd, char* buf) {

  // Validate the response
  int numbytes = 0;
  char temp[MAX_RESPONSE_LENGTH] = "";

  // Initialize these to 0
  memset(&temp, 0, sizeof(temp));

  if(DEBUG) {
    printf("\nreadMessage: Reading message from network.\n");
  }

  if((numbytes = recv(*sockfd, temp, sizeof temp, 0)) == -1) {
    perror("chatclient: readMessage - recv() failed\n");
    return -1;
  }

  // The remote server closed the connection
  if (numbytes == 0) {
    if(DEBUG) {
      printf("readMessage() - Message was 0 bytes.  Remote connection closed.\n");
    }
    return 0;
  }

  strncpy(buf, temp, MAX_MESSAGE_LENGTH);
  buf[MAX_MESSAGE_LENGTH+1] = 0; // Ensure the string is null terminated
  if(DEBUG) {
    printf("readMessage - Message Received: %s\n",buf);
  }
  return 1;
}

int sendMessage(int* sockfd, char* buf) {
  int numbytes = 0;

  if(DEBUG) {
    printf("sendMessage() - Preparing to send message. %s\n", buf);
  }

  if ((numbytes = send(*sockfd, buf, strlen(buf), 0)) == -1) {
    perror("chatclient: sendMessage() failed");
    close(*sockfd);
    return(-1);
  }
  if(DEBUG) {
    printf("sendMessage() - Message successfully sent.\n%s\n",buf);
  }
  return 0;
}
