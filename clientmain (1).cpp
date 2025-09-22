#include <signal.h>
#include <iostream>
#include <string>
#include <sstream>
#include <Helpers/Misc.hpp>
#include <Helpers/Socket.hpp>
#include <Helpers/AddrInfo.hpp>
#include <Helpers/Tokenizer.hpp>

/* You will to add includes here */

// Enable if you want debugging to be printed, see examble below.
// Alternative, pass CFLAGS=-DDEBUG to make, make CFLAGS=-DDEBUG
//#define DEBUG


// Included to get the support library

int main(const int argc, char* argv[])
{
    // Ignore SIGPIPE because write()/read() are essentially equivalent to send()/recv()

    // Check whether an argument has been passed
    if (argc < 2)
    {
        std::stringstream ss("Usage: %s protocol://server:port/path. ");
        ss << argv[0] << "\n";
        throw std::runtime_error(ss.str());
    }

    // Getting the argument
    const std::string input = argv[1];

    // Checking whether a triple slash has been
    if (input.find("///") != std::string::npos)
    {
        std::stringstream ss("Invalid format: ");
        ss << input << "\n";
        throw std::runtime_error(ss.str());
    }

    // Tokenize Input
    Helper::Tokenizer tokenizer;
    Helper::TokenizerData ipData = tokenizer.Tokenize(input);

    std::cout << "Host " << ipData.Hostname << ", and port " << ipData.Port << "\n";

    // Check whether port is valid
    Helper::Misc::CheckPortValidity(std::stoi(ipData.Port));

    // Setup socket hints
    addrinfo socketHints{};
    socketHints.ai_family = AF_UNSPEC;

    if (ipData.Protocol == "UDP" || ipData.Protocol == "udp")
    {
        socketHints.ai_socktype = SOCK_DGRAM;
    }

    if (ipData.Protocol == "TCP" || ipData.Protocol == "tcp")
    {
        socketHints.ai_socktype = SOCK_STREAM;
    }

    // Establish server information
    Helper::AddrInfo serverInformation(ipData.Hostname, ipData.Port, socketHints);

    // Create Socket
    Helper::Socket socket = Helper::Misc::CreateSocket(serverInformation);

    if (ipData.Path == "BINARY" || ipData.Path == "binary")
    {
        Helper::Misc::PerformBinaryCommunication(socket);
    }

    if (ipData.Path == "TEXT" || ipData.Path == "text")
    {
        Helper::Misc::PerformTextCommunication(socket);
    }

    exit(EXIT_SUCCESS);

#ifdef DEBUG
    std::cout
            << "Protocol: " << ipData.Protocol
            << " Host: " << ipData.Hostname
            << " Port: " << ipData.Port
            << " Path: " << ipData.Path;
#endif
}
