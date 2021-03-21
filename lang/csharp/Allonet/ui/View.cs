using System;
using System.Diagnostics;
using System.IO;
using System.Collections;
using System.Collections.Generic;

namespace Allonet
{
    public class View
    {
        public string ViewId {get; private set;} = Guid.NewGuid().ToString("N");
        

        /// Override this to describe how your view is represented in the world
        public EntitySpecification Specification()
        {
            EntitySpecification spec = new EntitySpecification();
            return spec;
        }

        public List<View> Subviews = new List<View>();

        public View FindView(string viewId)
        {
            if(viewId == this.ViewId)
            {
                return this;
            }

            foreach(View view in Subviews)
            {
                View found = view.FindView(viewId);
                if(found != null)
                {
                    return found;
                }
            }
            return null;
        }

        private App _app;
        public App app
        {
            get => _app;
            set {
                _app = app;
                foreach(View child in Subviews) {
                    child.app = app;
                }
            }
        }

        public AlloEntity entity;
        public void Awake()
        {

        }

        public void OnInteraction(string type, List<object> body, AlloEntity sender)
        {

        }
    }
}
