using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using MathNet.Numerics.LinearAlgebra.Double;
using MathNet.Spatial.Euclidean;

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
        public Component.UI ui;
        public Component.Collider collider;
        public Component.Relationships relationships;
        public Component.Grabbable grabbable;
    }

    namespace Component 
    {
        public class Transform
        {
            public CoordinateSystem matrix = new CoordinateSystem();
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
        public class HardcodedGeometry : Geometry
        {
            override public string type { get { return "asset"; } }
            public string name;
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

        abstract public class Collider
        {
            abstract public string type {get;}
        }
        public class BoxCollider : Collider
        {
            override public string type { get { return "box"; } }
            public double width = 1, height = 1, depth = 1;

            public BoxCollider(double width, double height, double depth)
            {
                this.width = width;
                this.height = height;
                this.depth = depth;
            }
        }

        public class Material
        {
            public List<double> color; // vec4 rgba
            public string shader_name;
        }

        public class UI
        {
            public string view_id;

            public UI(string id)
            {
                this.view_id = id;
            }
        }

        public class Relationships
        {
            public string parent;

            public Relationships(string parentId)
            {
                this.parent = parentId;
            }
        }

        public class Grabbable
        {
            public string actuate_on;
            public List<int> translation_constraint;
            public List<int> rotation_constraint;
        }
    }
}
