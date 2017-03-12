#ifndef FTSERVER_H_ /* Include Guard */
#define FTSERVER_H_

int fileExists(char *filename);
void getDirectoryListing(char* buf, int maxLen);
void handleCommands(int socketFd);

void listenForCommands(int socketFileDescriptor);
int openSocket(int portNum);
int parseCommandlineArgs(int argc, char* argv[]);
void sigchld_handler(int s);

#endif // CHATCLIENT_H_
