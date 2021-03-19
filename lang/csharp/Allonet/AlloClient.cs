using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Diagnostics;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

namespace Allonet
{
    public class AlloIdentity
    {
        public string display_name { get; set; }
    }

    public class EntitySpecification
    {
        public AlloComponents components = new AlloComponents();
        public List<EntitySpecification> children = new List<EntitySpecification>();
    }

    public class AlloClient
    {
        private unsafe _AlloClient* client;
        public Dictionary<string, AlloEntity> entities = new Dictionary<string, AlloEntity>();
        public delegate void EntityAdded(AlloEntity entity);
        public EntityAdded onAdded = null;
        public delegate void EntityRemoved(AlloEntity entity);
        public EntityRemoved onRemoved = null;
        public delegate void Interaction(string type, AlloEntity from, AlloEntity to, List<object> command);
        public Interaction onInteraction = null;
        public delegate void Connected();
        public Connected onConnected = null;
        public delegate void Disconnected();
        public Disconnected onDisconnected = null;
        public delegate void AssetBytesRequested(string _assetId, long offset, long length);
        public AssetBytesRequested onAssetBytesRequested = null;

        public bool connected {get; private set;}
        public string avatarId {get; private set;}
        public string placeName {get; private set;}

        public delegate void ResponseCallback(string body);
        private Dictionary<string, ResponseCallback> responseCallbacks = new Dictionary<string, ResponseCallback>();

        private _AlloClient.InteractionCallbackFun interactionCallback;
        private GCHandle interactionCallbackHandle;
        private _AlloClient.DisconnectedCallbackFun disconnectedCallback;
        private GCHandle disconnectedCallbackHandle;
        private _AlloClient.AssetRequestBytesCallbackFun assetRequestBytesCallback;
        private GCHandle assetRequestBytesCallbackHandle;
        private GCHandle thisHandle;


        public AlloClient()
        {
            if (!_AlloClient.allo_initialize(false)) {
               throw new Exception("Unable to initialize Allonet");
            }
            unsafe
            {
                client = _AlloClient.alloclient_create(false);
                if(client == null)
                    throw new Exception("Failed to create Allonet instance");
                
                interactionCallback = new _AlloClient.InteractionCallbackFun(AlloClient._interaction);
                interactionCallbackHandle = GCHandle.Alloc(interactionCallback);
                IntPtr icp = Marshal.GetFunctionPointerForDelegate(interactionCallback);
                client->interaction_callback = icp;

                disconnectedCallback = new _AlloClient.DisconnectedCallbackFun(AlloClient._disconnected);
                disconnectedCallbackHandle = GCHandle.Alloc(disconnectedCallback);
                IntPtr icp2 = Marshal.GetFunctionPointerForDelegate(disconnectedCallback);
                client->disconnected_callback = icp2;

                assetRequestBytesCallback = new _AlloClient.AssetRequestBytesCallbackFun(AlloClient._assetRequestBytes);
                assetRequestBytesCallbackHandle = GCHandle.Alloc(assetRequestBytesCallback);
                IntPtr icp3 = Marshal.GetFunctionPointerForDelegate(assetRequestBytesCallback);
                client->asset_request_bytes_callback = icp3;

                thisHandle = GCHandle.Alloc(this);
                client->_backref = (IntPtr)thisHandle;
            }
        }
        ~AlloClient()
        {
            if (interactionCallbackHandle.IsAllocated)
            {
                interactionCallbackHandle.Free();
                disconnectedCallbackHandle.Free();
            }
        }

        public void Connect(string url, AlloIdentity identity, EntitySpecification avatarDesc)
        {
            unsafe
            {
                IntPtr urlPtr = Marshal.StringToHGlobalAnsi(url);
                IntPtr identPtr = Marshal.StringToHGlobalAnsi(JsonConvert.SerializeObject(identity));
                IntPtr avatarPtr = Marshal.StringToHGlobalAnsi(JsonConvert.SerializeObject(avatarDesc));

                bool ok = _AlloClient.alloclient_connect(client, urlPtr, identPtr, avatarPtr);
                Marshal.FreeHGlobal(urlPtr);
                Marshal.FreeHGlobal(identPtr);
                Marshal.FreeHGlobal(avatarPtr);
                if (!ok)
                {
                    throw new Exception("Failed to connect to " + url);
                }
            }
        }

