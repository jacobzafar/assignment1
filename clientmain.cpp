int main(const int argc, char* argv[])
{
    // Kontroll: argument
    if (argc < 2)
    {
        std::stringstream ss("Usage: %s protocol://server:port/path. ");
        ss << argv[0] << "\n";
        throw std::runtime_error(ss.str());
    }

    // Hämta argumentet
    const std::string input = argv[1];

    // Kolla om triple slash finns
    if (input.find("///") != std::string::npos)
    {
        std::stringstream ss("Invalid format: ");
        ss << input << "\n";
        throw std::runtime_error(ss.str());
    }

    // Tokenize input
    Helper::Tokenizer tokenizer;
    Helper::TokenizerData ipData = tokenizer.Tokenize(input);

    std::cout << "Host " << ipData.Hostname << ", and port " << ipData.Port << "\n";

    // Kontrollera port
    Helper::Misc::CheckPortValidity(std::stoi(ipData.Port));

    // Socket hints
    addrinfo socketHints{};
    socketHints.ai_family = AF_UNSPEC;

    // --- UDP ---
    if (ipData.Protocol == "UDP" || ipData.Protocol == "udp")
    {
        socketHints.ai_socktype = SOCK_DGRAM;
    }
    // --- TCP ---
    else if (ipData.Protocol == "TCP" || ipData.Protocol == "tcp")
    {
        socketHints.ai_socktype = SOCK_STREAM;
    }
    // --- ANY ---
    else if (ipData.Protocol == "ANY" || ipData.Protocol == "any")
    {
        std::cout << "Protocol 'any' explicitly supported.\n";
        std::cout << "Trying TCP first...\n";

        socketHints.ai_socktype = SOCK_STREAM;
        try
        {
            Helper::AddrInfo serverInformation(ipData.Hostname, ipData.Port, socketHints);
            Helper::Socket socket = Helper::Misc::CreateSocket(serverInformation);

            if (ipData.Path == "BINARY" || ipData.Path == "binary")
                Helper::Misc::PerformBinaryCommunication(socket);
            else if (ipData.Path == "TEXT" || ipData.Path == "text")
                Helper::Misc::PerformTextCommunication(socket);

            exit(EXIT_SUCCESS);
        }
        catch (const std::exception& e)
        {
            std::cerr << "TCP failed: " << e.what() << "\n";
            std::cout << "Trying UDP...\n";

            socketHints.ai_socktype = SOCK_DGRAM;
            try
            {
                Helper::AddrInfo serverInformation(ipData.Hostname, ipData.Port, socketHints);
                Helper::Socket socket = Helper::Misc::CreateSocket(serverInformation);

                if (ipData.Path == "BINARY" || ipData.Path == "binary")
                    Helper::Misc::PerformBinaryCommunication(socket);
                else if (ipData.Path == "TEXT" || ipData.Path == "text")
                    Helper::Misc::PerformTextCommunication(socket);

                exit(EXIT_SUCCESS);
            }
            catch (const std::exception& e2)
            {
                std::cerr << "UDP failed: " << e2.what() << "\n";
                std::cerr << "Error: BOTH TCP and UDP failed under 'any'.\n";
                exit(EXIT_FAILURE);
            }
        }
    }

    // --- Okänt protokoll ---
    else
    {
        std::stringstream ss;
        ss << "Error: Unknown or unsupported protocol: " << ipData.Protocol << "\n";
        throw std::runtime_error(ss.str());
    }

    // Endast TCP/UDP-fallen når hit
    Helper::AddrInfo serverInformation(ipData.Hostname, ipData.Port, socketHints);
    Helper::Socket socket = Helper::Misc::CreateSocket(serverInformation);

    if (ipData.Path == "BINARY" || ipData.Path == "binary")
    {
        Helper::Misc::PerformBinaryCommunication(socket);
    }
    else if (ipData.Path == "TEXT" || ipData.Path == "text")
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
