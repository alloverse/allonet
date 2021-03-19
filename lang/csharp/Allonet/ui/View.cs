using System;
using System.Diagnostics;
using System.IO;
using System.Collections;
using System.Collections.Generic;

namespace Allonet
{
    public class View
    {
        /// Override this to describe how your view is represented in the world
        public EntitySpecification Specification()
        {
            EntitySpecification spec = new EntitySpecification();
            return spec;
        }

        public List<View> subviews = new List<View>();

        private App _app;
        public App app
        {
            get => _app;
            set {
                _app = app;
                foreach(View child in subviews) {
                    child.app = app;
                }
            }
        }
    }
}
