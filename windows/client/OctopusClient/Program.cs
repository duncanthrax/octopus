using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Net.Sockets;
using System.Net;
using System.IO;
using System.Runtime.InteropServices;


namespace OctopusClient
{
    class Program
    {
        [StructLayout(LayoutKind.Sequential)]
        public struct MOUSEINPUT {
            public int dx;
            public int dy;
            public int mouseData;
            public uint dwFlags;
            public uint time;
            public IntPtr dwExtraInfo;
        }
        [StructLayout(LayoutKind.Sequential)]
        public struct KEYBDINPUT {
            public ushort wVk;
            public ushort wScan;
            public uint dwFlags;
            public uint time;
            public IntPtr dwExtraInfo;
        }
        [StructLayout(LayoutKind.Sequential)]
        public struct HARDWAREINPUT {
            public uint uMsg;
            public ushort wParamL;
            public ushort wParamH;
        }
        [StructLayout(LayoutKind.Explicit)]
        public struct InputBatch {
            [FieldOffset(0)]
            public HARDWAREINPUT Hardware;
            [FieldOffset(0)]
            public KEYBDINPUT Keyboard;
            [FieldOffset(0)]
            public MOUSEINPUT Mouse;
        }
        [StructLayout(LayoutKind.Sequential)]
        public struct INPUT {
            public SendInputEventType type;
            public InputBatch data;
        }
        [Flags]
        public enum LinuxEventTypes : uint {
            EV_SYN		 = 0x00,
            EV_KEY		 = 0x01,
            EV_REL		 = 0x02,
            EV_ABS		 = 0x03,
            EV_MSC		 = 0x04,
            EV_SW		 = 0x05,
            EV_LED		 = 0x11,
            EV_SND		 = 0x12,
            EV_REP		 = 0x14,
            EV_FF		 = 0x15,
            EV_PWR		 = 0x16,
            EV_FF_STATUS = 0x17,
            EV_MAX		 = 0x1f,
            EV_CNT		 = 0x20
        }
        public enum LinuxSynCodes : uint {
            SYN_REPORT    = 0,
            SYN_CONFIG    = 1,
            SYN_MT_REPORT = 2,
            SYN_DROPPED   = 3,
            SYN_MAX       = 0xf,
            SYN_CNT       = 0x10
        }

        public enum MouseEventFlags : uint {
            MOUSEEVENTF_MOVE = 0x0001,
            MOUSEEVENTF_LEFTDOWN = 0x0002,
            MOUSEEVENTF_LEFTUP = 0x0004,
            MOUSEEVENTF_RIGHTDOWN = 0x0008,
            MOUSEEVENTF_RIGHTUP = 0x0010,
            MOUSEEVENTF_MIDDLEDOWN = 0x0020,
            MOUSEEVENTF_MIDDLEUP = 0x0040,
            MOUSEEVENTF_XDOWN = 0x0080,
            MOUSEEVENTF_XUP = 0x0100,
            MOUSEEVENTF_WHEEL = 0x0800,
            MOUSEEVENTF_VIRTUALDESK = 0x4000,
            MOUSEEVENTF_ABSOLUTE = 0x8000,
            MOUSEEVENTF_HWHEEL = 0x01000
        }
        public enum SendInputEventType : int
        {
            InputMouse,
            InputKeyboard,
            InputHardware
        }
        public enum UsefulConst : uint {
            BTN_MIN    = 0x0100,
            BTN_MAX    = 0x015f,

            // Supported mouse buttons
            BTN_LEFT   = 0x110,
            BTN_RIGHT  = 0x111,
            BTN_MIDDLE = 0x112,
            BTN_SIDE   = 0x113,
            BTN_EXTRA  = 0x114,

            // Supported relative axes
            REL_X      = 0x00,
            REL_Y      = 0x01,
            REL_HWHEEL = 0x06,
            REL_WHEEL  = 0x08
        }

