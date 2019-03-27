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
        public struct MOUSEINPUT
        {
            public int dx;
            public int dy;
            public int mouseData;
            public uint dwFlags;
            public uint time;
            public IntPtr dwExtraInfo;
        }
        [StructLayout(LayoutKind.Sequential)]
        public struct KEYBDINPUT
        {
            public ushort wVk;
            public ushort wScan;
            public uint dwFlags;
            public uint time;
            public IntPtr dwExtraInfo;
        }
        [StructLayout(LayoutKind.Sequential)]
        public struct HARDWAREINPUT
        {
            public uint uMsg;
            public ushort wParamL;
            public ushort wParamH;
        }
        [StructLayout(LayoutKind.Explicit)]
        public struct InputBatch
        {
            [FieldOffset(0)]
            public HARDWAREINPUT Hardware;
            [FieldOffset(0)]
            public KEYBDINPUT Keyboard;
            [FieldOffset(0)]
            public MOUSEINPUT Mouse;
        }
        [StructLayout(LayoutKind.Sequential)]
        public struct INPUT
        {
            public SendInputEventType type;
            public InputBatch data;
        }
        [Flags]
        public enum MouseEventFlags : uint
        {
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

        [DllImport("user32.dll", SetLastError = true)]
        public static extern uint SendInput(uint nInputs, INPUT[] pInputs, int cbSize);

        static public int SendSingleInput(INPUT input)
        {
            INPUT[] inputs = new INPUT[] { input };
            uint numSent = SendInput(1, inputs, Marshal.SizeOf(typeof(INPUT)));
            if (numSent == 0)
            {
                int error = Marshal.GetLastWin32Error();
                Console.WriteLine("SendInput error: " + error);
                return error;
            }
            return 0;
        }

        static void Main(string[] args)
        {
            UdpClient socket = new UdpClient(4020);
            socket.JoinMulticastGroup(IPAddress.Parse("239.255.77.88"));
            IPEndPoint RemoteIpEndPoint = new IPEndPoint(IPAddress.Any, 0);

            INPUT key_input = new INPUT { type = SendInputEventType.InputKeyboard };
            INPUT mouse_input = new INPUT { type = SendInputEventType.InputMouse };

            key_input.data.Keyboard = new KEYBDINPUT();
            mouse_input.data.Mouse = new MOUSEINPUT();

            key_input.data.Keyboard.time = 0;
            mouse_input.data.Mouse.time = 0;

            key_input.data.Keyboard.dwExtraInfo = IntPtr.Zero;
            mouse_input.data.Mouse.dwExtraInfo = IntPtr.Zero;

            while (true)
            {
                Byte[] message = socket.Receive(ref RemoteIpEndPoint);

                using (BinaryReader reader = new BinaryReader(new MemoryStream(message)))
                {
                    byte client = reader.ReadByte();
                    ushort type = reader.ReadUInt16();
                    ushort code = reader.ReadUInt16();
                    int value = reader.ReadInt32();

                    if (client != 1) continue;

                    Console.WriteLine("Type:" + type + " Code:" + code + " Value:" + value);

                    continue;
                    // Keyboard
                    if (type == 1)
                    {
                        if (code > 0xF000)
                        {
                            // Must use wVk mechanism instead of raw scancode
                            key_input.data.Keyboard.wVk = (ushort)(code & 0x00FF);
                            key_input.data.Keyboard.wScan = 0;
                            key_input.data.Keyboard.dwFlags = 0x0;
                        }
                        else
                        {
                            // Raw scancode
                            key_input.data.Keyboard.wVk = 0;
                            key_input.data.Keyboard.wScan = code;
                            key_input.data.Keyboard.dwFlags = 0x8;
                        }

                        if (value == 0)
                        {
                            // Key released
                            key_input.data.Keyboard.dwFlags |= 0x2;
                        }

                        SendSingleInput(key_input);
                    }

               
                    // Relative mouse movement
                    else if (type == 2)
                    {
                        // MOUSEEVENTF_MOVE
                        mouse_input.data.Mouse.dwFlags = (uint)MouseEventFlags.MOUSEEVENTF_MOVE;
                        if (code == 0)
                        {
                            mouse_input.data.Mouse.dx = value;
                            mouse_input.data.Mouse.dy = 0;
                        }
                        else
                        {
                            mouse_input.data.Mouse.dx = 0;
                            mouse_input.data.Mouse.dy = value;
                        }

                        mouse_input.data.Mouse.mouseData = 0;

                        SendSingleInput(mouse_input);
                    }

                    // Mouse Buttons
                    else if (type == 3)
                    {
                        mouse_input.data.Mouse.dy = 0;
                        mouse_input.data.Mouse.dx = 0;
                        mouse_input.data.Mouse.mouseData = 0;

                        // Left
                        if (code == 0xf101)
                        {
                            if (value == 0)
                            {
                                mouse_input.data.Mouse.dwFlags = (uint)MouseEventFlags.MOUSEEVENTF_LEFTUP;
                            }
                            else
                            {
                                mouse_input.data.Mouse.dwFlags = (uint)MouseEventFlags.MOUSEEVENTF_LEFTDOWN;
                            }
                        }
                        // Right
                        else if (code == 0xf102)
                        {
                            if (value == 0)
                            {
                                mouse_input.data.Mouse.dwFlags = (uint)MouseEventFlags.MOUSEEVENTF_RIGHTUP;
                            }
                            else
                            {
                                mouse_input.data.Mouse.dwFlags = (uint)MouseEventFlags.MOUSEEVENTF_RIGHTDOWN;
                            }
                        }
                        // Middle
                        else if(code == 0xf103)
                        {
                            if (value == 0)
                            {
                                mouse_input.data.Mouse.dwFlags = (uint)MouseEventFlags.MOUSEEVENTF_MIDDLEUP;
                            }
                            else
                            {
                                mouse_input.data.Mouse.dwFlags = (uint)MouseEventFlags.MOUSEEVENTF_MIDDLEDOWN;
                            }
                        }
                        // SIDE
                        else if (code == 0xf104)
                        {
                            mouse_input.data.Mouse.mouseData = 1;
                            if (value == 0)
                            {
                                mouse_input.data.Mouse.dwFlags = (uint)MouseEventFlags.MOUSEEVENTF_XUP;
                            }
                            else
                            {
                                mouse_input.data.Mouse.dwFlags = (uint)MouseEventFlags.MOUSEEVENTF_XDOWN;
                            }
                        }
                        // EXTRA
                        else if (code == 0xf105)
                        {
                            mouse_input.data.Mouse.mouseData = 2;
                            if (value == 0)
                            {
                                mouse_input.data.Mouse.dwFlags = (uint)MouseEventFlags.MOUSEEVENTF_XUP;
                            }
                            else
                            {
                                mouse_input.data.Mouse.dwFlags = (uint)MouseEventFlags.MOUSEEVENTF_XDOWN;
                            }
                        }

                        SendSingleInput(mouse_input);
                    }

                    // Mouse Wheel
                    else if (type == 4)
                    {
                        mouse_input.data.Mouse.dy = 0;
                        mouse_input.data.Mouse.dx = 0;

                        // Amount of movement in clicks is in value
                        mouse_input.data.Mouse.mouseData = value * 120;

                        // Vertical wheel
                        if (code == 1)
                        {
                            mouse_input.data.Mouse.dwFlags = (uint)MouseEventFlags.MOUSEEVENTF_WHEEL;
                        }
                        // Horiz. wheel
                        else if (code == 2)
                        {
                            mouse_input.data.Mouse.dwFlags = (uint)MouseEventFlags.MOUSEEVENTF_HWHEEL;
                        }

                        SendSingleInput(mouse_input);
                    }
                }
            }
        }
    }
}
