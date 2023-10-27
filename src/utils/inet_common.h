#pragma once

#ifdef _WIN32
#include <winsock2.h> /* htonll */
#include <ws2tcpip.h> /* addrinfo */

typedef int ssize_t;
typedef SOCKET MYSOCKET;

#else
#include <arpa/inet.h> /* inet_pton */
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#define closesocket close
typedef int MYSOCKET;

#endif
