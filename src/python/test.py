##############################################################################
# simple python bot test script
##############################################################################



import meanwhile
import os
import time



# global session and service instances
tSession = None
tSrvcAware = None
tSrvcConf = None
tSrvcIm = None
tSrvcStore = None
    


def _cbExec_real(who, text):

    ''' actually executes text as code '''

    print "executing code %s" % text
    try:
        exec text in globals()
    except Exception, err:
        msg = "caught exception: %s" % err
        print msg
        tSrvcIm.sendText(who, msg)

            

def _cbExec_none(text):
    print "not executing code %s" % text    



# determine which _cbExec to use based on the allow_exec environment
# variable
_cbExec = None
if os.environ.get('allow_exec'):
    _cbExec = _cbExec_real
else:
    _cbExec = _cbExec_none



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
    def __init__(self, session):
        meanwhile.ServiceIm.__init__(self, session)
        self._send_queue = {}

    
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

        elif text.startswith('text '):
            self.sendText(who, text[5:])

        elif text.startswith('html '):
            self.sendHtml(who, text[5:])

        elif text.startswith('subj '):
            self.sendSubject(who, text[5:])

        elif text == 'help':
            help = '''Available test-bot Commands:
~ <code>\n\texecutes code in the python interpreter
load <n>\n\tloads string value from key n in the storage service
store <n> <str>\n\tstores string str into key n in the storage service
text <str>\n\techos plain-text str back to you
html <str>\n\techos html formatted str back to you
subj <str>\n\tsets the conversation subject to str
help\n\tprints this information'''
            self.sendText(who, help)


    def _queue(self, who, cb, data):
        
        ''' queues up a send call to be handled when the conversation
        is fully opened. '''
        
        q = self._send_queue
        t = (cb, data)
        if not q.has_key(who):
            q[who] = [t]
        else:
            q[who].append(t)


    def _delqueue(self, who):
        
        ''' deletes all queued send calls '''
        
        q = self._send_queue
        if q.has_key(who):
            del q[who]


    def _runqueue(self, who):
        
        ''' sends all queued calls '''
        
        q = self._send_queue
        if q.has_key(who):
            for act in q[who]:
                act[0](self,who,act[1])
            del q[who]


    def onOpened(self, who):
        print '<opened>%s' % who[0]
        self._runqueue(who)
        

    def onClosed(self, who, err):
        print '<closed>%s: 0x%x' % (who[0], err)
        self._delqueue(who)


    def sendText(self, who, text):
        state = self.conversationState(who)
        if state == meanwhile.CONVERSATION_OPEN:
            meanwhile.ServiceIm.sendText(self, who, text)
        else:
            self._queue(who, meanwhile.ServiceIm.sendText, text)
            if state == meanwhile.CONVERSATION_CLOSED:
                self.openConversation(who)


    def sendHtml(self, who, html):
        state = self.conversationState(who)
        if state == meanwhile.CONVERSATION_OPEN:
            meanwhile.ServiceIm.sendHtml(self, who, html)
        else:
            self._queue(who, meanwhile.ServiceIm.sendHtml, html)
            if state == meanwhile.CONVERSATION_CLOSED:
                self.openConversation(who)


    def sendSubject(self, who, subj):
        state = self.conversationState(who)
        if state == meanwhile.CONVERSATION_OPEN:
            meanwhile.ServiceIm.sendSubject(self, who, subj)
        else:
            self._queue(who, meanwhile.ServiceIm.sendSubject, subj)
            if state == meanwhile.CONVERSATION_CLOSED:
                self.openCconversation(who)


    def sendTyping(self, who, typing=True):
        state = self.conversationState(who)
        if state == meanwhile.CONVERSATION_OPEN:
            meanwhile.ServiceIm.sendTyping(self, who, typing)
        else:
            self._queue(who, meanwhile.ServiceIm.sendTyping, typing)
            if state == meanwhile.CONVERSATION_CLOSED:
                self.openConversation(who)
                
    
    def onText(self, who, text):
        print '<text>%s: "%s"' % (who[0], text)
        try:
            self.processCmd(who, text)
        except Exception, e:
            self.sendText(who, e)


    def onHtml(self, who, text):
        print '<html>%s: "%s"' % (who[0], text)
        try:
            self.processCmd(who, text)
        except Exception, e:
            self.sendText(who, e)


    def onMime(self, who, data):
        
        ''' Handles incoming MIME messages, such as those sent by
        NotesBuddy containing images. '''

        from email.Parser import Parser
        from StringIO import StringIO
        
        print '<mime>%s' % who[0]
        msg = Parser().parsestr(data)

        html = StringIO()  # combined text segments
        images = {}        # map of Content-ID:binary image
                
        for part in msg.walk():
            mt = part.get_content_maintype()
            if mt == 'text':
                html.write(part.get_payload(decode=True))
            elif mt == 'image':
                cid = part.get('Content-ID')
                images[cid] = part.get_payload(decode=True)

        print ' <text/html>:', html.getvalue()
        html.close()

        print ' <images>:', [k[1:-1] for k in images.keys()]
                

    def onSubject(self, who, subj):
        print '<subject>%s: "%s"' % (who[0], subj)


    def onTyping(self, who, typing):
        str = ("stopped typing", "typing")
        print '<typing>%s: %s' % (who[0], str[typing])



class ServiceConference(meanwhile.ServiceConference):
    def onText(self, conf, who, text):
        print '<text>%s, %s: %s' % (conf, who[0], text)


    def onPeerJoin(self, conf, who):
        print '<peer joined>%s, %s' % (conf, who[0])


    def onPeerPart(self, conf, who):
        print '<peer parted>%s, %s' % (conf, who[0])


    def onTyping(self, conf, who, typing):
        str = ("stopped typing", "typing")
        print '<typing>%s, %s: %s' % (conf, who[0], str[typing])


    def onInvited(self, conf, inviter, text):
        print '<invited>%s, %s: %s' % (conf, inviter[0], text)


    def onOpened(self, conf):
        print '<joined>%s' % conf



def main():

    ''' Run the test bot '''

    test_user = os.environ.get('mw_user')
    test_pass = os.environ.get('mw_pass')
    test_host = os.environ.get('mw_host')
    test_port = int(os.environ.get('mw_port'))
    
    print "Creating session"
    tSession = Session((test_host, test_port), (test_user, test_pass))

    print "Creating services"
    tSrvcAware = ServiceAware(tSession)
    tSrvcConf = ServiceConference(tSession)
    tSrvcIm = ServiceIm(tSession)
    tSrvcStore = meanwhile.ServiceStorage(tSession)

    tSession.addService(tSrvcAware)
    tSession.addService(tSrvcConf)
    tSession.addService(tSrvcIm)
    tSession.addService(tSrvcStore)

    print "Starting session"
    try:
        tSession.start(background=False, daemon=False)
    except Exception, e:
        print "Error: %s" % e

    print "Shutting down"
    tSession.removeService(tSrvcAware.type)
    tSession.removeService(tSrvcConf.type)
    tSession.removeService(tSrvcIm.type)
    tSession.removeService(tSrvcStore.type)

    print "Done"
    


if __name__ == "__main__":
    main()



# The End.
