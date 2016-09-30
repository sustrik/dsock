# dsock: An alternative to BSD socket API

## 1. Introduction

## 2. Concepts

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

