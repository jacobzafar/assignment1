#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include "protocol.h"

#define DEBUG
#include <calcLib.h>

int handle_tcp(const char *Desthost, const char *Destport, const char *Destpath);
int handle_udp(const char *Desthost, const char *Destport, const char *Destpath);

int main(int argc, char *argv[]){
  if (argc < 2) {
    fprintf(stderr, "Usage: %s protocol://server:port/path.\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  char protocolstring[6], hoststring[2000],portstring[6], pathstring[7];
  char *input = argv[1];

  if (strstr(input, "///") != NULL ){
    printf("Invalid format: %s.\n", input);
    return 1;
  }

  char *proto_end = strstr(input, "://");
  if (!proto_end) {
    printf("Invalid format: missing '://'\n");
    return 1;
  }

  size_t proto_len = proto_end - input;
  if (proto_len >= sizeof(protocolstring)) {
    fprintf(stderr, "Error: Protocol string too long\n");
    return 1;
  }

  strncpy(protocolstring, input, proto_end - input);
  protocolstring[proto_end - input] = '\0';

  char *host_start = proto_end + 3;
  char *port_start = strchr(host_start, ':');
  if (!port_start || port_start == host_start) {
    printf("Error: Port is missing or ':' is misplaced\n");
    return 1;
  }

  size_t host_len = port_start - host_start;
  if (host_len >= sizeof(hoststring)) {
    printf("Error: Host string too long\n");
    return 1;
  }

  strncpy(hoststring, host_start, port_start - host_start);
  hoststring[port_start - host_start] = '\0';

  char *path_start = strchr(host_start, '/');
  if (!path_start || *(path_start + 1) == '\0') {
    fprintf(stderr, "Error: Path is missing or invalid\n");
    return 1;
  }

  if (strlen(path_start + 1) >= sizeof(pathstring)) {
    fprintf(stderr, "Error: Path string too long\n");
    return 1;
  }
  strcpy(pathstring, path_start + 1);

  size_t port_len = path_start - port_start - 1;
  if (port_len >= sizeof(portstring)) {
    fprintf(stderr, "Error: Port string too long\n");
    return 1;
  }
  strncpy(portstring, port_start + 1, port_len);
  portstring[port_len] = '\0';

  for (size_t i = 0; i < strlen(portstring); ++i) {
    if (portstring[i] < '0' || portstring[i] > '9') {
      fprintf(stderr, "Error: Port must be numeric\n");
      return 1;
    }
  }

  char *protocol, *Desthost, *Destport, *Destpath;
  protocol=protocolstring;
  Desthost=hoststring;
  Destport=portstring;
  Destpath=pathstring;

  int port=atoi(Destport);
  if (port < 1 || port >65535) {
    printf("Error: Port is out of server scope.\n");
    if ( port > 65535 ) {
      printf("Error: Port is not a valid UDP or TCP port.\n");
    }
    return 1;
  }
#ifdef DEBUG 
  printf("Protocol: %s Host %s, port = %d and path = %s.\n",protocol, Desthost,port, Destpath);
#endif

if (strcasecmp(protocol, "tcp") == 0) {
    return handle_tcp(Desthost, Destport, Destpath);
}
else if (strcasecmp(protocol, "udp") == 0) {
    return handle_udp(Desthost, Destport, Destpath);
}
else if (strcasecmp(protocol, "any") == 0) {
    printf("Protocol 'any' explicitly supported.\n");
    printf("Trying TCP first...\n");
    if (handle_tcp(Desthost, Destport, Destpath) == 0) {
        return 0;
    }
    printf("TCP failed, trying UDP...\n");
    return handle_udp(Desthost, Destport, Destpath);
}
else {
    fprintf(stderr, "Error: Unknown or unsupported protocol: %s\n", protocol);
    return 1;
}

}

// ---------------- HANDLE TCP ----------------
int handle_tcp(const char *Desthost, const char *Destport, const char *Destpath) {
    struct addrinfo hints, *addrinfoRes;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    getaddrinfo(Desthost, Destport, &hints, &addrinfoRes);

    int socket1 = socket(addrinfoRes->ai_family, addrinfoRes->ai_socktype, addrinfoRes->ai_protocol);

    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    setsockopt(socket1, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    connect(socket1, addrinfoRes->ai_addr, addrinfoRes->ai_addrlen);

#ifdef DEBUG
    printf("kopllade upp sig till %s:%s via TCP\n", Desthost, Destport);
#endif

    char buffer[1024];
    int n = recv(socket1, buffer, sizeof(buffer), 0);

    if (n <= 0) {
        printf("ERROR: MESSAGE LOST (TIMEOUT)\n");
        close(socket1);
        freeaddrinfo(addrinfoRes);
        exit(EXIT_FAILURE);
    }
    buffer[n] = '\0';

#ifdef DEBUG
    printf("Server handshake:\n%s\n", buffer);
#endif

    if (strcasecmp(Destpath, "text") == 0) {
        if (strstr(buffer, "TEXT TCP 1.1") == NULL) {
            printf("ERROR\n");
            freeaddrinfo(addrinfoRes);
            close(socket1);
            return EXIT_FAILURE;
        }
        if (strstr(buffer, "TEXT TCP 1.1")) {
            const char* reply = "TEXT TCP 1.1 OK\n";
            send(socket1, reply, strlen(reply), 0);
#ifdef DEBUG
            printf("svarade: %s", reply);
#endif
        }

        char buffer[1024];
        int n = recv(socket1, buffer, sizeof(buffer), 0);
        if (n <= 0) {
            printf("ERROR: MESSAGE LOST (TIMEOUT)\n");
            close(socket1);
            freeaddrinfo(addrinfoRes);
            exit(EXIT_FAILURE);
        }
        buffer[n] = '\0';

        printf("ASSIGNMENT: %s\n", buffer);

        char operation[10];
        int value1, value2;
        sscanf(buffer, "%s %d %d", operation, &value1, &value2);

#ifdef DEBUG
        printf("Operation: %s, value1=%d, value2=%d\n", operation, value1, value2);
#endif

        int result = 0;
        if (strcmp(operation, "add") == 0) result = value1 + value2;
        else if (strcmp(operation, "sub") == 0) result = value1 - value2;
        else if (strcmp(operation, "mul") == 0) result = value1 * value2;
        else if (strcmp(operation, "div") == 0) result = value1 / value2;

#ifdef DEBUG
        printf("Resultat: %d\n", result);
#endif

        char answer[10];
        sprintf(answer, "%d\n", result);
        send(socket1, answer, strlen(answer), 0);

        int m = recv(socket1, buffer, sizeof(buffer)-1, 0);
        if (m <= 0) {
            printf("ERROR: MESSAGE LOST (TIMEOUT)\n");
            close(socket1);
            freeaddrinfo(addrinfoRes);
            exit(EXIT_FAILURE);
        }
        buffer[m] = '\0';

        printf("%s (myresult=%d)\n", buffer, result);

        close(socket1);
        freeaddrinfo(addrinfoRes);
    }
    else if (strcasecmp(Destpath, "binary") == 0) {
        if (strstr(buffer, "BINARY TCP 1.1") == NULL) {
            printf("ERROR\n");
            freeaddrinfo(addrinfoRes);
            close(socket1);
            return EXIT_FAILURE;
        }

        const char* handshakereply = "BINARY TCP 1.1 OK\n";
        if (send(socket1, handshakereply, strlen(handshakereply), 0) <= 0) {
            printf("ERROR: CANT CONNECT TO %s\n", Desthost);
            close(socket1);
            freeaddrinfo(addrinfoRes);
            exit(EXIT_FAILURE);
        }

        struct calcProtocol task;
        int n = recv(socket1, &task, sizeof(task), 0);

        if (n <= 0) {
            printf("ERROR: MESSAGE LOST (TIMEOUT)\n");
            close(socket1);
            freeaddrinfo(addrinfoRes);
            exit(EXIT_FAILURE);
        }

#ifdef DEBUG
        uint16_t type    = ntohs(task.type);
        uint16_t major   = ntohs(task.major_version);
        uint16_t minor   = ntohs(task.minor_version);
        uint32_t id      = ntohl(task.id);
        uint32_t arith   = ntohl(task.arith);
        int32_t value1   = ntohl(task.inValue1);
        int32_t value2   = ntohl(task.inValue2);

        printf("DEBUG: type=%u version=%u.%u id=%u arith=%u value1=%d value2=%d\n",
               type, major, minor, id, arith, value1, value2);
#endif

        int32_t result = 0;
        const char* opname = "unknown";

        switch (ntohl(task.arith)) {
            case 1: opname = "add"; result = ntohl(task.inValue1) + ntohl(task.inValue2); break;
            case 2: opname = "sub"; result = ntohl(task.inValue1) - ntohl(task.inValue2); break;
            case 3: opname = "mul"; result = ntohl(task.inValue1) * ntohl(task.inValue2); break;
            case 4: opname = "div"; result = ntohl(task.inValue1) / ntohl(task.inValue2); break;
        }

        printf("ASSIGNMENT: %s %d %d\n", opname, ntohl(task.inValue1), ntohl(task.inValue2));

        task.type     = htons(2);
        task.inResult = htonl(result);

        send(socket1, &task, sizeof(task), 0);

        struct calcMessage response;
        int m = recv(socket1, &response, sizeof(response), 0);
        if (m <= 0) {
            fprintf(stderr, "ERROR: MESSAGE LOST (TIMEOUT)\n");
            close(socket1);
            freeaddrinfo(addrinfoRes);
            exit(EXIT_FAILURE);
        }

        uint32_t message = ntohl(response.message);
        if (message == 1) printf("OK (myresult=%d)\n", result);
        else if (message == 2) printf("NOT OK (myresult=%d)\n", result);
        else printf("ERROR: UNKNOWN RESPONSE\n");

        close(socket1);
        freeaddrinfo(addrinfoRes);
        exit(EXIT_SUCCESS);
    }
    return 0;
}

// ---------------- HANDLE UDP ----------------
int handle_udp(const char *Desthost, const char *Destport, const char *Destpath) {
    if (strcasecmp(Destpath, "binary") == 0) {
        struct addrinfo udpHints, *udpResult = NULL;
        memset(&udpHints, 0, sizeof(udpHints));
        udpHints.ai_family   = AF_UNSPEC;
        udpHints.ai_socktype = SOCK_DGRAM;

        getaddrinfo(Desthost, Destport, &udpHints, &udpResult);
        int udpSocket = socket(udpResult->ai_family, udpResult->ai_socktype, udpResult->ai_protocol);

        struct timeval tv;
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        setsockopt(udpSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

        struct calcMessage firstMessage;
        firstMessage.type          = htons(22);
        firstMessage.message       = htonl(0);
        firstMessage.protocol      = htons(17);
        firstMessage.major_version = htons(1);
        firstMessage.minor_version = htons(1);

        sendto(udpSocket, &firstMessage, sizeof(firstMessage), 0, udpResult->ai_addr, udpResult->ai_addrlen);

        struct calcProtocol task;
        int n = recvfrom(udpSocket, &task, sizeof(task), 0, NULL, NULL);
        if (n <= 0) {
            printf("ERROR: MESSAGE LOST (TIMEOUT)\n");
            close(udpSocket);
            freeaddrinfo(udpResult);
            exit(EXIT_FAILURE);
        }

#ifdef DEBUG
        printf("DEBUG: Fick %d bytes från servern via UDP\n", n);
#endif

        uint32_t arith  = ntohl(task.arith);
        int32_t  value1 = ntohl(task.inValue1);
        int32_t  value2 = ntohl(task.inValue2);

        int32_t result = 0;
        const char* opname = "unknown";
        switch (arith) {
            case 1: opname = "add"; result = value1 + value2; break;
            case 2: opname = "sub"; result = value1 - value2; break;
            case 3: opname = "mul"; result = value1 * value2; break;
            case 4: opname = "div"; result = value1 / value2; break;
        }

        printf("ASSIGNMENT: %s %d %d\n", opname, value1, value2);

        task.type     = htons(2);
        task.inResult = htonl(result);

        sendto(udpSocket, &task, sizeof(task), 0, udpResult->ai_addr, udpResult->ai_addrlen);

        struct calcMessage response;
        int m = recvfrom(udpSocket, &response, sizeof(response), 0, NULL, NULL);
        if (m <= 0) {
            printf("ERROR: MESSAGE LOST (TIMEOUT)\n");
            close(udpSocket);
            freeaddrinfo(udpResult);
            exit(EXIT_FAILURE);
        }

        uint32_t message = ntohl(response.message);
        if (message == 1) printf("OK (myresult=%d)\n", result);
        else if (message == 2) printf("NOT OK (myresult=%d)\n", result);
        else printf("ERROR: UNKNOWN RESPONSE\n");

        close(udpSocket);
        freeaddrinfo(udpResult);
        exit(EXIT_SUCCESS);
    }
    else if (strcasecmp(Destpath, "text") == 0) {
        struct addrinfo udpHints, *udpResult = NULL;
        memset(&udpHints, 0, sizeof(udpHints));
        udpHints.ai_family   = AF_UNSPEC;
        udpHints.ai_socktype = SOCK_DGRAM;

        getaddrinfo(Desthost, Destport, &udpHints, &udpResult);
        int udpSocket = socket(udpResult->ai_family, udpResult->ai_socktype, udpResult->ai_protocol);

        struct timeval tv;
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        setsockopt(udpSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

        const char* firstmessage = "TEXT UDP 1.1\n";
        sendto(udpSocket, firstmessage, strlen(firstmessage), 0, udpResult->ai_addr, udpResult->ai_addrlen);

        char buffer[1024];
        int n = recvfrom(udpSocket, buffer, sizeof(buffer), 0, NULL, NULL);
        if (n <= 0) {
            printf("ERROR: MESSAGE
