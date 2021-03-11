using System;
using Allonet;

class Program
{
    AlloClient client;
    static void Main(string[] args)
    {


        Program prog = new Program();
        AlloIdentity ident = new AlloIdentity();
        ident.display_name = "test";

        LitJson.JsonData avatarDesc = new LitJson.JsonData();
        avatarDesc["geometry"] = new LitJson.JsonData();
        avatarDesc["geometry"]["type"] = new LitJson.JsonData("hardcoded-model");
        avatarDesc["geometry"]["name"] = new LitJson.JsonData("cubegal");

        prog.client = new AlloClient(args[0], ident, avatarDesc);
    }
}