        public dictionary LinuxKeyCode2Extended = new Dictionary<uint, uint> {
            {  96, 0x001c },    // KEY_KPENTER
            {  97, 0x001d },    // KEY_RIGHTCTRL
            {  98, 0x0035 },    // KEY_KPSLASH
            { 100, 0x0038 },    // KEY_RIGHTALT
            { 139, 0x005d },    // KEY_MENU
            { 116, 0x005e },    // KEY_POWER
            { 142, 0x005f },    // KEY_SLEEP
            { 143, 0x0063 }     // KEY_WAKEUP
        };

        public dictionary LinuxKeyCode2Virtual = new Dictionary<uint, uint> {
            // These would need fakeshifts with scancodes, so we use virtual codes instead.
            { 103, 0x0026 },   // KEY_UP
            { 105, 0x0025 },   // KEY_LEFT
            { 106, 0x0027 },   // KEY_RIGHT
            { 108, 0x0028 },   // KEY_DOWN
            { 102, 0x0024 },   // KEY_HOME
            { 104, 0x0021 },   // KEY_PAGEUP
            { 107, 0x0023 },   // KEY_END
            { 109, 0x0022 },   // KEY_PAGEDOWN
            { 110, 0x002d },   // KEY_INSERT
            { 111, 0x002e },   // KEY_DELETE
            { 125, 0x005b },   // KEY_LEFTMETA
            { 126, 0x005c },   // KEY_RIGHTMETA
            // Nonstandard scancodes, use virtual codes instead.
            { 113, 0x00AD },   // KEY_MUTE
            { 114, 0x00AE },   // KEY_VOLUMEDOWN
            { 115, 0x00AF },   // KEY_VOLUMEUP
            { 163, 0x00B0 },   // KEY_NEXTSONG
            { 164, 0x00B3 },   // KEY_PLAYPAUSE
            { 165, 0x00B1 },   // KEY_PREVIOUSSONG
            { 158, 0x00A6 },   // KEY_BACK
            { 172, 0x00AC },   // KEY_HOMEPAGE
            { 127, 0x00B4 }    // KEY_COMPOSE
        };

        [DllImport("user32.dll", SetLastError = true)]
        public static extern uint SendInput(uint nInputs, INPUT[] pInputs, int cbSize);

        public INPUT[] stackedInputs;

        static public int StackKbdInput(ushort wVk, ushort wScan, uint dwFlags) {
            INPUT input = new INPUT { type = SendInputEventType.InputKeyboard };
            input.data.Keyboard = new KEYBDINPUT();
            input.data.Keyboard.time = 0;
            input.data.Keyboard.dwExtraInfo = IntPtr.Zero;
            input.data.Keyboard.wVk = wVk;
            input.data.Keyboard.wScan = wScan;
            input.data.Keyboard.dwFlags = dwFlags;
            stackedInputs.push(input);
        }

        static public int StackMouseInput(int dx, int dy, int mouseData, uint dwFlags) {
            INPUT input = new INPUT { type = SendInputEventType.InputMouse };
            input.data.Mouse = new MOUSEINPUT();
            input.data.Mouse.time = 0;
            input.data.Mouse.dwExtraInfo = IntPtr.Zero;
            input.data.Mouse.dx = dx;
            input.data.Mouse.dy = dy;
            input.data.Mouse.mouseData = mouseData;
            input.data.Mouse.dwFlags = dwFlags;
            stackedInputs.push(input);
        }

        static public int SendStackedInputs() {
            SendInput(stackedInputs.Length, stackedInputs, Marshal.SizeOf(typeof(INPUT)));
            Array.Clear(stackedInputs, 0, stackedInputs.Length);
        }

