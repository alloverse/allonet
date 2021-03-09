using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace Allonet
{
    class AlloEntity
    {
        public string id;
        public string owner;
        public LitJson.JsonData components;
        public AlloComponent.Transform transform {
            get {
                if(!components.ContainsKey("transform")) {
                    return null;
                }
                LitJson.JsonData transformRep = components["transform"];
                AlloComponent.Transform transform = new AlloComponent.Transform();
                /// uh fill it with data
                return transform;
            }
        }
    };

    namespace AlloComponent {
        class Transform {
            public AlloMatrix matrix;
        }
    }
}
