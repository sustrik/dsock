**Dillsocks: Protocol library for libdill**

Principles:

1. The goal is to allow for true micro-protocol that can be composed like
   lego blocks to form full-fledged protocols.
2. Currently we have big protocols (hundreds of pages, e.g. 3GPP) and small
   protocols (tens of pages, e.g. IETF). We want microprotocols (specification
   one paragraph long).
3. To achieve that we need vertical composability (e.g. TCP on top of IP;
   HTTP on top of TCP)...
4. And horizontal composability (e.g. HTTP switches to WebSockets; STARTTLS)
5. Protocol initiation and termination is not abstracted; must be done in
   protocol-specific way.
6. Sending data and receiving data is abstracted. Two abstractions are used:
   bytestream and messages.
7. Sending and receiving must not presuppose each other.
8. Sending and receiving with metadata (as with sndmsg, recvmsg) is not
   abstracted and must be done in protocol specific way.
9. The system must be able to support purely "structural" protocols, for example
   one that joins two uni-directional protocols into a single bi-directional
   one.

Some protocols have to be supplied out of the box, to give people something
to play with in the beginning. Options include:

1. TCP bytestream
2. UNIX domain bytestream
3. UDP
4. Simple framing (size + data)
5. Line protocol (CRLF-terminated lines of text)
6. HTTP
7. WebSockets
8. SMTP
9. Version and/or capability negotiation protocol
10. SSL
11. TCPMUX
12. WebSocketMUX
13. PGM
14. DCCP

