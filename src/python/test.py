
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


class Session(meanwhile.Session):
    def onAdmin(self, text):
        print "ADMIN: %s" % text



class ServiceIm(meanwhile.ServiceIm):
    def onText(self, who, text):
        print '%s said "%s"' % (who[0], text)
        if text == 'shutdown':
            sess = self.session
            sess.stop()
        else:
            t = 'you said "%s"' % text
            self.sendText(who, t)


if __name__ == "__main__":
    s = Session(WHERE, WHO)
    im = ServiceIm(s)
    
    s.addService(im)
    s.start(background=False)
