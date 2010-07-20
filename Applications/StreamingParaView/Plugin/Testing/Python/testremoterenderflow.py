#exercises the expected flow of data in remote rendering

from paraview import servermanager

plugin_fname = "/Builds/ParaView/CSStreaming/buildAll/bin/libStreamingPlugin.dylib"
connection = servermanager.Connect("localhost")
servermanager.LoadPlugin(plugin_fname, remote=1)
servermanager.LoadPlugin(plugin_fname, remote=0)

pxm = servermanager.ProxyManager()

#make a simple pipeline that mirrors what streaming app makes

#the data processing portion
reader = pxm.NewProxy("sources", "XMLImageDataReader")
reader.SetServers(0x01)
reader.GetProperty("FileName").SetElement(0,"/Users/demarle/tmp/wavelet_8.vti")
reader.UpdateProperty("FileName",1)

geomfilter = pxm.NewProxy("filters", "GeometryFilter")
geomfilter.SetServers(0x01)
geomfilter.AddInput(0, reader, 0, "SetInputConnection")

prop = geomfilter.GetProperty("UseOutline")
prop.SetElement(0,1)
geomfilter.UpdateProperty("UseOutline",1)

#the display portion
#PCF
pcf = pxm.NewProxy("filters", "PieceCache")
pcf.SetServers(0x01)
pcf.AddInput(0, geomfilter, 0, "SetInputConnection") #WHY DOES THIS MAKE OUTPUT EMPTY?
#pcf.AddInput(0, reader, 0, "SetInputConnection")

#SUS
sus = pxm.NewProxy("filters", "StreamingUpdateSuppressor")
sus.SetServers(0x01)
#sustoplevel = servermanager._getPyProxy(sus)
sus.AddInput(0, pcf, 0, "SetInputConnection")
#sustoplevel.PieceCacheFilter = pcf

#make a prop that I can call to inspect US with
#printpropR = servermanager.vtkSMProperty()
#printpropR.SetCommand("PrintMe")
#sus.AddProperty("printme", printpropR)
printpropR = sus.GetProperty("PrintMe")
printpropR.Modified()
sus.UpdateProperty("PrintMe",1)

#don't let this one intercept and stop updates
prop = sus.GetProperty("Enabled")
prop.SetElement(0,0)
sus.UpdateProperty("Enabled")

#make up a property and use it to set the number of passes
newprop = servermanager.vtkSMProperty()
prop = sus.GetProperty("SetNumberOfPasses")
prop.SetElement(0,4)
sus.UpdateProperty("SetNumberOfPasses")

print "MADE ABC"

#MPIMoveData
MD = pxm.NewProxy("filters", "StreamingMoveData")
MD.SetServers(0x15)
xprop = MD.GetProperty("MoveMode")
xprop.SetElement(0,0) #passthrough - for server side rendering
MD.UpdateProperty("MoveMode",1)

#configure output type
prop = MD.GetProperty("OutputDataType")
prop.SetElement(0, 0) #0=PD,4=UG,6=ID
MD.UpdateProperty("OutputDataType", 1)

#configure server side of MD. It has an input and it knows it is the server.
rMD = MD.NewInstance()
rMD.InitializeAndCopyFromProxy(MD)
rMD.SetServers(0x01)
rMD.AddInput(0, sus, 0, "SetInputConnection")
newprop = servermanager.vtkSMProperty()
newprop.SetCommand("SetServerToDataServer")
rMD.AddProperty("SetServerToDataServer", newprop)
rMD.UpdateProperty("SetServerToDataServer",1)

#configure client side of MD. It has NO input and it knows it is the client.
lMD = MD.NewInstance()
lMD.InitializeAndCopyFromProxy(MD)
lMD.SetServers(0x10)
newprop = servermanager.vtkSMProperty()
newprop.SetCommand("SetServerToClient")
lMD.AddProperty("SetServerToClient", newprop)
lMD.UpdateProperty("SetServerToClient",1)
llmd = lMD.GetClientSideObject()

print "MADE D,J"

#Parallel Piece Cache - on client ONLY
ppc = pxm.NewProxy("filters", "ParallelPieceCache")
ppc.SetServers(0x10)
ppc.AddInput(0, MD, 0, "SetInputConnection")

print "MADE L"

#PostCollect Update Suppressor
sus2 = pxm.NewProxy("filters", "StreamingUpdateSuppressor")
sus2.SetServers(0x15)
#sus2.AddInput(0, MD, 0, "SetInputConnection")

rsus2 = sus2.NewInstance()
rsus2.InitializeAndCopyFromProxy(sus2)
rsus2.SetServers(0x01)
rsus2.AddInput(0, MD, 0, "SetInputConnection")

print "MADE E"
lsus2 = sus2.NewInstance()
lsus2.InitializeAndCopyFromProxy(sus2)
lsus2.SetServers(0x10)
lsus2.AddInput(0, ppc, 0, "SetInputConnection")

print "MADE M"
printpropL = sus2.GetProperty("PrintMe")
printpropL.Modified()
sus2.UpdateProperty("PrintMe", 1)

#make up a property and use it to set the number of passes
prop = sus2.GetProperty("SetNumberOfPasses")
prop.SetElement(0,4)
sus2.UpdateProperty("SetNumberOfPasses")

