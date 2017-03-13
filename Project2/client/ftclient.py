#!/usr/bin/python
# ftclient.py
# CS372_400_W2017
# Jeromie Clark <clarkje@oregonstate.edu>
#
# ftclient starts on host B and validates any pertinent commandline parameters
# <SERVER_HOST>, <SERVER_PORT>, <COMMAND>, <FILENAME>, <DATA_PORT>, etc.
# ftserver and ftclient establish a TCP control connection on <SERVER_PORT>
# ftclient sends -l or -g <FILENAME> on Control Port
#

import os
import socket
import struct
import sys
import threading
import time

MSGLEN = 65535          # Maximum message length

class FTClient:

    mCmdSock = None
    mDataSock = None
    mDataConnection = None

    def promptForOverwrite(self):
        # http://sweetme.at/2014/01/22/how-to-get-user-input-from-the-command-line-in-a-python-script/
        response = raw_input("File exists.  Would you like to Overwrite? (Y/N)");
        if (response == "Y" or response == "y"):
            return 1
        else:
            return 0
        return 0

    # Requests a file from the remote server and writes it to disk
    def getFile(self, filename):

        print("in getFile()")

        # Check to see if the specified filename exists
        # http://stackoverflow.com/questions/82831/how-do-i-check-whether-a-file-exists-using-python
        if (os.path.isfile(filename)):
            print("isFile")

            # file exists, negotiate overwrite
            if (self.promptForOverwrite() == 0):
                print("Operation Cancelled.  Exiting")
                sys.exit(0)

        # ask the server to send us the file
        self.mCmdSock.sendall("-g {0}".format(filename))
        time.sleep(1)

        response = self.mCmdSock.recv(32)
        print("RESPONSE: {0}".format(response));

        if ("OK" in response):
            # we're good to write/overwrite this file
            with open(filename, 'w') as f:
                print("Transferring File, Please Wait.")
                f.write(self.receiveDataTimeout())
                print("File received.  Exiting")

        elif ("ERROR_FILE_NOT_FOUND" in response):
            print("The file could not be found on the server.  Exiting.")
        else:
            print("An error occurred. Exiting.")

        return

    # Requests a directory listing from the remote server, then displays it
    def getDirectoryListing(self):
        self.mCmdSock.sendall("-l")
        data = self.receiveDataTimeout(1)
        print("\n{0}".format(data))
        return

    # Use a timeout to keep track of the end of the transmission.  Good enough for this.
    # http://code.activestate.com/recipes/408859-socketrecv-three-ways-to-turn-it-into-recvall/
    def receiveDataTimeout(self, timeout=2):

        self.mDataConnection, serverAddress = self.mDataSock.accept()
        self.mDataConnection.setblocking(0)
        total_data=[]; data=''; begin=time.time()
        while 1:
            # if you get some data, then wait for a second and break
            if total_data and time.time()-begin>timeout:
                break
            # if we got no data at all, wait a little longer
            elif time.time()-begin>timeout * 2:
                break
            try:
                data = self.mDataConnection.recv(8192)
                if data:
                    total_data.append(data)
                    begin=time.time()
                else:
                    time.sleep(1)
            except:
                continue

        return ''.join(total_data)


    # Starts a session with the specified server
    # References tutorial code at: https://pymotw.com/2/socket/tcp.html
    def startSession(self):

        self.mCmdSock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client_address = (SERVER_HOST, int(SERVER_PORT))
        print("Conneting to {0} port {1}".format(SERVER_HOST, SERVER_PORT))
        self.mCmdSock.connect(client_address)

        # The server should begin the handshake, which starts with HELLO\0
        data_received = 0
        data_expected = len("HELLO")

        while data_received < data_expected:
            data = self.mCmdSock.recv(16)
            data_received += len(data)

        if (data == "HELLO"):
            print("Server Handshake Received: {0}", data)
            data = None
        # Open a listener on DATA_PORT
        self.mDataSock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        server_address = (socket.gethostname(), int(DATA_PORT))
        #server_address = ('', int(DATA_PORT))
        self.mDataSock.bind(server_address)
        self.mDataSock.listen(5)

        # Send DATA_PORT <DATA_PORT> to the server
        print("Sending DATA_PORT {0}".format(DATA_PORT))
        self.mCmdSock.sendall("DATA_PORT {0}".format(DATA_PORT))

        # Wait for server to connections
        print("Waiting for server connection on data port")
        self.mDataConnection, serverAddress = self.mDataSock.accept()

        try:
            # The server should begin the handshake, which starts with HELLO\0
            print("Remote Server Connected on Data Port")

        finally:
            return

    # Shows the Usage Instructions

    def showUsage(self):
        print "usage:\nftclient.py <SERVER_HOST> <SERVER_PORT> -l <DATA_PORT>"
        print "ftclient.py <SERVER_HOST> <SERVER_PORT> -g <FILENAME> <DATA_PORT>"
        return

if __name__ == '__main__':

    # Start the client-side
    client = FTClient()

    # Validate Commandline Params
    if (len(sys.argv) == 5):
        if (sys.argv[3] != "-l"):
            client.showUsage()
            sys.exit(0)
        else:
            # Unpacking argv appraoch from https://learnpythonthehardway.org/book/ex13.html
            PROGRAM, SERVER_HOST, SERVER_PORT, COMMAND, DATA_PORT = sys.argv
    else:
        if (len(sys.argv) != 6):
            client.showUsage()
            sys.exit(0)
        else:
            PROGRAM, SERVER_HOST, SERVER_PORT, COMMAND, FILENAME, DATA_PORT = sys.argv

    client.startSession()

    if (COMMAND == "-l"):
        client.getDirectoryListing()

    if (COMMAND == "-g"):
        client.getFile(FILENAME)

    # clean up
    client.mDataConnection.shutdown(socket.SHUT_RDWR)
    client.mDataConnection.close()
    client.mDataSock.shutdown(socket.SHUT_RDWR)
    client.mDataSock.close()
    # tell the server we're done
    client.mCmdSock.sendall("EXIT")
    # client.mCmdSock.shutdown(socket.SHUT_RDWR)
    client.mCmdSock.close()
