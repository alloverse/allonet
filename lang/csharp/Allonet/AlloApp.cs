using System;
using System.Diagnostics;
using System.IO;
using System.Collections;
using System.Collections.Generic;

namespace Allonet
{
    class App
    {
        public AlloClient client {get;}
        public View mainView = new View();
        public List<View> rootViews {get; private set; } = new List<View>();
        
        public bool running {get; private set;}
        public bool connected {get; private set;}
        
        public App(AlloClient client)
        {
            this.client = client;

        }
    }
}