
# simple test script to create, start, and stop a session

import meanwhile
import os
import time


# provided from test.conf via the test script
test_user = os.environ.get('mw_user')
test_pass = os.environ.get('mw_pass')
test_host = os.environ.get('mw_host')
test_port = os.environ.get('mw_port')

WHO = (test_user, test_pass)
WHERE = (test_host, int(test_port))


tSession = None
tSrvcIm = None
tSrvcStore = None


def _cbShutdown():
    tSession.stop()


def _cbExec(text):
    print "executing code %s" % text
    exec text in globals()    


def _cbLoadStr(who, key, result, value):
    if not result:
        t = "load key 0x%x success:\n%s" % (key, value)
        print t
        tSrvcIm.sendText(who, t)
        
    else:
        t = "load key 0x%x failed: 0x%x" % (key, result)
        print t
        tSrvcIm.sendText(who, t)

    time.sleep(10)
    print "stopping session"
    tSession.stop()


def _cbLoad(who, kstr):
    try:
        key = int(kstr)
        l = lambda k,r,v,w=who: _cbLoadStr(w, k, r, v)
        tSrvcStore.loadString(key, l)
        tSrvcIm.sendText(who, "requesting load of key %s" % kstr)

    except:
        tSrvcIm.sendText(who, 'key "%s" is invalid' % kstr)



class Session(meanwhile.Session):
    def onAdmin(self, text):
        print "ADMIN: %s" % text



class ServiceIm(meanwhile.ServiceIm):
    def onText(self, who, text):
        print '%s: "%s"' % (who[0], text)

        if text == 'shutdown':
            _cbShutdown()

        elif text.startswith('~ '):
            _cbExec(text[2:])
            
        elif text.startswith('load '):
            _cbLoad(who, text[5:])
            
        else:
            self.sendText(who, 'you said "%s"' % text)


if __name__ == "__main__":
    tSession = Session(WHERE, WHO)
    tSrvcIm = ServiceIm(tSession)
    tSrvcStore = meanwhile.ServiceStorage(tSession)
    
    tSession.addService(tSrvcIm)
    tSession.addService(tSrvcStore)

    tSession.start(daemon=True)
