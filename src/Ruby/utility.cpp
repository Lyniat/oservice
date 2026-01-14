#include "utility.h"
#include "komihash.h"

#include <string>

#include <assert.h>
#include <vector>

#ifdef PLATFORM_WINDOWS
#include <ObjectArray.h>
#include <IPTypes.h>
#include <iphlpapi.h>
#endif

constexpr const char* SALT = "uCxxvgcq9ibAP85bsq8zn1N73VvcKnRj";
constexpr uint64_t SEED = 1460381799380413747;

std::string get_local_user_name() {
    #if PLATFORM_MACOS
    return getenv("USER");
    #elif PLATFORM_LINUX
    return getenv("USERNAME");
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