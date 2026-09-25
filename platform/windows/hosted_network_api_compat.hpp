#pragma once

#include <wlanapi.h>

#if !defined(__MINGW32__)
using rauch_wlan_property_type = WLAN_OPCODE_VALUE_TYPE;
#endif

// Older MinGW-w64 SDK headers omit Microsoft's legacy Hosted Network declarations.
#if defined(__MINGW32__)
using rauch_wlan_property_type = WLAN_HOSTED_NETWORK_OPCODE;
typedef enum _WLAN_HOSTED_NETWORK_STATE
{
    wlan_hosted_network_unavailable = 0,
    wlan_hosted_network_idle,
    wlan_hosted_network_active
} WLAN_HOSTED_NETWORK_STATE,
    *PWLAN_HOSTED_NETWORK_STATE;

typedef enum _WLAN_HOSTED_NETWORK_OPCODE
{
    wlan_hosted_network_opcode_connection_settings = 0,
    wlan_hosted_network_opcode_security_settings,
    wlan_hosted_network_opcode_station_profile,
    wlan_hosted_network_opcode_enable
} WLAN_HOSTED_NETWORK_OPCODE,
    *PWLAN_HOSTED_NETWORK_OPCODE;

typedef DWORD                        WLAN_HOSTED_NETWORK_REASON;
typedef WLAN_HOSTED_NETWORK_REASON*  PWLAN_HOSTED_NETWORK_REASON;
constexpr WLAN_HOSTED_NETWORK_REASON wlan_hosted_network_reason_success = 0;

typedef struct _WLAN_HOSTED_NETWORK_CONNECTION_SETTINGS
{
    DOT11_SSID hostedNetworkSSID;
    DWORD      dwMaxNumberOfPeers;
} WLAN_HOSTED_NETWORK_CONNECTION_SETTINGS, *PWLAN_HOSTED_NETWORK_CONNECTION_SETTINGS;

typedef struct _WLAN_HOSTED_NETWORK_STATUS
{
    GUID                      IPDeviceID;
    DOT11_MAC_ADDRESS         wlanHostedNetworkBSSID;
    DOT11_PHY_TYPE            wlanHostedNetworkPhyType;
    WLAN_HOSTED_NETWORK_STATE hostedNetworkState;
} WLAN_HOSTED_NETWORK_STATUS, *PWLAN_HOSTED_NETWORK_STATUS;

extern "C"
{
    DWORD WINAPI WlanHostedNetworkInitSettings(HANDLE, PWLAN_HOSTED_NETWORK_REASON, PVOID);
    DWORD WINAPI WlanHostedNetworkQueryProperty(HANDLE, WLAN_HOSTED_NETWORK_OPCODE, PDWORD, PVOID*,
                                                PWLAN_HOSTED_NETWORK_OPCODE, PVOID);
    DWORD WINAPI WlanHostedNetworkSetProperty(HANDLE, WLAN_HOSTED_NETWORK_OPCODE, DWORD, PVOID,
                                              PWLAN_HOSTED_NETWORK_REASON, PVOID);
    DWORD WINAPI WlanHostedNetworkSetSecondaryKey(HANDLE, DWORD, PUCHAR, BOOL, BOOL, PWLAN_HOSTED_NETWORK_REASON,
                                                  PVOID);
    DWORD WINAPI WlanHostedNetworkQueryStatus(HANDLE, PWLAN_HOSTED_NETWORK_STATUS*, PVOID);
    DWORD WINAPI WlanHostedNetworkStartUsing(HANDLE, PWLAN_HOSTED_NETWORK_REASON, PVOID);
    DWORD WINAPI WlanHostedNetworkStopUsing(HANDLE, PWLAN_HOSTED_NETWORK_REASON, PVOID);
}
#endif
