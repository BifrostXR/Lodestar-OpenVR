#pragma once

#include <driverlog.h>

#include <windows.h>
#include <hidsdi.h>
#include <setupapi.h>

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>

#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")

// Device constants 
namespace HID
{
    constexpr DWORD  VID = 0x1915; // Nordic Semiconductor
    constexpr DWORD  PID = 0x1101; // Lodestar Hub
    constexpr USHORT USAGE_PAGE = 0xFF01; // Lodestar Tracker Page
    constexpr USHORT USAGE = 0x0001; // Lodestar Tracker Usage (Input)

    enum class Status
    {
        Null = 0x00,
        Off,
        Pairing,
        Ready,
        Active,
        Tracking,
        Error,
        Size
    };

#pragma pack(push,1)
    struct Report
    {
        uint8_t device_id;
        uint8_t device_status;
        uint8_t device_battery;

        uint8_t pose_confidence;
        float   pose_position[3];
        float   pose_orientation[4];
        float   pose_linearVelocity[3];
        float   pose_angularVelocity[3];
    };
#pragma pack(pop)
}

// Utility constants
constexpr std::chrono::seconds CONN_CHECK_INTERVAL(1); // re-scan every 1s
constexpr DWORD IDLE_WAIT_MS = 200; // thread sleep when idle

class HIDManager
{
public:
    HIDManager();
    ~HIDManager();

    bool IsConnected() const { return !m_interfaces.empty(); }
    size_t InterfaceCount() const { return  m_interfaces.size(); }

    // Retrieve a snapshot of all reports (thread-safe shallow copy)
    std::vector<std::pair<std::wstring, std::vector<uint8_t>>> GetReportBytes() const;
    std::vector<std::pair<std::wstring, HID::Report>> GetReports() const;

private:
    struct InterfaceHandle
    {
        HANDLE handle = INVALID_HANDLE_VALUE;
        HIDP_CAPS caps = {};
        std::wstring path;

        HID::Report report{}; // Last payload
        OVERLAPPED ov{}; // Permanent OVERLAPPED
        std::vector<UCHAR> buffer; // Input buffer
    };

    // Enumeration & hot-plug helpers
    bool  OpenAll(); // Append new interfaces
    void  CloseAll(); // Close & clear vector

    static bool  IsDesiredDevice(const HIDD_ATTRIBUTES& attr);
    static bool  IsDesiredInterface(HANDLE h, HIDP_CAPS& outCaps);

    // Async I/O helpers
    bool  BeginAsyncRead(InterfaceHandle& iface);

    // Monitor thread
    void  MonitorLoop();

    std::vector<InterfaceHandle> m_interfaces;

    std::thread m_monitorThread;
    std::atomic<bool> m_stopThread{ false };
};