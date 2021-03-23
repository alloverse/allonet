using System;
using System.Diagnostics;
using System.IO;
using System.Collections;
using System.Collections.Generic;
using MathNet.Spatial.Euclidean;

public class Size
{
    public double Width = 1.0;
    public double Height = 1.0;
    public double Depth = 1.0;
}

public class Bounds
{
    public Size Size = new Size();
    public CoordinateSystem Pose = new CoordinateSystem();

    public Bounds(double x, double y, double z, double w, double h, double d)
    {
        Size.Width = w;
        Size.Height = h;
        Size.Depth = d;
        Move(x, y, z);
    }

    public Bounds Move(double x, double y, double z)
    {
        Pose.Multiply(CoordinateSystem.Translation(new Vector3D(x, y, z)), Pose);
        return this;
    }
}
