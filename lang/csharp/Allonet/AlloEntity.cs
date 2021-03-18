using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using MathNet.Numerics.LinearAlgebra;

namespace Allonet
{
    public class AlloEntity
    {
        public string id;
        public string owner;
        public AlloComponents components = new AlloComponents();
    };
    public class AlloComponents
    {
        public Component.Transform transform = new Component.Transform();
        public Component.Geometry geometry;
        public Component.Material material;
    }

    namespace Component 
    {
        public class Transform
        {
            public List<double> matrix;
        }

        abstract public class Geometry
        {
            abstract public string type {get;}
        }
        public class AssetGeometry : Geometry
        {
            override public string type { get { return "asset"; } }
            public string name;

            public AssetGeometry(string name)
            {
                this.name = name;
            }
        }
        public class InlineGeometry : Geometry
        {
            override public string type { get { return "inline"; } }
            public string name;
            public List<List<double>> vertices; // vec3
            public List<List<double>> normals; // vec3
            public List<List<double>> uvs; // vec2
            public List<List<int>> triangles; // vec3 of indices
        }

        public class Material
        {
            public Vector<double> color; // vec4 rgba
            public string shader_name;
        }
    }
}
