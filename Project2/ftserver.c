/**
* ftserver.c
* Project 2
* cs372_400_w2017
* Jeromie Clark <clarkje@oregonstate.edu>
*
* Trivial file transfer server
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

#include <string.h>
#include <stdio.h>

#define MAX_PORT_LENGTH 5

int main ( int argc, char *argv[]) {

  char port[MAX_PORT_LENGTH];     // Port number supplied from commandline
  int portNum;

  // If the number of commandline arguments is wrong, print usage instructions
  if (argc != 2) {
    printf("Usage: ftserver.c <port>\n");
    return(1);
  }

  // Copy the commandline value to a named string
  strncpy(port, argv[1], MAX_PORT_LENGTH);

  // Convert the supplied value for port to an integer
  // If the conversion fails, throw an assert
  portNum = atoi(port);
  if(portNum == 0) {
    assert("The supplied value for port was not valid.  Please use a valid integer in [1024..65535] and try again.");
  }

  printf("Supplied value: %s, Integer value: %D", port, portNum);

}
