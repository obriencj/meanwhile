
"""
Wrappers for the Meanwhile library


  Meanwhile - Unofficial Lotus Sametime Community Client Library
  Copyright (C) 2004  Christopher (siege) O'Brien
  
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.
  
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.
  
  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the Free
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
"""

# todo: add more documentation for the base classes and their
# call-back methods

# todo: the meanwhile library isn't actually thread-safe. Need to put
# some sort of synchronization wrapper around the feed thread, or
# remove it. Maybe I can do that in the base type C implementations.


# the useful classes
from _meanwhile import Channel
from _meanwhile import Service
from _meanwhile import ServiceAware
from _meanwhile import ServiceConference
from _meanwhile import ServiceIm
from _meanwhile import ServiceResolve
from _meanwhile import ServiceStorage
from _meanwhile import Session


# service state values
from _meanwhile import \
     SERVICE_STARTING, SERVICE_STARTED, SERVICE_STOPPING, \
     SERVICE_STOPPED, SERVICE_ERROR, SERVICE_UNKNOWN


# session state values
from _meanwhile import \
     SESSION_STARTING, SESSION_HANDSHAKE, SESSION_HANDSHAKE_ACK, \
     SESSION_LOGIN, SESSION_LOGIN_REDIR, SESSION_LOGIN_ACK, \
     SESSION_STARTED, SESSION_STOPPING, SESSION_STOPPED, SESSION_UNKNOWN


# awareness id states
from _meanwhile import \
     STATUS_ACTIVE, STATUS_IDLE, STATUS_AWAY, STATUS_BUSY


# awareness id types
from _meanwhile import \
     AWARE_SERVER, AWARE_USER, AWARE_GROUP


# IM client types/features
from _meanwhile import \
     IM_CLIENT_PLAIN, IM_CLIENT_NOTESBUDDY


# IM conversation message types/features
from _meanwhile import \
     IM_PLAIN, IM_TYPING, IM_HTML, IM_SUBJECT, IM_MIME


# IM conversation states
from _meanwhile import \
     CONVERSATION_CLOSED, CONVERSATION_PENDING, CONVERSATION_OPEN, \
     CONVERSATION_UNKNOWN


# conference states
from _meanwhile import \
     CONFERENCE_NEW, CONFERENCE_PENDING, CONFERENCE_INVITED, \
     CONFERENCE_OPEN, CONFERENCE_CLOSING, CONFERENCE_ERROR, \
     CONFERENCE_UNKNOWN


# resolve options
from _meanwhile import \
     RESOLVE_UNIQUE, RESOLVE_FIRST, RESOLVE_ALL_DIRS, RESOLVE_USERS


# resolve result codes
from _meanwhile import \
     RESOLVED_SUCCESS, RESOLVED_PARTIAL, RESOLVED_MULTIPLE, \
     RESOLVED_BAD_FORMAT


class SocketSession(Session):

    ''' Implementation of a meanwhile session with built-in socket
    handling functionality.  '''
    
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
        
        ''' handles a login redirect message by closing the existing
        socket and attempting to connect to the new server as
        directed.  '''
        
        import socket

        # close off the existing session
        self.stop()

        # setup a socket elsewhere
        self._server = (host, self._server[1])
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.connect(self._server)
        
        # resend the beginning data
        Session.start(self, self._id)


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
        
        ''' open the connection to the indicated server and port and
        start the process of logging in. Optionally set background to
        True to cause the processing to take place in the background
        after a socket is successfully opened. Additionally set daemon
        to True to indicate the background thread should be daemonized.
        '''
        
        import socket
        import threading
        
        if self._sock or self._thread:
            raise Exception, "SocketSession already started and running"

        # open the socket
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.connect(self._server)
        
        # send the beginning data
        Session.start(self, self._id)

        if background:
            # create a thread to continuously read from the server
            r = lambda s=self: s._recvLoop()
            self._thread = threading.Thread(target=r)
            self._thread.setDaemon(daemon)
            self._thread.start()
        else:
            # continuously read from server in the current thread
            self._thread = threading.currentThread()
            self._recvLoop()


    def getThread(self):

        ''' the thread that the read loop is being executed in '''
        
        return self._thread



# The End.
