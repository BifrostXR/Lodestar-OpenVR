#include "hid_manager.h"

namespace detail
{
    template<typename T>
    using unique_ppd = std::unique_ptr<T, decltype(&::HidD_FreePreparsedData)>;
}

// Constructor
HIDManager::HIDManager()
{
    m_monitorThread = std::thread(&HIDManager::MonitorLoop, this);
}

// Destructor
HIDManager::~HIDManager()
{
    m_stopThread = true;

    if (m_monitorThread.joinable())
        m_monitorThread.join();

    CloseAll();
}

// Static helper predicates
bool HIDManager::IsDesiredDevice(const HIDD_ATTRIBUTES& attr)
{
    return attr.VendorID == HID::VID &&
        attr.ProductID == HID::PID;
}

bool HIDManager::IsDesiredInterface(HANDLE h, HIDP_CAPS& outCaps)
{
    PHIDP_PREPARSED_DATA ppd = nullptr;

    if (!::HidD_GetPreparsedData(h, &ppd))
        return false;

    detail::unique_ppd< _HIDP_PREPARSED_DATA > guard(ppd, &::HidD_FreePreparsedData);

    if (::HidP_GetCaps(ppd, &outCaps) != HIDP_STATUS_SUCCESS)
        return false;

    return outCaps.UsagePage == HID::USAGE_PAGE &&
        outCaps.Usage == HID::USAGE;
}

// Async read helper
bool HIDManager::BeginAsyncRead(InterfaceHandle& iface)
{
    if (iface.handle == INVALID_HANDLE_VALUE)
        return false;

    if (iface.buffer.empty())
        iface.buffer.resize(iface.caps.InputReportByteLength);

    if (!iface.ov.hEvent)
        iface.ov.hEvent = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);

    ::ResetEvent(iface.ov.hEvent);

    DWORD dummy = 0;
    BOOL ok = ::ReadFile(iface.handle,
        iface.buffer.data(),
        static_cast<DWORD>(iface.buffer.size()),
        &dummy, &iface.ov);

    return ok || (::GetLastError() == ERROR_IO_PENDING);
}

