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
        public Bounds Bounds = new Bounds(0, 0, 0, 1, 1, 1);
        public List<View> Subviews = new List<View>();
        public View Superview {get; private set;} = null;
        private App _app;
        public AlloEntity Entity {get; internal set;} = null;
        public bool IsGrabbable;

        /// Override this to describe how your view is represented in the world
        public EntitySpecification Specification()
        {
            EntitySpecification spec = new EntitySpecification();

            spec.components.ui = new Component.UI(ViewId);

            spec.components.transform.matrix = Bounds.Pose;

            if(Superview != null && Superview.IsAwake)
            {
                spec.components.relationships = new Component.Relationships(Superview.Entity.id);
            }
            if(IsGrabbable)
            {
                spec.components.collider = new Component.BoxCollider(Bounds.Size.Width, Bounds.Size.Height, Bounds.Size.Depth);
                spec.components.grabbable = new Component.Grabbable();
            }
            return spec;
        }

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

        public View addSubview(View subview)
        {
            Debug.Assert(subview.Superview == null);

            Subviews.Add(subview);
            subview.app = app;
            subview.Superview = this;
            if(IsAwake)
            {
                subview.Spawn();
            } // else, wait for awake()

            return subview; // for chaining
        }

        public void Spawn()
        {
            Debug.Assert(Superview != null && Superview.IsAwake);
            EntitySpecification spec = Specification();
            app.client.InteractRequest(
                Superview.Entity.id,
                "place", 
                new List<object>{"spawn_entity", spec}, 
                null
            );
        }

        
        public App app
        {
            get => _app;
            set
            {
                _app = app;
                foreach(View child in Subviews) 
                {
                    child.app = app;
                }
            }
        }

        
        /// Awake() is called when the entity that represents this view
        /// starts existing and is bound to this view.
        public void Awake()
        {
            foreach(View child in Subviews)
            {
                if(child.Entity == null)
                {
                    child.Spawn();
                }
            }
        }
        public bool IsAwake
        {
            get => this.Entity != null;
        }

        public void OnInteraction(string type, List<object> body, AlloEntity sender)
        {

        }
    }
}
