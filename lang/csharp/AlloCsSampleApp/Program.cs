using System;
using System.Diagnostics;
using Allonet;

class Program
{
    AlloClient client;
    static void Main(string[] args)
    {
         Program program = new Program();
         program.RunApp(args[0]);
    }

    bool running = true;
    string myAvatarEntityId;

    void RunApp(string url)
    {
        Debug.AutoFlush = true;
        Debug.WriteLine("Hello world");
        Program prog = new Program();
        AlloIdentity ident = new AlloIdentity();
        ident.display_name = "test";

        LitJson.JsonData avatarDesc = new LitJson.JsonData();
        avatarDesc["geometry"] = new LitJson.JsonData();
        avatarDesc["geometry"]["type"] = new LitJson.JsonData("asset");
        avatarDesc["geometry"]["name"] = new LitJson.JsonData("asset:sha256:d2a84f4b8b650937ec8f73cd8be2c74add5a911ba64df27458ed8222da804a21");

        client = new AlloClient();
        client.added = EntityAdded;
        client.removed = EntityRemoved;
        client.interaction = Interaction;
        client.disconnected = OnDisconnected;

        client.Connect(url, ident, avatarDesc);

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

    void Interaction(string type, AlloEntity from, AlloEntity to, LitJson.JsonData cmd)
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
}