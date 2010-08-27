print "This test is verifies that the client can get data in a multipiece format"
print "from there is can get at any particular piece that the parallel server "
print "sends to it"
print "To run: start up a two process pvserver, then run pvpython and"
print "execfile(\"this script\")"
print "set clientrender to true or false to exercise client side and remote "
print "rendering respectively"
print "set testStreaming to true or false to compare streaming delivery "
print "(client makes multipiece) verse normal delivery (client makes polydata)"

from paraview import servermanager

clientrender = True
testStreaming = False

if clientrender:
    print "CLIENT RENDER"
else:
    print "SERVER RENDER"

if testStreaming:
    print "USING STREAMING MoveData"
else:
    print "USING STANDARD MoveData"

#connection = servermanager.Connect();
connection = servermanager.Connect("localhost")

if testStreaming:
    servermanager.LoadPlugin("/Builds/ParaView/devel/streaming_clientside_cache/bin/libStreamingPlugin.dylib", 0)
    servermanager.LoadPlugin("/Builds/ParaView/devel/streaming_clientside_cache/bin/libStreamingPlugin.dylib", 1)

pxm = servermanager.ProxyManager()

print "generate some data on the server"
sphereP = pxm.NewProxy("sources", "SphereSource")

print "configure the MPIMOVEDATA that I am trying to test"
if testStreaming:
    MD = pxm.NewProxy("filters", "StreamingMoveData")
else:
    MD = pxm.NewProxy("filters", "MPIMoveData")
#the move data needs to be instantiated on both the server and client"
MD.SetServers(0x15)
xprop = MD.GetProperty("MoveMode")
if clientrender:
    xprop.SetElement(0,1) #collect - for client side rendering
else:
    xprop.SetElement(0,0) #passthrough - for server side rendering
MD.UpdateProperty("MoveMode",1)

#configure instance that lives on the server. It has an input and it knows it
#is the server.
MDserverClone = MD.NewInstance()
MDserverClone.InitializeAndCopyFromProxy(MD)
MDserverClone.SetServers(0x01)
MDserverClone.AddInput(0, sphereP, 0, "SetInputConnection")
newprop = servermanager.vtkSMProperty()
newprop.SetCommand("SetServerToDataServer")
MDserverClone.AddProperty("SetServerToDataServer", newprop)
MDserverClone.UpdateProperty("SetServerToDataServer",1)
MDserverClone.UnRegister(None)

#configure instance that lives on the client. It has NO input and it knows it
#is the client.
MDclientClone = MD.NewInstance()
MDclientClone.InitializeAndCopyFromProxy(MD)
MDclientClone.SetServers(0x10)
newprop = servermanager.vtkSMProperty()
newprop.SetCommand("SetServerToClient")
MDclientClone.AddProperty("SetServerToClient", newprop)
MDclientClone.UpdateProperty("SetServerToClient",1)
MDclientClone.UnRegister(None)

#tell pipeline to run
MD.UpdatePipeline()

#examine the outputs
newprop = servermanager.vtkSMProperty()
newprop.SetCommand("PrintMe")
MD.AddProperty("PrintMe", newprop)
MD.UpdateProperty("PrintMe",1)

outformat = ""
if testStreaming:
    outformat = "multipiece"
else:
    outformat = "polydata"

if clientrender:
    print "server's output should be aggregated to the root node"
    print "client should get the aggregate as a", outformat, "dataset"
else:
    print "server should produce balanced polydata"
    print "client should produce an empty", outformat, "dataset"
