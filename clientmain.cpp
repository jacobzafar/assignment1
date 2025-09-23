#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <system_error>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cerrno>
#include <sstream>
#include <regex>

// Protocol and API type enums
enum class Protocol { TCP, UDP, ANY };
enum class ApiType { TEXT, BINARY };

// Binary protocol constants
const uint16_t CALC_MESSAGE_TYPE = 22;
const uint16_t CALC_PROTOCOL_TYPE = 1;
const uint16_t ERROR_MESSAGE_TYPE = 2;
const uint16_t PROTOCOL_ID = 17;
const uint16_t MAJOR_VERSION = 1;
const uint16_t MINOR_VERSION = 0;

// Binary message structures
#pragma pack(push, 1)
struct CalcMessage {
    uint16_t type;
    uint16_t message;
    uint16_t protocol;
    uint16_t major_version;
    uint16_t minor_version;
};

struct CalcProtocol {
    uint16_t type;
    uint16_t major_version;
    uint16_t minor_version;
    uint32_t id;
    uint32_t arith;
    int32_t in_value1;
    int32_t in_value2;
    int32_t in_result;  // Only used in response
};
#pragma pack(pop)

// Function prototypes
void parseURL(const std::string& url, Protocol& protocol, std::string& host, int& port, ApiType& apiType);
addrinfo* resolveHost(const std::string& host, int port, Protocol protocol);
bool handleTCPText(int sockfd);
bool handleTCPBinary(int sockfd);
bool handleUDPText(int sockfd, const struct sockaddr_in& server_addr);
bool handleUDPBinary(int sockfd, const struct sockaddr_in& server_addr);

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " PROTOCOL://host:port/api" << std::endl;
        std::cerr << "Example: " << argv[0] << " TCP://alice.nplab.bth.se:5000/text" << std::endl;
        return 1;
    }

    Protocol protocol;
    std::string host;
    int port;
    ApiType apiType;

    try {
        parseURL(argv[1], protocol, host, port, apiType);
        std::cout << "Protocol: ";
        switch (protocol) {
            case Protocol::TCP: std::cout << "TCP"; break;
            case Protocol::UDP: std::cout << "UDP"; break;
            case Protocol::ANY: std::cout << "ANY"; break;
        }
        std::cout << ", Host: " << host << ", Port: " << port << ", API: ";
        switch (apiType) {
            case ApiType::TEXT: std::cout << "TEXT"; break;
            case ApiType::BINARY: std::cout << "BINARY"; break;
        }
        std::cout << std::endl;

        bool success = false;

        if (protocol == Protocol::ANY) {
            // Try TCP first
            addrinfo* tcpAddrInfo = resolveHost(host, port, Protocol::TCP);
            int tcpSockfd = socket(tcpAddrInfo->ai_family, tcpAddrInfo->ai_socktype, tcpAddrInfo->ai_protocol);
            
            if (tcpSockfd >= 0) {
                if (connect(tcpSockfd, tcpAddrInfo->ai_addr, tcpAddrInfo->ai_addrlen) >= 0) {
                    if (apiType == ApiType::TEXT) {
                        success = handleTCPText(tcpSockfd);
                    } else {
                        success = handleTCPBinary(tcpSockfd);
                    }
                }
                close(tcpSockfd);
            }
            
            freeaddrinfo(tcpAddrInfo);

            // If TCP failed, try UDP
            if (!success) {
                std::cout << "TCP failed, trying UDP..." << std::endl;
                addrinfo* udpAddrInfo = resolveHost(host, port, Protocol::UDP);
                int udpSockfd = socket(udpAddrInfo->ai_family, udpAddrInfo->ai_socktype, udpAddrInfo->ai_protocol);
                
                if (udpSockfd >= 0) {
                    struct sockaddr_in server_addr;
                    memset(&server_addr, 0, sizeof(server_addr));
                    server_addr = *(struct sockaddr_in*)udpAddrInfo->ai_addr;
                    
                    if (apiType == ApiType::TEXT) {
                        success = handleUDPText(udpSockfd, server_addr);
                    } else {
                        success = handleUDPBinary(udpSockfd, server_addr);
                    }
                    close(udpSockfd);
                }
                
                freeaddrinfo(udpAddrInfo);
            }
        } else if (protocol == Protocol::TCP) {
            addrinfo* addrInfo = resolveHost(host, port, Protocol::TCP);
            int sockfd = socket(addrInfo->ai_family, addrInfo->ai_socktype, addrInfo->ai_protocol);
            
            if (sockfd >= 0) {
                if (connect(sockfd, addrInfo->ai_addr, addrInfo->ai_addrlen) >= 0) {
                    if (apiType == ApiType::TEXT) {
                        success = handleTCPText(sockfd);
                    } else {
                        success = handleTCPBinary(sockfd);
                    }
                }
                close(sockfd);
            }
            
            freeaddrinfo(addrInfo);
        } else { // UDP
            addrinfo* addrInfo = resolveHost(host, port, Protocol::UDP);
            int sockfd = socket(addrInfo->ai_family, addrInfo->ai_socktype, addrInfo->ai_protocol);
            
            if (sockfd >= 0) {
                struct sockaddr_in server_addr;
                memset(&server_addr, 0, sizeof(server_addr));
                server_addr = *(struct sockaddr_in*)addrInfo->ai_addr;
                
                if (apiType == ApiType::TEXT) {
                    success = handleUDPText(sockfd, server_addr);
                } else {
                    success = handleUDPBinary(sockfd, server_addr);
                }
                close(sockfd);
            }
            
            freeaddrinfo(addrInfo);
        }

        return success ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
}

