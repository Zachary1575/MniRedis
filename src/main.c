#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h> // This allows us to use the addr structs defined by Apple.
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#include <pthread.h> // POSIX Threads (pthreads)
/*
    TODO:
    >> Verify TCP works.
    >> Implement a "Single-threaded event loop" architecture.
    >> Implement RESP protocol and Parser.

*/
/*
    Something interesting about C printf. What happens is that when you do printf, it sits in
    the stdout buffer file. You need to manually flush for it to appear in the tty.
*/
// Steps to an TCP socket according to GPT
/*
    1.) Create a socket using the BSD Socket API. This will return a file descriptor which
    is like a id to identify the network socket in kernel space. A file descriptor (FD) is a unique, 
    non-negative integer used by an operating system (specifically in Unix-like systems)
    to identify an open file or other input/output (I/O) resource, such as a network socket,
    a pipe, or a device. (socket())

    2.) Set socket options

    3.) Bind the socket to a port and address (bind())

    4.) Transition the socket to a listening state (listen())

    5.) Accept a new client connection (accept()). This does the TCP handshake protocol.

    6.) Recieve data from client. (recv())

    7.) Send data to client. (send())

    8.) Close the connection (close())
*/
// Boolean Enum
enum {
    TRUE = 1,
    FALSE = 0,
    true = 1,
    false = 0
};

