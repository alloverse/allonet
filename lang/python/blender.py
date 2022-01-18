import bpy, sys, os
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from allo import lib as allonet

bl_info = {
    "name": "Alloverse",
    "blender": (2, 80, 0),
    "category": "Object",
}
def register():
    print("Hello World")
    client = allonet.alloclient_create(False)
    print(client)
def unregister():
    print("Goodbye World")

# This allows you to run the script directly from Blender's Text editor
# to test the add-on without having to install it.
if __name__ == "__main__":
    register()