void parseURL(const std::string& url, Protocol& protocol, std::string& host, int& port, ApiType& apiType) {
    std::regex urlRegex("([a-zA-Z]+)://([^:/]+):(\\d+)/([a-zA-Z]+)");
    std::smatch matches;
    
    if (!std::regex_match(url, matches, urlRegex)) {
        throw std::runtime_error("Invalid URL format");
    }
    
    std::string protocolStr = matches[1];
    host = matches[2];
    port = std::stoi(matches[3]);
    std::string apiStr = matches[4];
    
    // Convert protocol string to enum
    if (protocolStr == "TCP" || protocolStr == "tcp") {
        protocol = Protocol::TCP;
    } else if (protocolStr == "UDP" || protocolStr == "udp") {
        protocol = Protocol::UDP;
    } else if (protocolStr == "ANY" || protocolStr == "any") {
        protocol = Protocol::ANY;
    } else {
        throw std::runtime_error("Invalid protocol: " + protocolStr);
    }
    
    // Convert API string to enum
    if (apiStr == "text" || apiStr == "TEXT") {
        apiType = ApiType::TEXT;
    } else if (apiStr == "binary" || apiStr == "BINARY") {
        apiType = ApiType::BINARY;
    } else {
        throw std::runtime_error("Invalid API type: " + apiStr);
    }
}

addrinfo* resolveHost(const std::string& host, int port, Protocol protocol) {
    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    
    if (protocol == Protocol::TCP || protocol == Protocol::ANY) {
        hints.ai_socktype = SOCK_STREAM;
    } else {
        hints.ai_socktype = SOCK_DGRAM;
    }
    
    addrinfo* result;
    int status = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &result);
    if (status != 0) {
        throw std::runtime_error("Resolve issue: " + std::string(gai_strerror(status)));
    }
    
    return result;
}

