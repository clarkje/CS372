#ifndef FTSERVER_H_ /* Include Guard */
#define FTSERVER_H_

int establishDataConnection(int socketFd);
int fileExists(char *filename);
void getDirectoryListing(char* buf, int maxLen);
void handleCommands(int socketFd);

void listenForCommands(int socketFileDescriptor);
int openSocket(int portNum);
int parseCommandlineArgs(int argc, char* argv[]);
int sendFile(int socketFd, int dataFd, char* filename);
void sigchld_handler(int s);

#endif // CHATCLIENT_H_
