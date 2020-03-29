#!/usr/bin/env python3
import upnpclient

def main():
    d = upnpclient.Device("http://miracast-sidescreen:60099/")

    print("AVTransport.GetCurrentTransportActions")
    print("    ", d.AVTransport.GetCurrentTransportActions(InstanceID=0))
    print("AVTransport.GetDeviceCapabilities")
    print("    ", d.AVTransport.GetDeviceCapabilities(InstanceID=0))
    print("AVTransport.GetMediaInfo")
    print("    ", d.AVTransport.GetMediaInfo(InstanceID=0))
    print("AVTransport.GetPositionInfo")
    print("    ", d.AVTransport.GetPositionInfo(InstanceID=0))
    print("AVTransport.GetTransportInfo")
    print("    ", d.AVTransport.GetTransportInfo(InstanceID=0))
    print("AVTransport.GetTransportSettings")
    print("    ", d.AVTransport.GetTransportSettings(InstanceID=0))
    print("ConnectionManager.GetCurrentConnectionIDs")
    print("    ", d.ConnectionManager.GetCurrentConnectionIDs())
    print("ConnectionManager.GetCurrentConnectionInfo")
    print("    ", d.ConnectionManager.GetCurrentConnectionInfo(ConnectionID=0))
    print("ConnectionManager.GetProtocolInfo")
    print("    ", d.ConnectionManager.GetProtocolInfo())
    print("RenderingControl.GetBrightness")
    print("    ", d.RenderingControl.GetBrightness(InstanceID=0))
    print("RenderingControl.GetContrast")
    print("    ", d.RenderingControl.GetContrast(InstanceID=0))
    print("RenderingControl.GetMute")
    print("    ", d.RenderingControl.GetMute(InstanceID=0, Channel='Master'))
    print("RenderingControl.GetVolume")
    print("    ", d.RenderingControl.GetVolume(InstanceID=0, Channel='Master'))
    print("RenderingControl.ListPresets")
    print("    ", d.RenderingControl.ListPresets(InstanceID=0))

if __name__ == '__main__':
    main()