        static void Main(string[] args)
        {
            UdpClient socket = new UdpClient(4020);
            socket.JoinMulticastGroup(IPAddress.Parse("239.255.77.88"));
            IPEndPoint RemoteIpEndPoint = new IPEndPoint(IPAddress.Any, 0);

            while (true) {
                Byte[] message = socket.Receive(ref RemoteIpEndPoint);

                using (BinaryReader reader = new BinaryReader(new MemoryStream(message))) {

                    byte client = reader.ReadByte();
                    ushort type = reader.ReadUInt16();
                    ushort code = reader.ReadUInt16();
                    int value   = reader.ReadInt32();

                    // FIXME
                    if (client != 1) continue;

                    //Console.WriteLine("Type:" + type + " Code:" + code + " Value:" + value);

                    //continue;

                    if (type == LinuxEventTypes.EV_SYN) {
                        // We only handle SYN_REPORT
                        if (code == LinuxSynCodes.SYN_REPORT) {
                            // Send buffered inputs
                            SendStackedInputs();
                        }
                    }
                    else if (type == LinuxEventTypes.EV_KEY) {

                        if (code >= UsefulConst.BTN_MIN && code <= UsefulConst.BTN_MAX) {
                            // Mouse Buttons
                            if (code == UsefulConst.BTN_LEFT) {
                                StackMouseInput(0, 0, 0, value ? (uint)MouseEventFlags.MOUSEEVENTF_LEFTDOWN
                                                               : (uint)MouseEventFlags.MOUSEEVENTF_LEFTUP);
                            }
                            else if (code == UsefulConst.BTN_RIGHT) {
                                StackMouseInput(0, 0, 0, value ? (uint)MouseEventFlags.MOUSEEVENTF_RIGHTDOWN
                                                               : (uint)MouseEventFlags.MOUSEEVENTF_RIGHTUP);
                            }
                            else if(code == UsefulConst.BTN_MIDDLE) {
                                StackMouseInput(0, 0, 0, value ? (uint)MouseEventFlags.MOUSEEVENTF_MIDDLEDOWN
                                                               : (uint)MouseEventFlags.MOUSEEVENTF_MIDDLEUP);
                            }
                            else if (code == UsefulConst.BTN_SIDE) {
                                StackMouseInput(0, 0, 1, value ? (uint)MouseEventFlags.MOUSEEVENTF_XDOWN
                                                               : (uint)MouseEventFlags.MOUSEEVENTF_XUP);
                            }
                            else if (code == UsefulConst.BTN_EXTRA) {
                                StackMouseInput(0, 0, 2, value ? (uint)MouseEventFlags.MOUSEEVENTF_XDOWN
                                                               : (uint)MouseEventFlags.MOUSEEVENTF_XUP);
                            }
                        }
                        else {
                            // Keyboard key
                            if (LinuxKeyCode2Virtual[code]) {
                                // Must use wVk mechanism instead of raw scancode
                                StackKbdInput(LinuxKeyCode2Virtual[code], 0, value ? 0x0 : 0x2);
                            }
                            else if (LinuxKeyCode2Extended[code]) {
                                // Extended key
                                StackKbdInput(0, LinuxKeyCode2Extended[code], value ? 0x1 : 0x3);
                            }
                            else {
                                // Raw scancode
                                StackKbdInput(0, code, value ? 0x8 : 0xa);
                            }
                        }
                    }
                    else if (type == LinuxEventTypes.EV_REL) {

                        if (code == UsefulConst.REL_X) {
                            StackMouseInput(value, 0, 0, (uint)MouseEventFlags.MOUSEEVENTF_MOVE);
                        }
                        else if (code == UsefulConst.REL_Y) {
                            StackMouseInput(0, value, 0, (uint)MouseEventFlags.MOUSEEVENTF_MOVE);
                        }
                        else if (code == UsefulConst.REL_WHEEL) {
                            StackMouseInput(0, 0, value * 120, (uint)MouseEventFlags.MOUSEEVENTF_WHEEL);
                        }
                        else if (code == UsefulConst.REL_HWHEEL) {
                            StackMouseInput(0, 0, value * 120, (uint)MouseEventFlags.MOUSEEVENTF_HWHEEL);
                        }
                    }
                }
            }
        }
    }
}
