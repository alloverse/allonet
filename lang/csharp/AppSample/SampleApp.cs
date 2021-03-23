using System;
using System.Diagnostics;
using System.IO;
using System.Collections;
using System.Collections.Generic;
using Allonet;
using Allonet.Component;


class SampleApp
{
    AlloClient client;
    App app;
    static void Main(string[] args)
    {
         SampleApp sample = new SampleApp();
         sample.Run(args[0]);
    }

    void Run(string url)
    {
        Debug.AutoFlush = true;
        Debug.WriteLine("Hello world");

        client = new AlloClient();
        app = new App(client, "csharp-appsample");
        app.mainView = MakeMainUI();
        app.Connect(url);
        app.Run(20);
    }

    View MakeMainUI()
    {
        View root = new View();
        root.Bounds.Move(0, 0, 1.5);
        root.IsGrabbable = true;
        return root;
    }
}
