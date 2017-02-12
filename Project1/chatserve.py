#!/usr/bin/python
# chatserve.py
# CS372_400_W2017
# Jeromie Clark <clarkje@oregonstate.edu>
#
# chatserve waits on a specified port for a client request
# chatclient sends an initial handshake message to establish a connection
#
# modeled on asynchronous socket server examples at:
# https://docs.python.org/release/2.6.5/library/socketserver.html
# https://pymotw.com/2/SocketServer/

import sys
import os
import SocketServer
import socket
import threading
import time

# Handles chat messages
class ChatMessageHandler(SocketServer.BaseRequestHandler):

    def handle(self):

        # On connect, the client will send the following handshake
        # HELLO
        # Username

        # Just wait until the client says HELLO
        data = self.request.recv(1024)
        while("HELLO" not in data):
            data = self.request.recv(1024)

        # Respond with HELLO
        self.request.send("HELLO")

        # The next message will be the username
        username = self.request.recv(1024)

        # Respond with our username
        self.request.send(HANDLE)

        # Echo data back to the client
        data = None
        data = self.request.recv(1024)
        while("\quit" not in data):
            print("{0}: {1}".format(username, data))
            # Strip any pesky newlines that come through
            response = "{0}: {1}".format(HANDLE, data.upper())
            print(response)
            self.request.send(response)
            data = self.request.recv(1024)
        return

# Provides the chat server
# Multithreaded, forking server based on Python docs at:
# https://docs.python.org/2/library/socketserver.html#SocketServer.ForkingMixIn
class ChatServe(SocketServer.ForkingMixIn, SocketServer.ForkingTCPServer):
    pass

if __name__ == '__main__':

    if (len(sys.argv) != 2):
        print "usage: chatserve.py <port>"

    HOST = socket.gethostname()
    HANDLE = "ChatServer"

    # I liked this approach to unpacking argv, from:
    # https://learnpythonthehardway.org/book/ex13.html
    SCRIPT, PORT = sys.argv

    # Create the socket server.  Bind to localhost and the port supplied at launch
    address = (HOST, int(PORT))
    server = ChatServe(address, ChatMessageHandler)

    # Start a thread with the server -- that thread will then start one
    # more thread for each request
    server_thread = threading.Thread(target=server.serve_forever)

    # Exit the server thread when the main thread terminates
    server_thread.setDaemon(True)
    server_thread.start()

    print "Server loop running in thread:", server_thread.getName()
    print "== Press Ctrl-C To Exit Server =="

    while True:
        pass

    server.shutdown()
    server.server_close()
