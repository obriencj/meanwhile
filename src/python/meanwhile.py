
'''
Wrappers for the Meanwhile library
'''

# todo: add more documentation for the base classes and their
# call-back methods

# todo: the meanwhile library isn't actually thread-safe. Need to put
# some sort of synchronization wrapper around the feed thread, or
# remove it. Maybe I can do that in the base type C implementations.


import _meanwhile
from _meanwhile import Channel
from _meanwhile import Service
from _meanwhile import ServiceIm
from _meanwhile import ServiceStorage

from _meanwhile import \
     SERVICE_STARTING, SERVICE_STARTED, SERVICE_STOPPING, SERVICE_STOPPED, \
     SERVICE_ERROR, SERVICE_UNKNOWN

from _meanwhile import \
     SESSION_STARTING, SESSION_HANDSHAKE, SESSION_HANDSHAKE_ACK, \
     SESSION_LOGIN, SESSION_LOGIN_REDIR, SESSION_LOGIN_ACK, \
     SESSION_STARTED, SESSION_STOPPING, SESSION_STOPPED, SESSION_UNKNOWN


class Session(_meanwhile.Session):
    '''
    '''
    
    def __init__(self, where, who):
        '''
        '''
        
        self._server = where
        self._id = who
        self._sock = None
        self._thread = None

    
    def onIoWrite(self, data):
        if not self._sock:
            return -1

        try:
            self._sock.sendall(data)
        except:
            return 1
        else:
            return 0


    def onIoClose(self):
        if self._sock:
            self._sock.close()
            self._sock = None


    def onLoginRedirect(self, host):
        import socket

        # close off the existing session
        self.stop()

        # setup a socket elsewhere
        self._server[0] = host
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.connect(self._server)
        
        # resend the beginning data
        _meanwhile.Session.start(self, self._id)


    def _recvLoop(self):
        while self._sock:
            data = self._sock.recv(1024)
            if data:
                self.recv(data)
            else:
                self.stop(-1)
                break
        self._thread = None


    def start(self, background=False, daemon=False):
        '''
        '''
        
        import socket
        import threading
        
        if self._sock:
            return

        # open the socket
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.connect(self._server)
        
        # send the beginning data
        _meanwhile.Session.start(self, self._id)

        if background:
            # create a thread to continuously read from the server
            r = lambda s=self: s._recvLoop()
            self._thread = threading.Thread(target=r)
            self._thread.setDaemon(daemon)
            self._thread.start()
        else:
            self._thread = threading.currentThread()
            self._recvLoop()


    def getThread(self):
        return self._thread

