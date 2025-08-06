# Lodestar SteamVR Driver

This driver lets you use Lodestar trackers as native tracked devices in SteamVR.
It connects directly to the OpenVR runtime, allowing Lodestar's 6DoF data to show up just like any other SteamVR device - no extra software or middleware needed.

### Disclaimers

This driver is currently in the Beta testing phase. If you are not comfortable with potential bugs, crashes, or strange behavior, you might consider waiting for a stable release. That being said, if you do encounter any issues, PLEASE report them. Reporting helps us make the experience better for everyone :)

All tracked poses will be relative to your Anchors, not your headset's playspace. In order to align Lodestar to your playspace you can use [Space Calibrator](https://github.com/pushrax/OpenVR-SpaceCalibrator) _(also available on [Steam](https://store.steampowered.com/app/3368750/Space_Calibrator/).)_ Be warned however, that Space Calibrator doesn't always play nicely with Lodestar and may take some work to get the spaces aligned. This is something we are actively working on resolving. For best alignment, rigidly mount a Lodestar tracker to your headset and use the continuous calibration mode.

Full-body tracking is not officially supported at this time. But feel free to experiment! Once the playspaces are properly aligned, you *can use them just like you would Vive trackers, so body tracking is technically possible. It may just require some tinkering.

### Installation

1) Clone this repo
2) Run ```install.bat```
3) Done!

_If you encounter an error while installing please make sure SteamVR is not running and check that the target directory is correct. If you continue to have issues, please manually copy the folder ```lodestar``` to your SteamVR driver directory._