        public void SetIntent(AlloIntent intent)
        {
            unsafe
            {
                _AlloClient.alloclient_set_intent(client, intent);
            }
        }

        void _Interact(string interactionType, string senderEntityId, string receiverEntityId, string requestId, string body)
        {
            unsafe
            {
                IntPtr interactionTypePtr = Marshal.StringToHGlobalAnsi(interactionType);
                IntPtr senderEntityIdPtr = Marshal.StringToHGlobalAnsi(senderEntityId);
                IntPtr receiverEntityIdPtr = Marshal.StringToHGlobalAnsi(receiverEntityId);
                IntPtr requestIdPtr = Marshal.StringToHGlobalAnsi(requestId);
                IntPtr bodyPtr = Marshal.StringToHGlobalAnsi(body);
                _AlloInteraction *interaction =  _AlloClient.allo_interaction_create(interactionTypePtr, senderEntityIdPtr, receiverEntityIdPtr, requestIdPtr, bodyPtr);
                _AlloClient.alloclient_send_interaction(client, interaction);
                _AlloClient.allo_interaction_free(interaction);
                Marshal.FreeHGlobal(interactionTypePtr);
                Marshal.FreeHGlobal(senderEntityIdPtr);
                Marshal.FreeHGlobal(receiverEntityIdPtr);
                Marshal.FreeHGlobal(requestIdPtr);
                Marshal.FreeHGlobal(bodyPtr);
            }
        }
        public void InteractOneway(string senderEntityId, string receiverEntityId, string body)
        {
            _Interact("oneway", senderEntityId, receiverEntityId, "", body);
        }
        public void InteractRequest(string senderEntityId, string receiverEntityId, string body, ResponseCallback callback)
        {
            string requestId = System.Guid.NewGuid().ToString();
            responseCallbacks[requestId] = callback;
            _Interact("request", senderEntityId, receiverEntityId, requestId, body);
        }

        public void Poll(double timeout)
        {
            int timeoutMs = (int)timeout*1000;
            HashSet<string> newEntityIds = new HashSet<string>();
            HashSet<string> lostEntityIds = new HashSet<string>();
            unsafe
            {
                // 1. Run network to send and get world changes
                _AlloClient.alloclient_poll(client, timeoutMs);

                // 2. Parse through all the C entities and create C# equivalents
                HashSet<string> incomingEntityIds = new HashSet<string>();

                _AlloEntity* entry = client->state.entityHead;
                while(entry != null)
                {
                    string entityId = Marshal.PtrToStringAnsi(entry->id);
                    AlloEntity entity;
                    bool exists = entities.TryGetValue(entityId, out entity);
                    if (!exists)
                    {
                        entity = new AlloEntity();
                        entity.id = entityId;
                        entities[entityId] = entity;
                        newEntityIds.Add(entityId);
                    }
                    incomingEntityIds.Add(entityId);
                    IntPtr componentsJsonPtr = _AlloClient.cJSON_Print(entry->components);
                    string componentsJson = Marshal.PtrToStringUTF8(componentsJsonPtr);
                    entity.components = JsonConvert.DeserializeObject<AlloComponents>(componentsJson, new EntityConverter());
                    _AlloClient.allo_free(componentsJsonPtr);
                    entry = entry->le_next;
                }
                HashSet<String> existingEntityIds = new HashSet<string>(entities.Keys);
                lostEntityIds = new HashSet<string>(existingEntityIds);
                lostEntityIds.ExceptWith(incomingEntityIds);
            }
            if(onAdded != null)
            {
                foreach (string addedId in newEntityIds)
                {
                    onAdded(entities[addedId]);
                }
            }
            foreach(string removedId in lostEntityIds)
            {
                if(onRemoved != null)
                {
                    onRemoved(entities[removedId]);
                }
                entities.Remove(removedId);
            }
        }
        public void Disconnect(int reason)
        {
            unsafe
            {
                if (client != null)
                {
                    _AlloClient.alloclient_disconnect(client, reason);
                    client = null;

                    thisHandle.Free();
                }
            }
        }

        public void SpawnEntity(EntitySpecification spec)
        {
            Debug.Assert(this.avatarId != null);
            List<object> bodyO = new List<object>{"spawn_entity", spec};
            string body = JsonConvert.SerializeObject(bodyO);
            this.InteractRequest(this.avatarId, "place", body, null);
        }

