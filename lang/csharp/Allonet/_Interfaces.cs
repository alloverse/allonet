using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace Allonet
{
    [StructLayout(LayoutKind.Sequential)]
    struct _AlloClient
    {
        const string _dllLocation = "/home/nevyn/Dev/allonet/build/liballonet.so";
        [DllImport(_dllLocation)]
        public unsafe static extern bool allo_initialize(bool redirect_stdout);

        [DllImport(_dllLocation)]
        public unsafe static extern _AlloClient *alloclient_create(bool threaded);

        [DllImport(_dllLocation)]
        public unsafe static extern bool alloclient_connect(_AlloClient* client, IntPtr urlString, IntPtr identity, IntPtr avatarDesc);

        [DllImport(_dllLocation)]
        public unsafe static extern void alloclient_disconnect(_AlloClient* client, int reason);

        [DllImport(_dllLocation)]
        public unsafe static extern void alloclient_poll(_AlloClient* client, int timeout_ms);

        [DllImport(_dllLocation)]
        public unsafe static extern void alloclient_send_interaction(_AlloClient* client, _AlloInteraction *interaction);

        [DllImport(_dllLocation)]
        public unsafe static extern void alloclient_set_intent(_AlloClient* client, AlloIntent intent);

        [DllImport(_dllLocation)]
        public unsafe static extern _AlloInteraction *allo_interaction_create(IntPtr type, IntPtr sender_entity_id, IntPtr receiver_entity_id, IntPtr request_id, IntPtr body);

        [DllImport(_dllLocation)]
        public unsafe static extern void allo_interaction_free(_AlloInteraction *interaction);

        [DllImport(_dllLocation)]
        public unsafe static extern void alloclient_asset_request(_AlloClient* client, IntPtr asset_id, IntPtr entity_id);

        [DllImport(_dllLocation)]
        public unsafe static extern void alloclient_asset_send(_AlloClient* client, IntPtr asset_id, IntPtr data, UIntPtr offset, UIntPtr length, UIntPtr total_size);

        [DllImport(_dllLocation)]
        public unsafe static extern IntPtr cJSON_Print(IntPtr cjson);
        [DllImport(_dllLocation)]
        public unsafe static extern IntPtr allo_free(IntPtr mallocd);
        
        public IntPtr state_callback;
        public IntPtr interaction_callback;
        public IntPtr audio_callback;
        public IntPtr disconnected_callback;
        public IntPtr asset_request_bytes_callback;
        public IntPtr asset_receive_callback;
        public IntPtr asset_state_callback;

        // internal
        public _AlloState state;
        public IntPtr _internal;
        public IntPtr _internal2;
        public IntPtr _backref;

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public unsafe delegate void StateCallbackFun(_AlloClient* client, ref _AlloState state);
        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public unsafe delegate void InteractionCallbackFun(_AlloClient* client, _AlloInteraction *inter);
        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public unsafe delegate void DisconnectedCallbackFun(_AlloClient* client);
        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public unsafe delegate void AssetRequestBytesCallbackFun(_AlloClient* client, IntPtr assetId, UIntPtr offset, UIntPtr length);
        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public unsafe delegate void AssetReceiveCallbackFun(_AlloClient* client, IntPtr assetId, IntPtr buffer, UIntPtr offset, UIntPtr length, UIntPtr total_length);
        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public unsafe delegate void AssetStateCallbackFun(_AlloClient* client, IntPtr assetId, int state);
    };

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack = 1)]
    struct _AlloState
    {
        public Int64 revision;
        public unsafe _AlloEntity* entityHead;
    };

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack = 1)]
    public struct AlloIntent
    {
        public IntPtr entity_id;
        public double zmovement;
        public double xmovement;
        public double yaw;
        public double pitch;
        public AlloPoses poses;
        public Int64 ackStateRev;
    };

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack = 1)]
    public struct _AlloInteraction
    {
        public IntPtr type;
        public IntPtr senderEntityId;
        public IntPtr receiverEntityId;
        public IntPtr requestId;
        public IntPtr body;
    };

    // column major matrix
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack = 1)]
    public struct AlloMatrix
    {
        public double c1r1, c1r2, c1r3, c1r4,
			c2r1, c2r2, c2r3, c2r4,
			c3r1, c3r2, c3r3, c3r4,
			c4r1, c4r2, c4r3, c4r4;
    };

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack = 1)]
    public struct AlloPose
    {
        public AlloMatrix matrix;
    };

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack = 1)]
    public struct AlloHandPose
    {
        public AlloMatrix matrix;
        public AlloMatrix PALM;
        public AlloMatrix WRIST;
        public AlloMatrix THUMB_METACARPAL;
        public AlloMatrix THUMB_PROXIMAL;
        public AlloMatrix THUMB_DISTAL;
        public AlloMatrix THUMB_TIP;
        public AlloMatrix INDEX_METACARPAL;
        public AlloMatrix INDEX_PROXIMAL;
        public AlloMatrix INDEX_INTERMEDIATE;
        public AlloMatrix INDEX_DISTAL;
        public AlloMatrix INDEX_TIP;
        public AlloMatrix MIDDLE_METACARPAL;
        public AlloMatrix MIDDLE_PROXIMAL;
        public AlloMatrix MIDDLE_INTERMEDIATE;
        public AlloMatrix MIDDLE_DISTAL;
        public AlloMatrix MIDDLE_TIP;
        public AlloMatrix RING_METACARPAL;
        public AlloMatrix RING_PROXIMAL;
        public AlloMatrix RING_INTERMEDIATE;
        public AlloMatrix RING_DISTAL;
        public AlloMatrix RING_TIP;
        public AlloMatrix PINKY_METACARPAL;
        public AlloMatrix PINKY_PROXIMAL;
        public AlloMatrix PINKY_INTERMEDIATE;
        public AlloMatrix PINKY_DISTAL;
        public AlloMatrix PINKY_TIP;
        public AlloPoseGrab grab;
    };

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack = 1)]
    public struct AlloPoseGrab
    {
        public IntPtr entity_id;
        AlloMatrix grabberFromEntityTransform;
    };

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack = 1)]
    public struct AlloPoses
    {
        public AlloPose root;
        public AlloPose head;
        public AlloPose torso;
        public AlloHandPose leftHand;
        public AlloHandPose rightHand;
    };



    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack = 1)]
    struct _AlloEntity
    {
        public IntPtr id; // to string
        public IntPtr ownerAgentId;
        public IntPtr components; // cJSON*
        public unsafe _AlloEntity* le_next;
    };
}
