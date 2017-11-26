#!/usr/bin/env python3

import asyncio, evdev, json, argparse, sys, re, socket, struct
from evdev import InputDevice, UInput, categorize, ecodes
from pprint import pprint

multicast_group = ('239.255.77.88', 4020); 

remote_codemap = {
    # Extended scancodes
     96: 0xe01c,   # KEY_KPENTER
     97: 0xe01d,   # KEY_RIGHTCTRL
     98: 0xe035,   # KEY_KPSLASH
    100: 0xe038,   # KEY_RIGHTALT
    139: 0xe05d,   # KEY_MENU
    116: 0xe05e,   # KEY_POWER
    142: 0xe05f,   # KEY_SLEEP
    143: 0xe063,   # KEY_WAKEUP

    # These would need fakeshifts with scancodes, so we use virtual codes instead.
    103: 0xf026,   # KEY_UP
    105: 0xf025,   # KEY_LEFT
    106: 0xf027,   # KEY_RIGHT
    108: 0xf028,   # KEY_DOWN
    102: 0xf024,   # KEY_HOME
    104: 0xf021,   # KEY_PAGEUP
    107: 0xf023,   # KEY_END
    109: 0xf022,   # KEY_PAGEDOWN
    110: 0xf02d,   # KEY_INSERT
    111: 0xf02e,   # KEY_DELETE
    125: 0xf05b,   # KEY_LEFTMETA
    126: 0xf05c,   # KEY_RIGHTMETA

    # Nonstandard scancodes, use virtual codes instead.
    113: 0xf0AD,   # KEY_MUTE
    114: 0xf0AE,   # KEY_VOLUMEDOWN
    115: 0xf0AF,   # KEY_VOLUMEUP
    163: 0xf0B0,   # KEY_NEXTSONG
    164: 0xf0B3,   # KEY_PLAYPAUSE
    165: 0xf0B1,   # KEY_PREVIOUSSONG
    158: 0xf0A6,   # KEY_BACK
    172: 0xf0AC,   # KEY_HOMEPAGE
    127: 0xf0B4,   # KEY_COMPOSE

    # Mouse buttons
    272: 0xf101,   # BTN_LEFT
    273: 0xf102,   # BTN_RIGHT
    274: 0xf103,   # BTN_MIDDLE
    275: 0xf104,   # BTN_SIDE
    276: 0xf105,   # BTN_EXTRA
}

def get_evdevices():
    return [ InputDevice(fn) for fn in evdev.list_devices() ];
    
def find_evdevice_by_name(name):
    for evdevice in get_evdevices():
        if evdevice.name == name:
            return evdevice
    return None

parser = argparse.ArgumentParser()

parser.add_argument("-c","--config", type=str,
                    help="configuration file")

parser.add_argument("-l", "--list", action="store_true",
                    help="list available input devices")

args = parser.parse_args()

if args.list:
    for evdevice in reversed(get_evdevices()):
        if re.match("^usb", evdevice.phys):
            print('%40s | %s' % (evdevice.phys, evdevice.name))
    sys.exit(0)

with open(args.config) as data_file:    
    config = json.load(data_file)

devices = [];
for device in config['devices']:
    if device['devname'] is not None:
        evdevice = find_evdevice_by_name(device['devname'])
        if evdevice is not None:
            evdevice.grab()
            caps = evdevice.capabilities()
            del caps[ecodes.EV_SYN]
            outdevice = UInput(caps, name=device['devname']+" [octopus]" )
            device['outdevice'] = outdevice
            device['evdevice'] = evdevice
            devices.append(device)

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)

if 'options' in config and 'interface_ip' in config['options']:
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(config['options']['interface_ip']))


remoteMode = False


def send_remote(event):
    global sock
    remote_type = event.type
    remote_code = event.code

    if remote_type == ecodes.EV_KEY:
        # Mouse Buttons
        if remote_code in ecodes.BTN:
            remote_type = 3
            # Don't send mouse button repetitions
            if (event.value == 2):
                return
        # Key translations
        if remote_code in remote_codemap:
            remote_code = remote_codemap[event.code]

    if remote_type == ecodes.EV_REL:
        # Vertical wheel
        if remote_code == ecodes.REL_WHEEL:
            remote_type = 4
            remote_code = 1    
        # Horizontal wheel
        elif remote_code == ecodes.REL_HWHEEL:
            remote_type = 4
            remote_code = 2

    sock.sendto(struct.pack("<HHi", remote_type, remote_code, event.value), multicast_group)


def forward_local(outdevice, event, handle):
    if handle is not None and 'replace_local' in handle:
        for code in handle['replace_local']:
            if code in ecodes.ecodes:
                event.code = ecodes.ecodes[code]
                outdevice.write_event(event)
        outdevice.syn()
        return
    outdevice.write_event(event)
    outdevice.syn()


def forward_remote(event, handle):
    if handle is not None and 'replace_remote' in handle:
        for code in handle['replace_remote']:
            if code in ecodes.ecodes:
                event.code = code
                send_remote(event)
        return
    send_remote(event);
    

async def handle_events(device):
    global remoteMode

    async for event in device['evdevice'].async_read_loop():

        #if (event.type > 0):
        #    print(categorize(event))
        #    print("     type{} code{} value{}".format(event.type, event.code, event.value))

        if event.type == ecodes.EV_KEY:

            if 'handle' in device and \
                event.code in ecodes.bytype[ecodes.EV_KEY]:

                kcode = ecodes.bytype[ecodes.EV_KEY][event.code];
                if type(kcode) is list:
                    kcode = kcode[0]

                if kcode in device['handle']:
               
                    handle = device['handle'][kcode]
                    
                    if event.value == 0 and 'mode_switch' in handle:
                        remoteMode = not remoteMode
                        #print("Remote mode now %i" % remoteMode)
                   
                    if remoteMode:
                        if 'always_local' in handle:
                            forward_local(device['outdevice'], event, handle)
                        if 'filter_remote' in handle:
                            continue
                        forward_remote(event, handle)
                    else:
                        if 'always_remote' in handle:
                            forward_remote(event, handle)
                        if 'filter_local' in handle:
                            continue
                        forward_local(device['outdevice'], event, handle)
                    continue
        

        if remoteMode:
            # Don't forward things other than REL or KEY to remote
            if event.type == ecodes.EV_REL or event.type == ecodes.EV_KEY:
                forward_remote(event, None)
        else:
            forward_local(device['outdevice'], event, None)

for device in devices:
    asyncio.ensure_future(handle_events(device))

loop = asyncio.get_event_loop()
loop.run_forever()
