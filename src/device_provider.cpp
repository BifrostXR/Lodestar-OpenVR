#include "device_provider.h"

vr::EVRInitError DeviceProvider::Init(vr::IVRDriverContext* pDriverContext)
{
    VR_INIT_SERVER_DRIVER_CONTEXT(pDriverContext);

    vr::VRDriverLog()->Log("Driver Loaded");

    m_hid = new HIDManager();

    return vr::VRInitError_None;
}

void DeviceProvider::Cleanup()
{
    if(m_hid != nullptr)
        m_hid->~HIDManager();

    vr::VRDriverLog()->Log("HID Manager Ended");
    
    VR_CLEANUP_SERVER_DRIVER_CONTEXT();
}

const char* const* DeviceProvider::GetInterfaceVersions()
{
    return vr::k_InterfaceVersions;
}

void DeviceProvider::RunFrame()
{
    if (m_hid == nullptr)
        return;

    UpdateTrackers();
}

bool DeviceProvider::ShouldBlockStandbyMode()
{
    return true;
}

void DeviceProvider::EnterStandby()
{

}

void DeviceProvider::LeaveStandby()
{

}

void DeviceProvider::AddTracker(std::wstring devicePath)
{
    m_trackers.emplace_back(std::make_unique<LodestarTracker>(devicePath));
    std::unique_ptr<LodestarTracker>& tracker = m_trackers.back();

    vr::VRServerDriverHost()->TrackedDeviceAdded(std::to_string(100 + m_trackers.size()).c_str(),
        vr::TrackedDeviceClass_GenericTracker,
        tracker.get());
}

void DeviceProvider::UpdateTrackers()
{
    std::vector<std::pair<std::wstring, HID::Report>> reports;

    if (m_hid != nullptr)
        reports = m_hid->GetReports();

    std::unordered_map<std::wstring, HID::Report> reportMap;
    reportMap.reserve(reports.size());

    for (auto const& pr : reports)
        reportMap.emplace(pr.first, pr.second);

    for (auto const& pr : reports)
    {
        const auto& path = pr.first;
        const auto& report = pr.second;

        // Ignore unconnected devices
        if (static_cast<HID::Status>(report.device_status) <= HID::Status::Off)
            continue;

        // Already have it?
        bool known = std::any_of(
            m_trackers.begin(), m_trackers.end(),
            [&](auto const& t) { return t->GetPath() == path; });

        if (!known)
            AddTracker(path);
    }

    HID::Report zeroReport{};
    std::memset(&zeroReport, 0, sizeof(zeroReport));

    for (auto& trackerPtr : m_trackers)
    {
        const std::wstring& path = trackerPtr->GetPath();
        auto it = reportMap.find(path);

        if (it != reportMap.end())
            trackerPtr->UpdateDevice(it->second); // Send data
    }

}