int main() {
    printf("Welcome to my TCP server!\n");
    printf("(1) Here we first create a socket as per the BSD socket API.\n");

    /*
    int domain - (ENUM usually) Specifies a communication domain. See 'man 2 socket' for details.
    int type - (ENUM usually) Specifies the semantics of the communication. See 'man 2 socket' for details.
    int protocol - (ENUM usually) Gets the protocol. I think it needs to first fine the protocol using a getprotoent type func which reads the '/etc/protocols' file.
    */
    int tcpProtocolNumber = getprotobyname("TCP")->p_proto;
    int fd = socket(PF_INET, SOCK_STREAM, tcpProtocolNumber);
    printf("Socket file descriptor: %d.\n", fd);
    printf("Socket finished and initiated!\n\n");


    printf("(2) We then have to bind the socket to an avaliable port and address.\n");
    /*
    int socket - Basically your file descriptor. Created by the OS for you as a IO file.
    const struct sockaddr *address - TBD 
    socklen_t address_len - TBD In MacOS, socklen_t is typucally 32-bit (4 bytes) which is equivalent of an unsigned int.

    // See "https://developer.apple.com/documentation/kernel/sockaddr_in" for different types of structs that satsify the general tag of "sockaddr".
    */
    /*
    Here we define use "sockaddr_in" as "IPv4 addresses use the sockaddr type: sockaddr_in".
    This contains the following members of:
    > sin_addr - struct in_addr sin_addr;
    > sin_family - sa_family_t sin_family;
    > sin_len - __uint8_t sin_len;
    > sin_port - in_port_t sin_port;
    > sin_zero - char sin_zero[8];

    Notes:
    'sin_len' is a BSD/macOS-specific field that stores the length of the socket-address structure.

    Type 'in_addr_t' which is a typedef of __uint32_t in_addr_t; __uint32_t -> Unsigned integer of 32 bits, or 4 bytes.

    Type '__uint16_t' which is a typedef __uint16_t in_port_t; __uint16_t -> Unsigned integer of 16 bits, or 2 bytes.

    'sin_zero' is just padding — unused, meaningless bytes included only for historical binary-compatibility reasons. 
    Never read or written by the kernel. Just here for historical purposes.

    In C, there is no out-of-box type conversions of structs. What you can do is type cast the pointer!
    BTW: watch out for using 'htonl' -> unsinged 32 bit and 'htons' -> unsigned 16 bit for network ordering!
    */
    struct in_addr sin_addr_struct; 
    sin_addr_struct.s_addr = htonl(INADDR_LOOPBACK); // This is the unsigned integer version of localhost. Also needs to be network byte order.

    struct sockaddr_in address; 
    memset(&address, 0, sizeof(address)); // Here we have to do a "*memset(void *ptr, int value, size_t num);" because of "sin_zero" needs to initalized (although not read by kernel). Just good practice.
    address.sin_addr = sin_addr_struct; // __uint32_t, 4 bytes
    address.sin_family = AF_INET; // For structs that are addresses. Was told not to use 'PF_INET' as thats for socket protocols.
    address.sin_len = sizeof(address); // __uint8_t, 1 byte
    address.sin_port = htons(8080); // __uint16_t, 2 bytes. Important! Port needs to be in network byte order.

    struct sockaddr_in *addressPtr = &address; // Create a pointer for our IPv4 address.
    const struct sockaddr *baseAddressPtr = (const struct sockaddr *) addressPtr; // Then replace the pointer type to "sockaddr" with a cast.

    int bindingStatus = bind(fd, baseAddressPtr, sizeof(address));
    printf("Binding Status: %d.\n\n", bindingStatus);
    if (bindingStatus == -1) {
        int bindingErrNum = errno;
        printf("We have an error when binding socket!\n");
        printf("Error Number: %d.\n", bindingErrNum);

        switch(bindingErrNum) {
            case EACCES:
                printf("The requested address is protected, and the current user has inadequate permission to access it.\n");
                break;
            case EADDRINUSE:
                printf("The specified address is already in use.\n");
                break;
            case EADDRNOTAVAIL:
                printf("The specified address is not available from the local machine.\n"); // The local machine doesn't support that IP Address given the protocol format.
                break;
            case EAFNOSUPPORT:
                printf("address is not valid for the address family of socket.\n");
                break;
            case EBADF:
                printf("socket is not a valid file descriptor.\n");
                break;
            case EDESTADDRREQ:
                printf("socket is a null pointer.\n");
                break;
            case EFAULT:
                printf("The address parameter is not in a valid part of the user address space.\n");
                break;
            case EINVAL:
                printf("socket is already bound to an address and the protocol does not support binding to a new address.  Alternatively, socket may have been shut down.");
                break;
            case ENOTSOCK:
                printf("socket does not refer to a socket.\n");
                break;
            case EOPNOTSUPP:
                printf("socket is not of a type that can be bound to an address.\n");
                break;
            default:
                printf("A general error for binding has failed that is not covered by bind(2)!\n");
        }

        return -1;
    }

    printf("(3) Once we binded, we then need to kickstart the socket to be in a listening state!\n\n");
    const int maxConnectionsAllowed = 5;
    int listenStatus = listen(fd, maxConnectionsAllowed);
    
    printf("(4) Now we then accept any incoming connections on the queue.\n");
    /*
        Thinking about this high level. When an incoming connection is enqueued on this socket,
        accept unblocks and does a TCP handshake. Once that is done, we then create another
        socket for it and return a file descriptor. That file descriptor would be a session.
        From there, each session becomes a thread which then is created and will wait for "recv()".

        So an alternative architecture is having a single-thread execution architecture.
    */
    printf("Entering main event loop.\n");

    // Here we need to define our connection pool table.
    // - Insert Table Here -

    /*
        Kernel Level Events dealing with Sockets:
        EVFILT_READ - 
            Sockets which have previously been passed to listen() return when there is an incoming
            connection pending. data contains the size of the listen backlog.

            Other socket descriptors return when there is data to be read, subject to the
            SO_RCVLOWAT value of the socket buffer.  This may be overridden with a per-filter low
            water mark at the time the filter is added by setting the NOTE_LOWAT flag in fflags,
            and specifying the new low water mark in data.  The derived per filter low water mark
            value is, however, bounded by socket receive buffer's high and low water mark values.
            On return, data contains the number of bytes of protocol data available to read.

            The presence of EV_OOBAND in flags, indicates the presence of out of band data on the
            socket data equal to the potential number of OOB bytes availble to read.

            If the read direction of the socket has shutdown, then the filter also sets EV_EOF in
            flags, and returns the socket error (if any) in fflags.  It is possible for EOF to be
            returned (indicating the connection is gone) while there is still data pending in the
            socket buffer

        EVFILT_WRITE - 
            Takes a file descriptor as the identifier, and returns whenever it is possible to write to
            the descriptor.  For sockets, pipes and fifos, data will contain the amount of space
            remaining in the write buffer.  The filter will set EV_EOF when the reader disconnects,
            and for the fifo case, this may be cleared by use of EV_CLEAR.  Note that this filter is
            not supported for vnodes.

            For sockets, the low water mark and socket error handling is identical to the EVFILT_READ
            case.

        EVFILT_EXCEPT - 
            Currently, this filter can be used to monitor the arrival of out-of-band data on a socket
            descriptor using the filter flag NOTE_OOB.

            If the read direction of the socket has shutdown, then the filter also sets EV_EOF in
            flags, and returns the socket error (if any) in fflags.
    */


    /*
        *************** SINGLE EVENT THREAD EVENT LOOP. ***************
        Here we have our single-thread execution architecture.
        This is our main event loop that runs on one thread.
    */
    int kqueueFD = kqueue();
    printf("Created \"kqueue\" successfully!\n");

    struct kevent eventTypeA; // Define a the 'kevent' struct
    EV_SET(&eventTypeA, fd, EVFILT_READ, EV_ADD, 0, 0, NULL); // Then we fill the struct out.
    kevent(kqueueFD, &eventTypeA, 1, NULL, 0, NULL);

    while(TRUE) {
        // Lets try a simple poll.
        struct kevent event;
        int eventStatus = kevent(kqueueFD, NULL, 0, &event, 1, NULL); // This blocks the thread.

        if (eventStatus == -1) {
            printf("Event Status ran into an error (returned -1)!\n");
            break;
        }

        if (eventStatus > 0) {
            printf("Event from kqueue returned withs status: %d.\n", eventStatus);
            printf("We officially got an 'EVFILT_READ event!\n");

            // If this is not in the connection table, we have to accept.
            struct sockaddr_in peer;
            socklen_t peerSize = sizeof(peer);
            socklen_t *peerSizePnter = &peerSize;
            struct sockaddr_in *peerPointer = &peer;
            struct sockaddr *basePeerPointer = (struct sockaddr *) peerPointer;
            int acceptedConnection = accept(fd, basePeerPointer, peerSizePnter);

            printf("Accepted connection with file descriptor client socket: %d.\n", acceptedConnection);
        }

        // We create kevent arrays
        // When the load them onto the kevent
        // Iterate through them
        // Check for acceptance events
        // Check for read events
        /*
            If there are any connections that enter the listen socket. We can check this by
            looking for a "EVFILT_READ" event flag within the kqueue that monitors the listen 'fd'.

            We then accept the enqueue connection using the socket "fd". This then returns a
            file descriptor to which we can refer it as a the client file descriptor. NOTE:
            this creates a new socket! Essentially we have our own connection socket to the client!

            Using the client file descriptor, we can then read "EVFILT_READ" event flag on that
            to which if incoming data is present, we then can invoke the "read()" function to direct
            to the user read buffer.
        */
    } 

    return 0;
}