// Enumerate & open new interfaces
bool HIDManager::OpenAll()
{
    GUID hidGuid;
    ::HidD_GetHidGuid(&hidGuid);

    HDEVINFO hDevInfo = ::SetupDiGetClassDevs(&hidGuid, nullptr, nullptr,
        DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
    if (hDevInfo == INVALID_HANDLE_VALUE)
        return false;

    SP_DEVICE_INTERFACE_DATA ifData{};
    ifData.cbSize = sizeof(ifData);

    DWORD index = 0;
    while (::SetupDiEnumDeviceInterfaces(hDevInfo, nullptr, &hidGuid, index++, &ifData))
    {
        DWORD pathLen = 0;
        ::SetupDiGetDeviceInterfaceDetail(hDevInfo, &ifData, nullptr, 0, &pathLen, nullptr);
        if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            continue;

        std::vector<BYTE> buffer(pathLen, 0);
        auto detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA*>(buffer.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        if (!::SetupDiGetDeviceInterfaceDetail(hDevInfo, &ifData, detail, pathLen, nullptr, nullptr))
            continue;

        // Skip if we already have this path
        bool known = false;
        for (auto& ih : m_interfaces)
        {
            if (!_wcsicmp(ih.path.c_str(), detail->DevicePath))
            {
                known = true;
                break;
            }
        }
        if (known)
            continue;

        HANDLE h = ::CreateFile(detail->DevicePath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED, nullptr);

        if (h == INVALID_HANDLE_VALUE)
            continue;

        HIDD_ATTRIBUTES attr{}; attr.Size = sizeof(attr);
        if (!::HidD_GetAttributes(h, &attr) || !IsDesiredDevice(attr))
        {
            ::CloseHandle(h);
            continue;
        }

        HIDP_CAPS caps{};
        if (!IsDesiredInterface(h, caps))
        {
            ::CloseHandle(h);
            continue;
        }

        InterfaceHandle ih{};
        ih.handle = h;
        ih.caps = caps;
        ih.path = detail->DevicePath;

        m_interfaces.push_back(std::move(ih));
        BeginAsyncRead(m_interfaces.back());
    }

    ::SetupDiDestroyDeviceInfoList(hDevInfo);
    return !m_interfaces.empty();
}

// Close everything
void HIDManager::CloseAll()
{
    for (auto& ih : m_interfaces)
    {
        if (ih.handle != INVALID_HANDLE_VALUE)
            ::CloseHandle(ih.handle);

        if (ih.ov.hEvent)
            ::CloseHandle(ih.ov.hEvent);
    }

    m_interfaces.clear();
}

// Public accessor
std::vector<std::pair<std::wstring, std::vector<uint8_t>>>
HIDManager::GetReportBytes() const
{
    std::vector<std::pair<std::wstring, std::vector<uint8_t>>> out;
    out.reserve(m_interfaces.size());

    for (auto const& ih : m_interfaces)
    {
        std::vector<uint8_t> payload(sizeof(HID::Report));
        std::memcpy(payload.data(), &ih.report, sizeof(HID::Report));
        out.emplace_back(ih.path, std::move(payload));
    }

    return out;
}

std::vector<std::pair<std::wstring, HID::Report>> HIDManager::GetReports() const
{
    std::vector<std::pair<std::wstring, HID::Report>> out;
    out.reserve(m_interfaces.size());

    for (auto const& ih : m_interfaces)
        out.emplace_back(ih.path, ih.report);

    return out;
}

// Main monitor thread
void HIDManager::MonitorLoop()
{
    auto lastScan = std::chrono::steady_clock::now();
    OpenAll(); // initial enumeration

    while (!m_stopThread)
    {
        // build vector of events
        std::vector<HANDLE> events;
        events.reserve(m_interfaces.size());

        for (auto& ih : m_interfaces)
            events.push_back(ih.ov.hEvent);

        DWORD waitRes = events.empty() ? WAIT_TIMEOUT
            : ::WaitForMultipleObjects(
                static_cast<DWORD>(events.size()),
                events.data(),
                FALSE, // any one
                IDLE_WAIT_MS);

        // A report arrived
        if (waitRes >= WAIT_OBJECT_0 &&
            waitRes < WAIT_OBJECT_0 + events.size())
        {
            size_t idx = waitRes - WAIT_OBJECT_0;
            InterfaceHandle& ih = m_interfaces[idx];

            DWORD bytes = 0;

            if (::GetOverlappedResult(ih.handle, &ih.ov, &bytes, FALSE) &&
                bytes >= 1 + sizeof(HID::Report))
            {
                std::memcpy(&ih.report,
                    ih.buffer.data() + 1, // skip Report-ID
                    sizeof(HID::Report));

                BeginAsyncRead(ih); // queue next read
            }
            else
            {   // Permanent I/O error
                ::CancelIoEx(ih.handle, nullptr);
                ::CloseHandle(ih.handle);
                ::CloseHandle(ih.ov.hEvent);
                m_interfaces.erase(m_interfaces.begin() + idx);
            }
        }

        // Periodic re-enumeration
        if (std::chrono::steady_clock::now() - lastScan >= CONN_CHECK_INTERVAL)
        {
            // validate still-open handles
            auto it = m_interfaces.begin();
            while (it != m_interfaces.end())
            {
                HIDD_ATTRIBUTES attr{}; attr.Size = sizeof(attr);
                if (!::HidD_GetAttributes(it->handle, &attr) || !IsDesiredDevice(attr))
                {
                    ::CancelIoEx(it->handle, nullptr);
                    ::CloseHandle(it->handle);
                    ::CloseHandle(it->ov.hEvent);
                    it = m_interfaces.erase(it);
                }
                else ++it;
            }

            // Open any newcomers
            OpenAll();
            lastScan = std::chrono::steady_clock::now();
        }
    }
}