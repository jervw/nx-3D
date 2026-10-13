#include <shared/network_ports.hh>

#include <cstddef>
#include <stdexcept>

namespace
{

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

template <std::size_t Size>
nx3d::NetworkPorts parse(char* (&arguments)[Size])
{
    return nx3d::parseNetworkPorts(static_cast<int>(Size), arguments);
}

} // namespace

int main()
{
    char program[] = "nx3d";

    {
        char* arguments[] = {program};
        const auto ports = parse(arguments);
        require(ports.tcp == 54200, "Default TCP authentication port changed");
        require(ports.udp == 54201, "Default UDP port changed");
        require(ports.tcpSession == 54202, "Default TCP session port changed");
    }

    {
        char tcpOption[] = "--tcp-port";
        char tcpValue[] = "55200";
        char udpOption[] = "--udp-port";
        char udpValue[] = "55201";
        char sessionOption[] = "--tcp-session-port";
        char sessionValue[] = "55202";
        char* arguments[] = {
                program,
                tcpOption, tcpValue,
                udpOption, udpValue,
                sessionOption, sessionValue};
        const auto ports = parse(arguments);
        require(ports.tcp == 55200, "TCP authentication port was not parsed");
        require(ports.udp == 55201, "UDP port was not parsed");
        require(ports.tcpSession == 55202, "TCP session port was not parsed");
    }

    {
        char serverOption[] = "-server";
        char serverValue[] = "game.example.com";
        char tcpOption[] = "-tcp-port";
        char tcpValue[] = "60000";
        char* arguments[] = {program, serverOption, serverValue, tcpOption, tcpValue};
        const auto ports = parse(arguments);
        require(ports.tcp == 60000, "Single-dash TCP option was not parsed");
        require(ports.udp == 54201, "Unspecified UDP port did not keep its default");
        require(ports.tcpSession == 54202, "Unspecified session port did not keep its default");
    }

    {
        char tcpOption[] = "--tcp-port";
        char invalidValue[] = "65536";
        char* arguments[] = {program, tcpOption, invalidValue};
        bool rejected = false;
        try
        {
            parse(arguments);
        }
        catch (const std::invalid_argument&)
        {
            rejected = true;
        }
        require(rejected, "Out-of-range port was accepted");
    }

    {
        char udpOption[] = "--udp-port";
        char* arguments[] = {program, udpOption};
        bool rejected = false;
        try
        {
            parse(arguments);
        }
        catch (const std::invalid_argument&)
        {
            rejected = true;
        }
        require(rejected, "Port option without a value was accepted");
    }
}
