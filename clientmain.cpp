#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <cstdlib>
#include "protocol.h"
#include "calcLib.h"

void checkPortValidity(int port) {
    if (port <= 0 || port > 65535) {
        throw std::runtime_error("Invalid port number");
    }
}

int createSocket(const std::string& host, const std::string& port, int socktype) {
    addrinfo hints{}, *res;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = socktype;

    int rc = getaddrinfo(host.c_str(), port.c_str(), &hints, &res);
    if (rc != 0) {
        throw std::runtime_error("getaddrinfo failed");
    }

    int sock = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        throw std::runtime_error("socket() failed");
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        close(sock);
        throw std::runtime_error("connect() failed");
    }

    freeaddrinfo(res);
    return sock;
}

// ========== TEXT ==========
void performTextCommunication(int sock) {
    char buffer[1024];
    int n = read(sock, buffer, sizeof(buffer)-1);
    if (n <= 0) {
        std::cout << "ERROR" << std::endl;
        exit(EXIT_FAILURE);
    }
    buffer[n] = '\0';
    std::string handshake(buffer);

    if (handshake.find("TEXT TCP 1.1") == std::string::npos) {
        std::cout << "ERROR" << std::endl;
        exit(EXIT_FAILURE);
    }
    write(sock, "OK\n", 3);

    // Läs assignment
    n = read(sock, buffer, sizeof(buffer)-1);
    buffer[n] = '\0';
    std::string assignment(buffer);

    std::istringstream iss(assignment);
    std::string op; double a,b;
    iss >> op >> a >> b;
    double result;
    if (op=="add") result = a+b;
    else if (op=="sub") result = a-b;
    else if (op=="mul") result = a*b;
    else if (op=="div") result = a/b;
    else {
        std::cout << "ERROR" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::ostringstream oss; oss<<result<<"\n";
    write(sock, oss.str().c_str(), oss.str().size());

    n = read(sock, buffer, sizeof(buffer)-1);
    buffer[n] = '\0';
    std::string response(buffer);

    if (response.find("OK") != std::string::npos) {
        std::cout << "OK" << std::endl;
    } else {
        std::cout << "ERROR" << std::endl;
        exit(EXIT_FAILURE);
    }
}

// ========== BINARY ==========
void performBinaryCommunication(int sock) {
    char buffer[1024];
    int n = read(sock, buffer, sizeof(buffer)-1);
    buffer[n] = '\0';
    std::string handshake(buffer);

    if (handshake.find("BINARY TCP 1.1") == std::string::npos) {
        std::cout << "ERROR" << std::endl;
        exit(EXIT_FAILURE);
    }
    write(sock, "OK\n", 3);

    calcProtocol cp{};
    if (read(sock, &cp, sizeof(cp)) != sizeof(cp)) {
        std::cout << "ERROR" << std::endl;
        exit(EXIT_FAILURE);
    }

    double result;
    switch(cp.arith) {
        case 1: result = ntohl(cp.inValue1) + ntohl(cp.inValue2); break;
        case 2: result = ntohl(cp.inValue1) - ntohl(cp.inValue2); break;
        case 3: result = ntohl(cp.inValue1) * ntohl(cp.inValue2); break;
        case 4: result = (double)ntohl(cp.inValue1) / ntohl(cp.inValue2); break;
        default: std::cout<<"ERROR"<<std::endl; exit(EXIT_FAILURE);
    }

    cp.result = htonl((int32_t)result);
    write(sock, &cp, sizeof(cp));

    n = read(sock, buffer, sizeof(buffer)-1);
    buffer[n] = '\0';
    std::string response(buffer);

    if (response.find("OK") != std::string::npos) {
        std::cout << "OK" << std::endl;
    } else {
        std::cout << "ERROR" << std::endl;
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: "<<argv[0]<<" protocol://server:port/path\n";
        return 1;
    }

    // Enkel tokenizer: tcp://127.0.0.1:5000/text
    std::string input = argv[1];
    std::string protocol = input.substr(0, input.find("://"));
    std::string rest = input.substr(input.find("://")+3);

    std::string hostport = rest.substr(0, rest.find("/"));
    std::string path = rest.substr(rest.find("/")+1);
    std::string host = hostport.substr(0, hostport.find(":"));
    std::string port = hostport.substr(hostport.find(":")+1);

    checkPortValidity(std::stoi(port));

    if (protocol == "tcp") {
        int sock = createSocket(host, port, SOCK_STREAM);
        if (path == "text") performTextCommunication(sock);
        else if (path == "binary") performBinaryCommunication(sock);
    } else if (protocol == "any") {
        try {
            int sock = createSocket(host, port, SOCK_STREAM);
            if (path == "text") performTextCommunication(sock);
            else if (path == "binary") performBinaryCommunication(sock);
        } catch (...) {
            int sock = createSocket(host, port, SOCK_DGRAM);
            if (path == "text") performTextCommunication(sock);
            else if (path == "binary") performBinaryCommunication(sock);
        }
    } else {
        std::cerr << "Unknown protocol: "<<protocol<<"\n";
        return 1;
    }

    return 0;
}
