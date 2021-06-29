# udp-fwd
Single-threaded program which is responsible for forwarding incoming UDP datagrams. The program is configured via administrative TCP protocol. The program takes one positional argument: TCP port the program will listen on for incoming administrative connections. The main process creates a TCP socket bound to localhost address port passed as an argument. Therefore, it won’t accept connections from different hosts. At most 3 connections are allowed – if 4th client would like to establish connection the  server should reply with an informative message and drop the connection.
The server simultaneously handles one line string configuration commands received from all clients along with UDP forwarding according to configuration. By default it forwards nothing (so it has no UPD sockets at the very beginning). Available commands are:
* `fwd <portL> <IP1:port1> ... <IPn:portn>` - Opens a new UDP socket and starts forwarding datagrams incoming on UDP port to all endpoints given in IP:port list. Up to 10 forwarding rules are allowed. Example usage: `> fwd 1500 127.0.0.1:2000`
* `close <port>` - Stops forwarding on given port and closes corresponding UDP socket. Example usage: `close 1500`

How to run the program:
```
$ make updfwd
$ ./updfwd 1500
```
where 1500 is a port number which will be used to handle TCP connections
