#ifndef CHATCLIENT_H_ /* Include Guard */
#define CHATCLIENT_H_

void doChat(int* sockfd, char* userHandle, char* remoteHandle);
void getMessageFromKeyboard(char* message, char* userHandle);
void getHandleFromKeyboard(char* handle);

int readMessage(int* sockfd, char* buf);
int sendMessage(int* sockfd, char* buf);
int doGoodbye(int* sockfd);
int doHandshake(int* sockfd);
int doHandleExchange(int* sockfd, char* userHandle, char* remoteHandle);

#endif // CHATCLIENT_H_
