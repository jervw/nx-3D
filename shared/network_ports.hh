#ifndef NX3D_SHARED_NETWORK_PORTS_HH
#define NX3D_SHARED_NETWORK_PORTS_HH

#include <charconv>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace nx3d
{

inline constexpr uint16_t kDefaultTCPPort = 54200;
inline constexpr uint16_t kDefaultUDPPort = 54201;
inline constexpr uint16_t kDefaultTCPSessionPort = 54202;

struct NetworkPorts
{
    uint16_t tcp = kDefaultTCPPort;
    uint16_t udp = kDefaultUDPPort;
    uint16_t tcpSession = kDefaultTCPSessionPort;
};

inline uint16_t parsePort(std::string_view option, std::string_view value)
{
    unsigned int parsed = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || parsed == 0 || parsed > 65535)
    {
        throw std::invalid_argument(std::string(option) + " must be a port number from 1 to 65535");
    }
    return static_cast<uint16_t>(parsed);
}

inline bool isOption(std::string_view argument, std::string_view name)
{
    return argument == name || (argument.size() == name.size() + 1 && argument[0] == '-' && argument.substr(1) == name);
}

/// Parse nx-3D's port options while leaving unrelated program options alone.
/// Both the existing single-dash style and conventional double-dash style are
/// accepted. Later occurrences replace earlier ones.
inline NetworkPorts parseNetworkPorts(int argc, char* const argv[])
{
    NetworkPorts ports;

    for (int i = 1; i < argc; ++i)
    {
        const std::string_view argument(argv[i]);
        uint16_t* destination = nullptr;

        if (isOption(argument, "-tcp-port"))
            destination = &ports.tcp;
        else if (isOption(argument, "-udp-port"))
            destination = &ports.udp;
        else if (isOption(argument, "-tcp-session-port"))
            destination = &ports.tcpSession;

        if (!destination)
            continue;
        if (i + 1 >= argc)
            throw std::invalid_argument(std::string(argument) + " requires a value");

        *destination = parsePort(argument, argv[++i]);
    }

    return ports;
}

} // namespace nx3d

#endif