bool handleTCPText(int sockfd) {
    try {
        // Read server protocols
        std::vector<std::string> protocols;
        char buffer[1024];
        std::string data;
        
        while (true) {
            ssize_t bytesRead = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
            if (bytesRead <= 0) {
                throw std::runtime_error("Connection closed by server");
            }
            
            buffer[bytesRead] = '\0';
            data += buffer;
            
            size_t pos = data.find('\n');
            if (pos != std::string::npos) {
                std::istringstream iss(data.substr(0, pos));
                std::string line;
                while (std::getline(iss, line)) {
                    if (line.empty()) break;
                    protocols.push_back(line);
                }
                break;
            }
        }
        
        // Check if TEXT TCP 1.1 is supported
        bool textTcp11Supported = false;
        for (const auto& p : protocols) {
            if (p == "TEXT TCP 1.1") {
                textTcp11Supported = true;
                break;
            }
        }
        
        if (!textTcp11Supported) {
            std::cerr << "ERROR: MISSMATCH PROTOCOL" << std::endl;
            return false;
        }
        
        // Send acceptance
        std::string acceptMsg = "TEXT TCP 1.1 OK\n";
        if (send(sockfd, acceptMsg.c_str(), acceptMsg.length(), 0) < 0) {
            throw std::runtime_error("Send failed: " + std::string(strerror(errno)));
        }
        
        // Read assignment
        std::string assignment;
        char ch;
        while (recv(sockfd, &ch, 1, 0) > 0) {
            if (ch == '\n') break;
            assignment += ch;
        }
        
        std::cout << "ASSIGNMENT: " << assignment << std::endl;
        
        // Parse and calculate
        std::istringstream iss(assignment);
        std::string op, val1Str, val2Str;
        iss >> op >> val1Str >> val2Str;
        
        if (iss.fail()) {
            std::cerr << "ERROR: Invalid assignment format" << std::endl;
            return false;
        }
        
        int val1 = std::stoi(val1Str);
        int val2 = std::stoi(val2Str);
        int result;
        
        if (op == "add") {
            result = val1 + val2;
        } else if (op == "sub") {
            result = val1 - val2;
        } else if (op == "mul") {
            result = val1 * val2;
        } else if (op == "div") {
            if (val2 == 0) {
                std::cerr << "ERROR: Division by zero" << std::endl;
                return false;
            }
            result = val1 / val2;
        } else {
            std::cerr << "ERROR: Unknown operation: " << op << std::endl;
            return false;
        }
        
        // Send result
        std::string resultStr = std::to_string(result) + "\n";
        if (send(sockfd, resultStr.c_str(), resultStr.length(), 0) < 0) {
            throw std::runtime_error("Send failed: " + std::string(strerror(errno)));
        }
        
        // Read response
        std::string response;
        while (recv(sockfd, &ch, 1, 0) > 0) {
            if (ch == '\n') break;
            response += ch;
        }
        
        if (response == "OK") {
            std::cout << "OK" << std::endl;
            return true;
        } else {
            std::cout << "ERROR" << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return false;
    }
}

bool handleTCPBinary(int sockfd) {
    try {
        // Read server protocols
        std::vector<std::string> protocols;
        char buffer[1024];
        std::string data;
        
        while (true) {
            ssize_t bytesRead = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
            if (bytesRead <= 0) {
                throw std::runtime_error("Connection closed by server");
            }
            
            buffer[bytesRead] = '\0';
            data += buffer;
            
            size_t pos = data.find('\n');
            if (pos != std::string::npos) {
                std::istringstream iss(data.substr(0, pos));
                std::string line;
                while (std::getline(iss, line)) {
                    if (line.empty()) break;
                    protocols.push_back(line);
                }
                break;
            }
        }
        
        // Check if BINARY TCP 1.1 is supported
        bool binaryTcp11Supported = false;
        for (const auto& p : protocols) {
            if (p == "BINARY TCP 1.1") {
                binaryTcp11Supported = true;
                break;
            }
        }
        
        if (!binaryTcp11Supported) {
            std::cerr << "ERROR: MISSMATCH PROTOCOL" << std::endl;
            return false;
        }
        
        // Send acceptance
        std::string acceptMsg = "BINARY TCP 1.1 OK\n";
        if (send(sockfd, acceptMsg.c_str(), acceptMsg.length(), 0) < 0) {
            throw std::runtime_error("Send failed: " + std::string(strerror(errno)));
        }
        
        // Read binary protocol message
        CalcProtocol calcProto;
        ssize_t totalRead = 0;
        while (totalRead < sizeof(CalcProtocol) - sizeof(int32_t)) { // -4 because in_result is not sent by server
            ssize_t bytesRead = recv(sockfd, reinterpret_cast<char*>(&calcProto) + totalRead, 
                                    sizeof(CalcProtocol) - sizeof(int32_t) - totalRead, 0);
            if (bytesRead <= 0) {
                throw std::runtime_error("Connection closed by server");
            }
            totalRead += bytesRead;
        }
        
        // Convert from network byte order
        calcProto.type = ntohs(calcProto.type);
        calcProto.major_version = ntohs(calcProto.major_version);
        calcProto.minor_version = ntohs(calcProto.minor_version);
        calcProto.id = ntohl(calcProto.id);
        calcProto.arith = ntohl(calcProto.arith);
        calcProto.in_value1 = ntohl(calcProto.in_value1);
        calcProto.in_value2 = ntohl(calcProto.in_value2);
        
        if (calcProto.type != CALC_PROTOCOL_TYPE) {
            std::cerr << "ERROR: Invalid message type" << std::endl;
            return false;
        }
        
        // Calculate result
        int32_t result;
        if (calcProto.arith == 0) { // add
            result = calcProto.in_value1 + calcProto.in_value2;
        } else if (calcProto.arith == 1) { // sub
            result = calcProto.in_value1 - calcProto.in_value2;
        } else if (calcProto.arith == 2) { // mul
            result = calcProto.in_value1 * calcProto.in_value2;
        } else if (calcProto.arith == 3) { // div
            if (calcProto.in_value2 == 0) {
                std::cerr << "ERROR: Division by zero" << std::endl;
                return false;
            }
            result = calcProto.in_value1 / calcProto.in_value2;
        } else {
            std::cerr << "ERROR: Unknown arithmetic operation: " << calcProto.arith << std::endl;
            return false;
        }
        
        // Prepare response
        CalcProtocol responseProto;
        responseProto.type = htons(CALC_PROTOCOL_TYPE);
        responseProto.major_version = htons(MAJOR_VERSION);
        responseProto.minor_version = htons(MINOR_VERSION);
        responseProto.id = htonl(calcProto.id);
        responseProto.arith = htonl(calcProto.arith);
        responseProto.in_value1 = htonl(calcProto.in_value1);
        responseProto.in_value2 = htonl(calcProto.in_value2);
        responseProto.in_result = htonl(result);
        
        // Send response
        if (send(sockfd, &responseProto, sizeof(responseProto), 0) < 0) {
            throw std::runtime_error("Send failed: " + std::string(strerror(errno)));
        }
        
        // Read server response
        char responseBuffer[1024];
        ssize_t bytesRead = recv(sockfd, responseBuffer, sizeof(responseBuffer) - 1, 0);
        if (bytesRead <= 0) {
            throw std::runtime_error("Connection closed by server");
        }
        
        responseBuffer[bytesRead] = '\0';
        std::string response(responseBuffer);
        
        if (response.find("OK") != std::string::npos) {
            std::cout << "OK" << std::endl;
            return true;
        } else {
            std::cout << "ERROR" << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return false;
    }
}

bool handleUDPText(int sockfd, const struct sockaddr_in& server_addr) {
    try {
        // Set timeout
        struct timeval tv;
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            throw std::runtime_error("Setsockopt failed: " + std::string(strerror(errno)));
        }
        
        // Send initial message
        std::string initMsg = "TEXT UDP 1.1\n";
        if (sendto(sockfd, initMsg.c_str(), initMsg.length(), 0, 
                  (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            throw std::runtime_error("Send failed: " + std::string(strerror(errno)));
        }
        
        // Receive assignment
        char buffer[1024];
        socklen_t addrLen = sizeof(server_addr);
        ssize_t bytesRead = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, 
                                    (struct sockaddr*)&server_addr, &addrLen);
        if (bytesRead < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::cerr << "ERROR: MESSAGE LOST (TIMEOUT)" << std::endl;
            } else {
                throw std::runtime_error("Recv failed: " + std::string(strerror(errno)));
            }
            return false;
        }
        
        buffer[bytesRead] = '\0';
        std::string assignment(buffer);
        std::cout << "ASSIGNMENT: " << assignment << std::endl;
        
        // Parse and calculate
        std::istringstream iss(assignment);
        std::string op, val1Str, val2Str;
        iss >> op >> val1Str >> val2Str;
        
        if (iss.fail()) {
            std::cerr << "ERROR: Invalid assignment format" << std::endl;
            return false;
        }
        
        int val1 = std::stoi(val1Str);
        int val2 = std::stoi(val2Str);
        int result;
        
        if (op == "add") {
            result = val1 + val2;
        } else if (op == "sub") {
            result = val1 - val2;
        } else if (op == "mul") {
            result = val1 * val2;
        } else if (op == "div") {
            if (val2 == 0) {
                std::cerr << "ERROR: Division by zero" << std::endl;
                return false;
            }
            result = val1 / val2;
        } else {
            std::cerr << "ERROR: Unknown operation: " << op << std::endl;
            return false;
        }
        
        // Send result
        std::string resultStr = std::to_string(result) + "\n";
        if (sendto(sockfd, resultStr.c_str(), resultStr.length(), 0, 
                  (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            throw std::runtime_error("Send failed: " + std::string(strerror(errno)));
        }
        
        // Receive response
        bytesRead = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, 
                            (struct sockaddr*)&server_addr, &addrLen);
        if (bytesRead < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::cerr << "ERROR: MESSAGE LOST (TIMEOUT)" << std::endl;
            } else {
                throw std::runtime_error("Recv failed: " + std::string(strerror(errno)));
            }
            return false;
        }
        
        buffer[bytesRead] = '\0';
        std::string response(buffer);
        
        if (response == "OK") {
            std::cout << "OK" << std::endl;
            return true;
        } else {
            std::cout << "ERROR" << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return false;
    }
}

bool handleUDPBinary(int sockfd, const struct sockaddr_in& server_addr) {
    try {
        // Set timeout
        struct timeval tv;
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            throw std::runtime_error("Setsockopt failed: " + std::string(strerror(errno)));
        }
        
        // Create and send initial message
        CalcMessage initMsg;
        initMsg.type = htons(CALC_MESSAGE_TYPE);
        initMsg.message = htons(0);
        initMsg.protocol = htons(PROTOCOL_ID);
        initMsg.major_version = htons(MAJOR_VERSION);
        initMsg.minor_version = htons(MINOR_VERSION);
        
        if (sendto(sockfd, &initMsg, sizeof(initMsg), 0, 
                  (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            throw std::runtime_error("Send failed: " + std::string(strerror(errno)));
        }
        
        // Receive response
        char buffer[1024];
        socklen_t addrLen = sizeof(server_addr);
        ssize_t bytesRead = recvfrom(sockfd, buffer, sizeof(buffer), 0, 
                                    (struct sockaddr*)&server_addr, &addrLen);
        if (bytesRead < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::cerr << "ERROR: MESSAGE LOST (TIMEOUT)" << std::endl;
            } else {
                throw std::runtime_error("Recv failed: " + std::string(strerror(errno)));
            }
            return false;
        }
        
        if (bytesRead == sizeof(CalcMessage)) {
            // Check if it's an error message
            CalcMessage* msg = reinterpret_cast<CalcMessage*>(buffer);
            msg->type = ntohs(msg->type);
            msg->message = ntohs(msg->message);
            
            if (msg->type == ERROR_MESSAGE_TYPE && msg->message == 2) {
                std::cerr << "ERROR: Server does not support the protocol" << std::endl;
                return false;
            }
        } else if (bytesRead == sizeof(CalcProtocol) - sizeof(int32_t)) { // -4 because in_result is not sent by server
            // Parse the CalcProtocol message
            CalcProtocol* calcProto = reinterpret_cast<CalcProtocol*>(buffer);
            calcProto->type = ntohs(calcProto->type);
            calcProto->major_version = ntohs(calcProto->major_version);
            calcProto->minor_version = ntohs(calcProto->minor_version);
            calcProto->id = ntohl(calcProto->id);
            calcProto->arith = ntohl(calcProto->arith);
            calcProto->in_value1 = ntohl(calcProto->in_value1);
            calcProto->in_value2 = ntohl(calcProto->in_value2);
            
            if (calcProto->type != CALC_PROTOCOL_TYPE) {
                std::cerr << "ERROR: Invalid message type" << std::endl;
                return false;
            }
            
            // Calculate result
            int32_t result;
            if (calcProto->arith == 0) { // add
                result = calcProto->in_value1 + calcProto->in_value2;
            } else if (calcProto->arith == 1) { // sub
                result = calcProto->in_value1 - calcProto->in_value2;
            } else if (calcProto->arith == 2) { // mul
                result = calcProto->in_value1 * calcProto->in_value2;
            } else if (calcProto->arith == 3) { // div
                if (calcProto->in_value2 == 0) {
                    std::cerr << "ERROR: Division by zero" << std::endl;
                    return false;
                }
                result = calcProto->in_value1 / calcProto->in_value2;
            } else {
                std::cerr << "ERROR: Unknown arithmetic operation: " << calcProto->arith << std::endl;
                return false;
            }
            
            // Prepare response
            CalcProtocol responseProto;
            responseProto.type = htons(CALC_PROTOCOL_TYPE);
            responseProto.major_version = htons(MAJOR_VERSION);
            responseProto.minor_version = htons(MINOR_VERSION);
            responseProto.id = htonl(calcProto->id);
            responseProto.arith = htonl(calcProto->arith);
            responseProto.in_value1 = htonl(calcProto->in_value1);
            responseProto.in_value2 = htonl(calcProto->in_value2);
            responseProto.in_result = htonl(result);
            
            // Send response
            if (sendto(sockfd, &responseProto, sizeof(responseProto), 0, 
                      (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
                throw std::runtime_error("Send failed: " + std::string(strerror(errno)));
            }
            
            // Receive final response
            bytesRead = recvfrom(sockfd, buffer, sizeof(buffer), 0, 
                                (struct sockaddr*)&server_addr, &addrLen);
            if (bytesRead < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    std::cerr << "ERROR: MESSAGE LOST (TIMEOUT)" << std::endl;
                } else {
                    throw std::runtime_error("Recv failed: " + std::string(strerror(errno)));
                }
                return false;
            }
            
            if (bytesRead == sizeof(CalcMessage)) {
                CalcMessage* responseMsg = reinterpret_cast<CalcMessage*>(buffer);
                responseMsg->type = ntohs(responseMsg->type);
                responseMsg->message = ntohs(responseMsg->message);
                
                if (responseMsg->type == CALC_MESSAGE_TYPE && responseMsg->message == 1) {
                    std::cout << "OK" << std::endl;
                    return true;
                } else {
                    std::cout << "ERROR" << std::endl;
                    return false;
                }
            } else {
                std::cerr << "ERROR: Invalid response size" << std::endl;
                return false;
            }
        } else {
            std::cerr << "ERROR: Invalid message size" << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return false;
    }
    
    return false;
}