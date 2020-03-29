#!/usr/bin/env python3
import upnpclient

def main():
    d = upnpclient.Device("http://miracast-sidescreen:60099/")

    for s in d.services:
        print("  ", s.name)
        for a in s.actions:
            print("    -", a.name)
            for arg in a.argsdef_in:
                print("        %s:%s (%s)" % (arg[0], arg[1]['datatype'], arg[1]['allowed_values']))

if __name__ == '__main__':
    main()
