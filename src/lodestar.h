#pragma once

#include "openvr_driver.h"
#include "hid_manager.h"
#include "vrmath.h"

static const char* modelPath = "{lodestar}/rendermodels/lodestar_puck";

class LodestarTracker : public vr::ITrackedDeviceServerDriver
{
public:
    LodestarTracker(std::wstring devicePath);

    // Inherited via ITrackedDeviceServerDriver
    virtual vr::EVRInitError Activate(uint32_t unObjectId) override;
    virtual void Deactivate() override;
    virtual void EnterStandby() override;
    virtual void* GetComponent(const char* pchComponentNameAndVersion) override;
    virtual void DebugRequest(const char* pchRequest, char* pchResponseBuffer, uint32_t unResponseBufferSize) override;
    virtual vr::DriverPose_t GetPose() override;

    const std::string& GetSerialNumber();
    const std::wstring& GetPath();

    void UpdateDevice(HID::Report report);

private:
    vr::TrackedDeviceIndex_t m_deviceID;
    std::string m_deviceSerialNumber = "100";
    std::wstring m_devicePath;

    vr::PropertyContainerHandle_t m_properties;
    vr::DriverPose_t m_pose = { 0 };

    bool m_activated = false;
};