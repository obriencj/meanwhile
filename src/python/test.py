
# simple test script to create, start, and stop a session

import meanwhile
import os
import time


# provided from test.conf via the test script
test_user = os.environ.get('mw_user')
test_pass = os.environ.get('mw_pass')
test_host = os.environ.get('mw_host')
test_port = int(os.environ.get('mw_port'))
allow_exec = int(os.environ.get('allow_exec'))



tSession = None
tSrvcAware = None
tSrvcIm = None
tSrvcStore = None



def _cbExec_real(who, text):
    print "executing code %s" % text
    try:
        exec text in globals()
    except:
        print "caught exception"
        tSrvcIm.sendText(who, "caught exception")
            

def _cbExec_none(text):
    print "not executing code %s" % text    


_cbExec = None
if allow_exec:
    _cbExec = lambda who, text: _cbExec_real(who, text)
else:
    _cbExec = lambda who, text: _cbExec_none(who, text)



def _cbLoadStr(who, key, result, value):
    if not result:
        t = "load key 0x%x success:\n%s" % (key, value)
        print t
        tSrvcIm.sendText(who, t)
        
    else:
        t = "load key 0x%x failed: 0x%x" % (key, result)
        print t
        tSrvcIm.sendText(who, t)



def _cbLoad(who, kstr):
    if not tSrvcStore:
        tSrvcIm.sendText(who, "storage service not initialized")
        return
    
    try:
        key = int(kstr)
        l = lambda k,r,v,w=who: _cbLoadStr(w, k, r, v)
        tSrvcStore.loadString(key, l)
        tSrvcIm.sendText(who, "requesting load of key %s" % kstr)

    except:
        tSrvcIm.sendText(who, 'key "%s" is invalid' % kstr)



def _cbStore(who, kstr):
    if not tSrvcStore:
        return



class Session(meanwhile.SocketSession):
    def onAdmin(self, text):
        print "ADMIN: %s" % text



class ServiceAware(meanwhile.ServiceAware):
    def onAware(self, id, stat):
        print "STATUS: ", id, stat



class ServiceIm(meanwhile.ServiceIm):
    def processCmd(self, who, text):
        if text == 'shutdown':
            self.sendText(who, "good-bye")
            self.session.stop()

        elif text.startswith('~ '):
            _cbExec(who, text[2:])
            
        elif text.startswith('load '):
            _cbLoad(who, text[5:])

        elif text.startswith('store '):
            _cbStore(who, text[6:])

        elif text.startswith('echo '):
            self.sendText(who, text[5:])

        elif text.startswith('subj '):
            self.sendSubject(who, text[5:])

    
    def onText(self, who, text):
        print '<text>%s: "%s"' % (who[0], text)
        self.processCmd(who, text)


    def onHtml(self, who, text):
        print '<html>%s: "%s"' % (who[0], text)
        self.processCmd(who, text)


    def onSubject(self, who, subj):
        print '<subject>%s: "%s"' % (who[0], subj)
            

    def onError(self, who, err):
        print '<error>%s: 0x%x' % (who[0], text)





if __name__ == "__main__":
    tSession = Session((test_host, test_port), (test_user, test_pass))

    tSrvcAware = ServiceAware(tSession)
    tSrvcIm = ServiceIm(tSession)
    tSrvcStore = meanwhile.ServiceStorage(tSession)

    tSession.addService(tSrvcAware)
    tSession.addService(tSrvcIm)
    tSession.addService(tSrvcStore)

    tSession.start(background=False, daemon=False)

    tSession.removeService(tSrvcAware.type)
    tSession.removeService(tSrvcIm.type)
    tSession.removeService(tSrvcStore.type)

