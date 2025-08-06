#pragma once

#include "openvr_driver.h"
#include "hid_manager.h"
#include "lodestar.h"
#include <vector>
#include <unordered_map>
#include <algorithm>

class DeviceProvider : public vr::IServerTrackedDeviceProvider
{
public:
    // Inherited via IServerTrackedDeviceProvider
    vr::EVRInitError Init(vr::IVRDriverContext* pDriverContext) override;
    void Cleanup() override;
    const char* const* GetInterfaceVersions() override;
    void RunFrame() override;
    bool ShouldBlockStandbyMode() override;
    void EnterStandby() override;
    void LeaveStandby() override;

    void AddTracker(std::wstring devicePath);
    void UpdateTrackers();

private:
    HIDManager* m_hid = nullptr;
    std::vector<std::unique_ptr<LodestarTracker>> m_trackers;
};