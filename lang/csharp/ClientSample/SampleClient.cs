using System;
using System.Diagnostics;
using System.IO;
using System.Collections;
using System.Collections.Generic;
using Allonet;
using Allonet.Component;


class SampleClient
{
    AlloClient client;
    static void Main(string[] args)
    {
         SampleClient program = new SampleClient();
         program.RunApp(args[0]);
    }

    bool running = true;
    string myAvatarEntityId;
    FileStream cubeAsset;

    void RunApp(string url)
    {
        Debug.AutoFlush = true;
        Debug.WriteLine("Hello world");
        AlloIdentity ident = new AlloIdentity();
        ident.display_name = "test";

        EntitySpecification ava = new EntitySpecification();
        ava.components.geometry = new AssetGeometry("asset:sha256:220969d522c1a88edccdeb5330980e27cfb095a5a62c8f1e870cfa42be13cd7b");
        ava.components.material = new Material();
        ava.components.material.shader_name = "pbr";
        
        cubeAsset = new FileStream("cube.glb", FileMode.Open, FileAccess.Read, FileShare.Read);

        client = new AlloClient();
        client.onAdded = EntityAdded;
        client.onRemoved = EntityRemoved;
        client.onInteraction = Interaction;
        client.onDisconnected = OnDisconnected;
        client.onAssetBytesRequested = OnAssetBytesRequested;

        client.Connect(url, ident, ava);

        while(running)
        {
            client.Poll(20);

        }
    }

    void EntityAdded(AlloEntity entity)
    {
        Debug.WriteLine("New entity: " + entity.id);
    }

    void EntityRemoved(AlloEntity entity)
    {
        Debug.WriteLine("Lost entity: " + entity.id);
    }

    void Interaction(string type, AlloEntity from, AlloEntity to, List<object> cmd)
    {
        if(cmd.Count >= 3 && cmd[0].ToString() == "announce") {
            myAvatarEntityId = cmd[1].ToString();
            string placeName = cmd[2].ToString();
            Debug.WriteLine("Successfully connected to " + placeName);
        }
    }
    private void OnDisconnected()
    {
        client = null;
        running = false;
        Debug.WriteLine("Disconnected");
    }

    private void OnAssetBytesRequested(string assetId, long offset, long length)
    {
        cubeAsset.Seek(offset, SeekOrigin.Begin);
        long remaining = cubeAsset.Length - offset;
        long toRead = Math.Min(remaining, length);
        byte[] b = new byte[toRead];
        long readLength = cubeAsset.Read(b, 0, b.Length);
        Debug.Assert(readLength == b.Length);

        client.SendAsset(assetId, b, offset, cubeAsset.Length);
    }
}
