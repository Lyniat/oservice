#include "utility.h"
#include "komihash.h"

#include <string>
#include <vector>

#if PLATFORM_LINUX || PLATFORM_MACOS
#include <arpa/inet.h>
#include <ifaddrs.h>
#endif

#if PLATFORM_WINDOWS
#include <ObjectArray.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#endif

constexpr const char* SALT = "uCxxvgcq9ibAP85bsq8zn1N73VvcKnRj";
constexpr uint64_t SEED = 1460381799380413747;

std::string get_local_user_name() {
    #if PLATFORM_MACOS
    return getenv("USER");
    #elif PLATFORM_LINUX
    const char *user = getenv("LOGNAME");
    if (user == nullptr) user = getenv("USER");
    if (user == nullptr) user = getenv("USERNAME");
    if (user == nullptr) user = "<unknown>";
    return std::string{user};
    #else
    char username[UNLEN + 1];
    DWORD username_len = UNLEN + 1;
    GetUserName(username, &username_len);
    return std::string(username, username_len);
    #endif
}

#if PLATFORM_MACOS
// see: https://stackoverflow.com/questions/8753171/using-iokit-to-return-macs-serial-number-returns-4-extra-characters
#include <IOKit/IOKitLib.h>

uint64_t get_local_system_hash() {
    CFMutableDictionaryRef matching = IOServiceMatching("IOPlatformExpertDevice");
    io_service_t service = IOServiceGetMatchingService(kIOMasterPortDefault, matching);
    CFStringRef serialNumber = (CFStringRef)IORegistryEntryCreateCFProperty(service,
                                                                            CFSTR("IOPlatformSerialNumber"),
                                                                            kCFAllocatorDefault, 0);
    const char* str = CFStringGetCStringPtr(serialNumber, kCFStringEncodingMacRoman);
    IOObjectRelease(service);
    auto to_hash = get_local_user_name() + str + SALT;
    uint64_t hash = komihash(to_hash.c_str(), to_hash.size(), SEED);
    return hash;
}
#endif

#if PLATFORM_WINDOWS
// see: https://learn.microsoft.com/de-de/windows/win32/api/iphlpapi/nf-iphlpapi-getadaptersinfo
uint64_t get_local_system_hash() {
    PIP_ADAPTER_INFO pAdapterInfo;
    PIP_ADAPTER_INFO pAdapter = nullptr;
    uint64_t mac = 0;

    ULONG ulOutBufLen = sizeof (IP_ADAPTER_INFO);
    pAdapterInfo = (IP_ADAPTER_INFO*)malloc(sizeof (IP_ADAPTER_INFO));
    if (pAdapterInfo == nullptr) {
        printf("Error allocating memory needed to call GetAdaptersinfo\n");
        return 1;
    }

    if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW) {
        free(pAdapterInfo);
        pAdapterInfo = (IP_ADAPTER_INFO*)malloc(ulOutBufLen);
        if (pAdapterInfo == nullptr) {
            printf("Error allocating memory needed to call GetAdaptersinfo\n");
            return 1;
        }
    }

    if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == NO_ERROR) {
        pAdapter = pAdapterInfo;
        for (int i = 0; (i < pAdapter->AddressLength && i < 4); i++) {
            mac |= (pAdapter->Address[i] << (i * 8));
        }
    } else {
        printf("Error finding adapter info.\n");
        return 1;
    }
    if (pAdapterInfo) {
        free(pAdapterInfo);
    }

    auto to_hash = get_local_user_name() + std::to_string(mac) + SALT;
    uint64_t hash = komihash(to_hash.c_str(), to_hash.size(), SEED);
    return hash;
}
#endif

#if PLATFORM_LINUX
uint64_t get_local_system_hash() {
    return 0; //TODO: return real hash here
}
#endif

#if PLATFORM_WINDOWS
std::string get_local_network_ipv4() {
    ULONG size = 0;
    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;

    ULONG ret = GetAdaptersAddresses(AF_INET, flags, nullptr, nullptr, &size);

    // we want ERROR_BUFFER_OVERFLOW since we pass nullptr
    if (ret != ERROR_BUFFER_OVERFLOW || size == 0) {
        return "";
    }

    std::vector<unsigned char> buffer(size);
    auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());

    ret = GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &size);
    if (ret != NO_ERROR) {
        return "";
    }

    for (auto* a = adapters; a; a = a->Next) {
        // only active adapters
        if (a->OperStatus != IfOperStatusUp) {
            continue;
        }

        for (auto* ua = a->FirstUnicastAddress; ua; ua = ua->Next) {
            if (!ua->Address.lpSockaddr) {
                continue;
            }

            auto* sa = reinterpret_cast<sockaddr_in*>(ua->Address.lpSockaddr);
            if (sa->sin_addr.s_addr == htonl(INADDR_LOOPBACK)) {
                continue;
            }

            char buf[INET_ADDRSTRLEN] = {};
            auto success = inet_ntop(AF_INET, &sa->sin_addr, buf, sizeof(buf));
            if (success == nullptr) {
                continue;
            }

            return std::string(buf);
        }
    }

    return "";
}
#endif

#if PLATFORM_LINUX || PLATFORM_MACOS
std::string get_local_network_ipv4() {
    ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) != 0 || ifaddr == nullptr) {
        return "";
    }

    // AF_INET6 would be ipv6
    for (auto* ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        auto* addr = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
        if (addr->sin_addr.s_addr == htonl(INADDR_LOOPBACK)) {
            continue;
        }

        char buf[INET_ADDRSTRLEN];
        auto success = inet_ntop(AF_INET, &addr->sin_addr, buf, sizeof(buf));

        if (success == nullptr) {
            continue;
        }
        freeifaddrs(ifaddr);
        return buf;
    }

    freeifaddrs(ifaddr);
    return "";
}
#endif