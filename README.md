# dsock: An alternative to BSD socket API

## 1. Introduction

TODO

## 2. Terminology

**vertical composability**

Ability to stack protocols one on the top of another. For example,
Ethernet/IP/TCP/HTTP is a stack of vertically composed protocols.

**horizontal composability**

Ability to execute protocols in sequential manner. Horizontally composed
protocols are less often seen in the wild that vertically composed protocols.
For example, HTTP protocol can be switched into WebSocket protocol.
Another example may be STARTTLS which switches non-encrypted protocol into
encrypted version of the same protocol.

**protocol**

In dsock world protocol is an object that implements a network protocol.
The definition is deliberately vague. Given the huge diversity of network
protocols there's no single defining feature for all protocols.

**transport protocol**

Transport protocol allows to send and/or receive raw unstructured data.
It is either a bytestream protocol or message protocol (see below).
Examples of transport protocols are TCP, UDP, WebSocket, FTP and so on.

**application protocol**

Application protocols live on top of transport protocols. Rather than passing
raw data they are meant to perform a specific service for the user. Examples
of application protocols are DNS, SMTP, NNTP and so on.

**bytestream protocol**

Byte stream protocols are transport protocols that define no message boundaries.
One peer can send 10 bytes, then 8 bytes. The other peer can read 12 bytes, then
6 bytes. Bytestream protocols are always reliable (no bytes can be lost) and
ordered (bytes are received in the same order they were sent in). TCP is an
example of bytestream protocol.

**message protocol**

Message protocols are transport protocols that preserve message boundaries.
While message protocols are not necessarily reliable (messages can be lost)
and ordered (messages can be reordered) they are always atomic - one will
receive either complete message or no message. IP, UDP or WebSockets are
examples of message protocols. 

**adapter**

While typical network protocols exhibit encapsulation behaviour (user of TCP
protocol has no access to the features of the underlying IP protocol) there's
a specifc subset of lightweight protocols where encapsulation is not desirable.
These are protocols that are adding a single feature to the underlying protocol
rather than defining a fully new behaviour. In dsock world these protocols
are called adapters. The examples are: compression adapter, encryption
adapter, bandwidth throttling adapter and so on.

## 3. API

### 3.1 Introduction

Unlike BSD sockets, dsock API doesn't try to virtualize all possible protocols
and provide a single set of functions to deal with all of them. Instead, it
acknowledges how varied is the protocol landscape and how much the requirements
for indivudal protocols differ. Therefore, it lets each protocol define its
own API and ask only for bare minimum of standardised behaviour needed to
implement protocol composability. It also provides some non-binding suggestions
for protocol API designers. Following these suggestions leads to uniform feel
of the API and flat learning curve for protocol users.

### 3.2 Scheduling or rather lack of it

During the decades since BSD sockets were introduced the way they are used
have significantly changed. While in the beginning the user was supposed to fork
a new process for each connection and do all the work using simple blocking
calls nowadays the user is supposed to keep a pool of connections check them
via functions like poll or kqueue and if there's any work to be done dispatch
it to one of the worker threads in a pool. In short, user is supposed to do
both netowrk and CPU scheduling.

This change happened for performance reasons and haven't improved
functionality or usability of BSD sockets in any way. On the contrary,
by requiring every hobby programmer writing a CRUD application for a local
pet shop to do system programmer's work it contributed to proliferation of
buggy, hard-to-debug and barely maintainable code.

To address this problem, dsock assumes that there already exists an efficient
concurrency implementation (i.e. forking a new lightweight process should
take hundreds of nanoseconds and a context switch tens of nanoseconds).
In such environment network programming can be done in the old
one-process-per-connection way. There's no need for polling, thread pools
and such.

(Dsock even makes sure that users aspiring to use "modern"
schedule-by-hand-style of network programming, won't be able to do so.
For example it lacks an equivalent of poll or kqueue which is a prerequiite
for a hand written network scheduler.)

Technically, current implementation of dsock is based on
[libdill](http://libdill.org). However, same or similar API can be implemented
on top of any concurrency system with similar performance characteristics
(e.g. Golang's goroutines).

### 3.2 Handles

Protocol instances should be referred to by file descriptors, same way as they
are in BSD sockets. In kernel space implementations there's no problem with
that. However, POSIX provides no way to create custom file descriptors in user
space and therefore user space implementations are forced to use fake file
descriptors that won't play well with standard POSIX functions (close, fcntl
and such). To avoid confusion this document will call file descriptors used
by dsock, whether actual ones or fake ones, "handles".

### 3.3 Deadlines

TODO

### 3.4 Function naming

For consistency's sake the function names SHOULD be in lowercase and SHOULD be
composed of short protocol name (e.g. "tcp") and action name (e.g. "connect").
The two parts of the name SHOULD be separated by underscore ("tcp_connect").

### 3.5 Protocol initialization

A protocol SHOULD be initialized using a "start" function (e.g. "smtp_start").
If protocol runs on top of another protocol the handle of the underlying
protocol SHOULD be the first argument of the function. The function may have
arbitrary number of additional arguments.

The function SHOULD return handle of the newly created protocol instance.
In case of error it SHOULD return -1 and set errno to the appropriate error.

Some protocols require more complex setup. Consider TCP's listen/connect/accept
system. These protocols should use custom set of functions rather than try
to shoehorn all the functionality into a "start" function.

If protocol runs on top of an underlying protocol it takes of ownership of
that protocol's handle. Using the handle of low level protocol while it is
being owned by a high level protocol will result in undefined behaviour.

Example of creating a stack of four protocols:

```
int h1 = tcp_connect("192.168.0.111:5555");
int h2 = foo_start(h1, arg1, arg2, arg3);
int h3 = bar_start(h2);
int h4 = baz_start(h3, arg4, arg5);
```

### 3.6 Normal operation

TODO

### 3.7 Protocol shutdown

When handle is closed (close function in POSIX, hclose function in this
implementation) the protocol MUST shut down immediately without even trying
to do termination handshake or similar. Note that this is different from BSD
socket behaviour where closing a socket starts decent shutdown sequence.

The protocol should also clean up all resources it owns including
closing the handle of the underlying protocol. Given that the underlying
protocol does the same, an entire stack of protocols can be shut down
by closing the handle of the topmost protocol.

```
int h1 = foo_start();
int h2 = bar_start(h1);
int h3 = baz_start(h2);
hclose(h3); /* baz, bar and foo are shut down */
```

For clean shut down there should be a protocol-specific function. The function
SHOULD be called "stop". The handle to close SHOULD be the first argument of the
function. The function MAY have arbitrary number of other arguments.

If the shut down functionality is potentially blocking (e.g. requires a response
from the peer) the last argument SHOULD be a deadline. If deadline expires shut
down SHOULD fail with ETIMEDOUT error.

If shut down function succeeds it SHOULD NOT close the underlying protocol.
Instead it should return its handle. This is crucial for horizontal
composability of protocols.

```
h1 = foo_start();    /* create stack of two protocols */
h2 = bar_start(h1);
h1 = bar_stop(h2);   /* top protocol finishes but bottom one is still alive */
h3 = baz_start(h1);  /* new top protocol started *.
h1 = baz_stop(h3);   /* shut down both protocols */
foo_stop(h1);
```

However, note that some protocols are by their nature not capable of doing
this, for example, they may not have a termination sequence. In such cases
the shut down function SHOULD simply close the underlying protocol and return 0.

In case of error shut down function SHOULD close the underying protocol,
return -1 and set errno to appropriate value.

