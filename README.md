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
(e.h. Golang's goroutines).

### 3.2 Handles

Protocol instances should be referred to by file descriptors, same way as they
are in BSD sockets. In kernel space implementations there's no problem with
that. However, POSIX provides no way to create custom file descriptors in user
space and therefore user space implementations are forced to use fake file
descriptors that won't play well with standard POSIX functions (close, fcntl
and such). To avoid confusion this document will call file descriptors used
by dsock, whether actual ones or fake ones, "handles".