        static unsafe private void _disconnected(_AlloClient* _client)
        {
            GCHandle backref = (GCHandle)_client->_backref;
            AlloClient self = backref.Target as AlloClient;
            Debug.WriteLine("_disconnected: calling delegate " + self.onDisconnected.ToString());
            self.onDisconnected?.Invoke();
            Debug.WriteLine("_disconnected: deallocating");
            self.Disconnect(-1);
        }

        static unsafe private void _interaction(_AlloClient* _client, _AlloInteraction *inter)
        {
            string type = Marshal.PtrToStringAnsi(inter->type);
            string from = Marshal.PtrToStringAnsi(inter->senderEntityId);
            string to = Marshal.PtrToStringAnsi(inter->receiverEntityId);
            string cmd = Marshal.PtrToStringAnsi(inter->body);
            string requestId = Marshal.PtrToStringAnsi(inter->requestId);

            GCHandle backref = (GCHandle)_client->_backref;
            AlloClient self = backref.Target as AlloClient;

            Debug.WriteLine("Incoming " + type + " interaction alloclient: " + from + " > " + to + ": " + cmd + ";");
            List<object> data = JsonConvert.DeserializeObject<List<object>>(cmd);

            if(from == "place" && data[0].ToString() == "announce")
            {
                self.avatarId = data[1].ToString();
                self.placeName = data[2].ToString();
                self.connected = true;
                self.onConnected?.Invoke();
            }
            
            AlloEntity fromEntity = null;
            if (!string.IsNullOrEmpty(from))
                self.entities.TryGetValue(from, out fromEntity);
            AlloEntity toEntity = null;
            if (!string.IsNullOrEmpty(to))
                self.entities.TryGetValue(to, out toEntity);

            ResponseCallback callback = null; 
            if (type == "response" && !string.IsNullOrEmpty(requestId))
            {
                self.responseCallbacks.TryGetValue(requestId, out callback);
            }

            if (callback != null)
            {
                callback(cmd);
                self.responseCallbacks.Remove(requestId);
            }
            else
            {
                self.onInteraction?.Invoke(type, fromEntity, toEntity, data);
            }
        }

        static unsafe private void _assetRequestBytes(_AlloClient* _client, IntPtr _assetId, UIntPtr offset, UIntPtr length)
        {
            GCHandle backref = (GCHandle)_client->_backref;
            AlloClient self = backref.Target as AlloClient;

            string assetId = Marshal.PtrToStringAnsi(_assetId);
            Debug.WriteLine($"Got request for asset {assetId}, length {length} at offset {offset}");
            self.onAssetBytesRequested?.Invoke(assetId, (long)offset, (long)length);
        }

        public void SendAsset(string asset_id, Byte[] data, long offset, long total_size)
        {
            unsafe
            {
                IntPtr assetIdPtr = Marshal.StringToHGlobalAnsi(asset_id);
                GCHandle pinnedData = GCHandle.Alloc(data, GCHandleType.Pinned);
                IntPtr dataPtr = pinnedData.AddrOfPinnedObject();

                _AlloClient.alloclient_asset_send(client, assetIdPtr, dataPtr, (UIntPtr)offset, (UIntPtr)data.Length, (UIntPtr)total_size);
                Marshal.FreeHGlobal(assetIdPtr);
                pinnedData.Free();
            }
        }

    }

    public class EntityConverter : JsonConverter
    {
        public override bool CanConvert(Type objectType)
        {
            return objectType == typeof(Component.Geometry);
        }
        public override object ReadJson(JsonReader reader, Type objectType, object existingValue, JsonSerializer serializer)
        {

            if(objectType == typeof(Component.Geometry))
            {
                JObject jobject = JObject.Load(reader);
                string geometryType = jobject["type"].ToString();
                if(geometryType == "inline")
                {
                    return JsonConvert.DeserializeObject<Component.InlineGeometry>(jobject.ToString());
                }
                else if(geometryType== "asset")
                {
                    return JsonConvert.DeserializeObject<Component.AssetGeometry>(jobject.ToString());
                }
                else
                {
                    return null;
                }
            }
            else
            {
                Debug.Assert(false);
                return null;
            }
        }
        public override void WriteJson(JsonWriter writer, object value, JsonSerializer serializer)
        {
            writer.WriteValue(value.ToString());
        }
    }
}
