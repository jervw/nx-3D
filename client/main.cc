#include <client/app/client_app.hh>

#include <nexilis/cmd_line_options.hh>
#include <nexilis/ports.hh>

#include <shared/network_ports.hh>

#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    try
    {
        const auto ports = nx3d::parseNetworkPorts(argc, argv);
        nexilis::Ports::setBoostTCPPort(ports.tcp);
        nexilis::Ports::setBoostUDPPort(ports.udp);
    }
    catch (const std::invalid_argument& error)
    {
        std::cerr << "Invalid network port: " << error.what() << std::endl;
        return 1;
    }

    // Parse --server and --username arguments
    nexilis::CmdLineOptions cmdLine(argc, argv);
    std::string serverAddress = cmdLine.getValue<std::string>("-server", "127.0.0.1");
    std::string username = cmdLine.getValue<std::string>("-username", "");

    nx3d::client::ClientApp app(serverAddress, username, argc > 1);
    app.run();

    return 0;
}
