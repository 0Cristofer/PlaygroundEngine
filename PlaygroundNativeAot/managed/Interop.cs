using System;
using System.Runtime.InteropServices;

namespace ManagedEcs;

// A blittable component, laid out identically to the C++ Position struct.
// Crosses the boundary as raw memory, no marshalling.
[StructLayout(LayoutKind.Sequential)]
public struct Position
{
    public float X;
    public float Y;
    public float Z;
}

// The "few C++ functions opened as managed hooks": a struct of native function
// pointers the host hands to managed code at init.
[StructLayout(LayoutKind.Sequential)]
public unsafe struct NativeHooks
{
    public delegate* unmanaged<byte*, int, void> Log;
}

public static unsafe class Interop
{
    private static NativeHooks _hooks;

    [UnmanagedCallersOnly(EntryPoint = "Managed_Initialize")]
    public static void Initialize(NativeHooks* hooks)
    {
        _hooks = *hooks;
        Log("[C#] runtime up, native hooks received"u8);
    }

    // Native -> managed per-frame "system": mutates native component storage in place.
    [UnmanagedCallersOnly(EntryPoint = "Managed_TickPositions")]
    public static void TickPositions(Position* items, int count)
    {
        for (int i = 0; i < count; i++)
        {
            items[i].X += 1.0f;
            items[i].Y += 2.0f;
            items[i].Z += 3.0f;
        }

        Log("[C#] TickPositions ran a system over native storage"u8);
    }

    // Managed -> native call back through a hook.
    private static void Log(ReadOnlySpan<byte> utf8)
    {
        if (_hooks.Log == null)
        {
            return;
        }

        fixed (byte* text = utf8)
        {
            _hooks.Log(text, utf8.Length);
        }
    }
}
