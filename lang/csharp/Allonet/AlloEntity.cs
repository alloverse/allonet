using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace Allonet
{
    public class AlloEntity
    {
        public string id;
        public string owner;
        public LitJson.JsonData components;
        public Component.Transform transform {
            get {
                if(!components.ContainsKey("transform")) {
                    return null;
                }
                LitJson.JsonData transformRep = components["transform"];
                Component.Transform transform = new Component.Transform();
                /// uh fill it with data
                return transform;
            }
        }
    };

    namespace Component {
        public class Transform {
            public AlloMatrix matrix;
        }
    }
}
