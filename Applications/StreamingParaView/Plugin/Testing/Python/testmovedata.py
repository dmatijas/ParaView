#execfile("/Source/ParaView/gitDevel/CSStreaming/Applications/StreamingParaView/Plugin/Testing/Python/testmovedata.py")

#This test is used to verify that the server can send pieces to the client in such a way
#that the client can reconstruct any processors piece.

#set clientrender False and only server should produce polydata
#set it true and the client should get the aggregate, as a multipiece dataset
from paraview import servermanager

clientrender = True
testStreaming = True

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
    servermanager.LoadPlugin("/Builds/ParaView/CSStreaming/buildAll/bin/libStreamingPlugin.dylib")
    servermanager.LoadPlugin("/Builds/ParaView/CSStreaming/buildAll/bin/libStreamingPlugin.dylib", 0)

pxm = servermanager.ProxyManager()

#generate some data on the server
sphereP = pxm.NewProxy("sources", "SphereSource")

#configure the MPIMOVEDATA that I am trying to test
if testStreaming:
    MD = pxm.NewProxy("filters", "StreamingMoveData")
else:
    MD = pxm.NewProxy("filters", "MPIMoveData")

MD.SetServers(0x15)

xprop = MD.GetProperty("MoveMode")
if clientrender:
    print "CLIENT RENDER"
    xprop.SetElement(0,1) #collect - for client side rendering
else:
    print "SERVER RENDER"
    xprop.SetElement(0,0) #passthrough - for server side rendering
MD.UpdateProperty("MoveMode",1)

#configure server side. It has an input and it knows it is the server.
MDserverClone = MD.NewInstance()
MDserverClone.InitializeAndCopyFromProxy(MD)
MDserverClone.SetServers(0x01)
MDserverClone.AddInput(0, sphereP, 0, "SetInputConnection")
newprop = servermanager.vtkSMProperty()
newprop.SetCommand("SetServerToDataServer")
MDserverClone.AddProperty("SetServerToDataServer", newprop)
MDserverClone.UpdateProperty("SetServerToDataServer",1)
MDserverClone.UnRegister(None)

#configure client side. It has NO input and it knows it is the client.
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
