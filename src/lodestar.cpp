#include "lodestar.h"

LodestarTracker::LodestarTracker(std::wstring devicePath) : 
    m_deviceID(vr::k_unTrackedDeviceIndexInvalid), m_devicePath(devicePath) {};

vr::EVRInitError LodestarTracker::Activate(uint32_t unObjectId)
{
    vr::VRDriverLog()->Log("Tracker Activated");

    m_deviceID = unObjectId;

    m_properties = vr::VRProperties()->TrackedDeviceToPropertyContainer(m_deviceID);
    vr::VRProperties()->SetStringProperty(m_properties, vr::Prop_RenderModelName_String, modelPath);

    m_activated = true;

    return vr::VRInitError_None;
}

void LodestarTracker::Deactivate()
{
}

void LodestarTracker::EnterStandby()
{
}

void* LodestarTracker::GetComponent(const char* pchComponentNameAndVersion)
{
    return nullptr;
}

void LodestarTracker::DebugRequest(const char* pchRequest, char* pchResponseBuffer, uint32_t unResponseBufferSize)
{
    if (unResponseBufferSize >= 1)
        pchResponseBuffer[0] = 0;
}

vr::DriverPose_t LodestarTracker::GetPose()
{
    return m_pose;
}

const std::string& LodestarTracker::GetSerialNumber()
{
    return m_deviceSerialNumber;
}

const std::wstring& LodestarTracker::GetPath()
{
    return m_devicePath;
}

void LodestarTracker::UpdateDevice(HID::Report report)
{
    if (!m_activated)
        return;
    
    HID::Status status = static_cast<HID::Status>(report.device_status);
    
    switch (status)
    {
        case HID::Status::Active:
            m_pose.result = vr::TrackingResult_Calibrating_InProgress;

        case HID::Status::Tracking:
        {
            if(status == HID::Status::Tracking)
                m_pose.result = vr::TrackingResult_Running_OK;
                
            m_pose.deviceIsConnected = true;
            m_pose.poseIsValid = true;

            // Transformation
            m_pose.qWorldFromDriverRotation.w = 1.f;
            m_pose.qDriverFromHeadRotation.w = 1.f;

/*
            OPENVR COORDINATE SYSTEM
            Up: +Y
            Right: +X
            Forwards: -Z

            LODESTAR COORDINATE SYSTEM
            Up: +Z
            Right +X
            Forwards: +Y
*/

            // Position
            m_pose.vecPosition[0] /*X*/ = report.pose_position[0];  // X
            m_pose.vecPosition[1] /*Y*/ = report.pose_position[2];  // Z
            m_pose.vecPosition[2] /*Z*/ = -report.pose_position[1]; // Y

            // Orientation
            vr::HmdQuaternion_t qLodestar =
            {
                qLodestar.w = report.pose_orientation[3], // W
                qLodestar.x = report.pose_orientation[0], // X
                qLodestar.y = report.pose_orientation[1], // Y
                qLodestar.z = report.pose_orientation[2]  // Z
            };

            const float s = std::sqrt(0.5f);
            const vr::HmdQuaternion_t r = { s, -s, 0.f, 0.f };
            const vr::HmdQuaternion_t r2 = { 0.f, -1.f, 0.f, 0.f }; // r^2

            m_pose.qRotation = HmdQuaternion_Normalize((r * qLodestar) * r2); // Lodestar -> OpenVR

            // Linear Velocity
            m_pose.vecVelocity[0] = report.pose_linearVelocity[0];
            m_pose.vecVelocity[1] = report.pose_linearVelocity[1];
            m_pose.vecVelocity[2] = report.pose_linearVelocity[2];

            // Angular Velocity
            m_pose.vecAngularVelocity[0] = report.pose_angularVelocity[0];
            m_pose.vecAngularVelocity[1] = report.pose_angularVelocity[1];
            m_pose.vecAngularVelocity[2] = report.pose_angularVelocity[2];

            break;
        }

        case HID::Status::Ready:
        {
            m_pose.deviceIsConnected = true;
            m_pose.poseIsValid = false;
            m_pose.result = vr::TrackingResult_Uninitialized;
            break;
        }

        default:
        {
            m_pose.deviceIsConnected = false;
            m_pose.poseIsValid = false;
            m_pose.result = vr::TrackingResult_Uninitialized;
            break;
        }
    }

    vr::VRServerDriverHost()->TrackedDevicePoseUpdated(m_deviceID, m_pose, sizeof(vr::DriverPose_t));
}