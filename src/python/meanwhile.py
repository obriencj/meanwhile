

import _meanwhile
from _meanwhile import Channel
from _meanwhile import Service
from _meanwhile import ServiceIm


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
        if self._sock:
            self._sock.sendall(data)
            return 0
        else:
            return -1


    def onIoClose(self):
        if self._sock:
            self._sock.close()
            self._sock = None


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
        import socket
        import threading
        
        if self._sock:
            return

        # open the socket
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.connect(self._server)
        
        _meanwhile.Session.start(self, self._id)

        if background:
            # create a thread to continuously read from the server
            r = lambda s=self: s._recvLoop()
            self._thread = threading.Thread(target=r)
            self._thread.setDaemon(daemon)
            self._thread.start()
        else:
            self._recvLoop()


    def getThread(self):
        return self._thread

    def onStateChange(self):
        pass

    def onSetPrivacy(self):
        pass

    def onSetUserStatus(self):
        pass

    def onAdmin(self):
        pass