#Set up an initial viewpoint
#camera located at 50,50,1000
cprop = servermanager.vtkSMDoubleVectorProperty()
cprop.SetCommand("SetCameraState")
cprop.SetNumberOfElements(9)
#cprop = lsus2.GetProperty("SetCamera")
cprop.SetElement(0,50)
cprop.SetElement(1,50)
cprop.SetElement(2,1000)
#camera up direction 0,1,0
cprop.SetElement(3,0)
cprop.SetElement(4,1)
cprop.SetElement(5,0)
#camera pointed at 0,0,0
cprop.SetElement(6,0)
cprop.SetElement(7,0)
cprop.SetElement(8,0)
cprop.SetArgumentIsArray(1)
lsus2.AddProperty("SetCamera", cprop)
lsus2.UpdateProperty("SetCamera",1)

print "DEFINED CAMERA"

#frustum setup to surround just two of the eight pieces
cprop =servermanager.vtkSMDoubleVectorProperty()
cprop.SetCommand("SetFrustum")
cprop.SetNumberOfElements(32)
cprop.SetArgumentIsArray(1)
frust = [2,2,100,1, 2,2,-100,1, 2,100,100,1, 2,100,-100,1, 100,2,100,1, 100,2,-100,1, 100,100,100,1, 100,100,-100,1]
for x in range(0,31):
  cprop.SetElement(x,frust[x])
lsus2.AddProperty("SetFrustum",cprop)
lsus2.UpdateProperty("SetFrustum",1)

print "DEFINED FRUSTUM"

printpropL.Modified()
sus2.UpdateProperty("PrintMe", 1)

#Now that pipeline is setup, exercise it
#compute the pipeline priorities and inspect
newprop = servermanager.vtkSMProperty()
newprop.SetCommand("ComputeLocalPipelinePriorities")
sus.AddProperty("compute", newprop)
sus.UpdateProperty("compute",1)
printpropR.Modified()
sus.UpdateProperty("PrintMe",1)

sprop = sus.GetProperty("Serialize")
sprop.Modified()
sus.UpdateProperty("Serialize",1)

sres = sus.GetProperty("SerializedLists")
sus.UpdatePropertyInformation(sres)
print "NUMELEMENT = "
sres.GetNumberOfElements()
print "THE LIST = "
thelist = sres.GetElement(0)

print "GIVING TO CLIENT"
psprop = sus2.GetProperty("UnSerialize")
psprop.SetElement(0, thelist)
sus2.UpdateProperty("UnSerialize", 1)
sus2.UpdateProperty("PrintPieceLists",1)

print "VIEW SORT"
#compute the view dependent priorities and inspect
newprop = servermanager.vtkSMProperty()
newprop.SetCommand("ComputeGlobalViewPriorities")
sus2.AddProperty("computeView", newprop)
sus2.UpdateProperty("computeView",1)
sus2.UpdateProperty("PrintPieceLists",1)

print "SEND TO SERVER"
sus2.UpdateProperty("Serialize", 1)
sprop = sus2.GetProperty("SerializedLists")
sus2.UpdatePropertyInformation(sprop)
sprop.GetNumberOfElements()
thelist2 = sprop.GetElement(0)

#send them to another SUS that lives on the (root node of the) server
#either is fine:
#{
#psprop = sus2.GetProperty("UnSerialize")
#psprop.SetElement(0, thelist2)
#sus2.UpdateProperty("UnSerialize")
#sus2.UpdateProperty("PrintPieceLists",1)
#}
#{
psprop = servermanager.vtkSMStringVectorProperty()
psprop.SetCommand("UnSerializeLists")
psprop.SetNumberOfElements(1)
psprop.SetElement(0, thelist2)
rsus2.AddProperty("UnSerialize", psprop)
rsus2.UpdateProperty("UnSerialize", 1)
newprop = servermanager.vtkSMProperty()
newprop.SetCommand("PrintPieceLists")
rsus2.AddProperty("PrintPieceLists", newprop)
rsus2.UpdateProperty("PrintPieceLists", 1)
#}

print "UPDATE NOW"
#finally run it through a few passes
newprop = servermanager.vtkSMIntVectorProperty()
newprop.SetNumberOfElements(2)
newprop.SetCommand("SetPassNumber")
newprop.SetElement(0,0)
newprop.SetElement(1,4)
rsus2.AddProperty("PassNumber", newprop)
rsus2.UpdateProperty("PassNumber",1)

upprop = servermanager.vtkSMProperty()
upprop.SetCommand("ForceUpdate")
upprop.Modified();
rsus2.AddProperty("FU", upprop)
rsus2.UpdateProperty("FU",1)

result = servermanager.Fetch(sus2)
print result.GetNumberOfPoints()
print result.GetBounds();

#test if append works
newprop = servermanager.vtkSMProperty()
newprop.SetCommand("AppendPieces")
pcf.AddProperty("Append", newprop)
pcf.UpdateProperty("Append",1)

newprop = servermanager.vtkSMIntVectorProperty()
newprop.SetCommand("SetUseAppend")
newprop.SetElement(0,1)
rsus2.AddProperty("UseAppend", newprop)
rsus2.UpdateProperty("UseAppend",1)
rsus2.UpdateProperty("FU",1)

result = servermanager.Fetch(sus2)
print result.GetNumberOfPoints()
print result.GetBounds();
