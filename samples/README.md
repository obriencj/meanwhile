
# Meanwhile Development Extras

These samples demonstrate how to do a few things with the Meanwhile
library. Some of these are also tools which can be used to help in
debugging and obtaining protocol information.


## Library Examples

The following are quick examples to demonstrate the usage of the
meanwhile library.

### socket.c

Compile with `./build socket`. This is the simplest possible
client. All it does is connect and authenticate to a host (specified
on the command line). Most of the code is dedicated to performing I/O.


### sendmessage.c

Compile with `./build sendmessage`. Expands upon socket.c to send a
message via the IM service after login, then immediately logs off and
exits.


## Utilities

The following are utilities that can assist in the development and
debugging of either meanwhile itself, or meanwhile-based clients, or
in inspecting official Sametime client streams.

### redirect_server.c

Compile with `./build redirect_server`. Acts as a redirecting sametime
server; any client attempting to connect to the socket this utility
listens on will be instructed to redirect its connection to an
alternative host (which is specified on the command line). Useful for
ensuring client code can handle redirects correctly when there's no
real redirecting server to test against.


### nocipher_proxy.c

Compile with `./build nocipher_proxy`. Acts as a sametime server
proxy, passing messages between a real client and server. However, it
will intercept and mangle channel creation messages to ensure that
they will not be used with encryption. This will cause many clients to
fail in strange places (where they demand encryption), but is useful
for getting some messages from a service in the clear. Will print all
messages in hex pairs to stdout using the hexdump utility. This may be
more useful than using ethereal, as it will actually group its output
by message rather than by receipt from the TCP stream.


### login_server.c

Compile with `./build login_server`. Acts as a sametime server; any
client attempting to connect to the socket this utility listens on
will be able to complete handshaking and send a login message. The
tool then analyzes the authentication method and data and prints the
decrypted data to stdout. This was useful for reverse-engineering the
RC2/128 auth method (and determining what one of the guint32 fields of
the handshake ack was for). Probably not very useful for anything
else.


### logging_proxy.c

Compile with `./build logging_proxy`. Acts as a sametime server proxy,
passing messages between a real client and server. However, it will
intercept and mangle channel data in order to obtain the unencrypted
data. This should be invisible to both the client and the server. Will
print all messages in hex pairs to stdout using the hexdump utility,
and will print decrypted contents of encrypted channel messages
separately. This is certainly more useful than using ethereal, as it
groups its output by message as well as provides an unencrypted view
of otherwise obscured service protocols.
