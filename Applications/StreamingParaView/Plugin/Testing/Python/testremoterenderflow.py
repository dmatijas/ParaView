print "This test exercises the flow of data in remote rendered streaming."
print "It sets up the same set of filters that paraview does to render data"
print "in builtin mode, and then sends some test data through that"
print "to run:"
print "first make some data via"
print """
pvpython
import paraview
import paraview.vtk
import paraview.vtk.imaging
import paraview.vtk.io
from paraview.vtk.imaging import *
from paraview.vtk.io import *
#source = vtkImageMandelbrotSource()
#source.SetWholeExtent(0,100,0,100,0,100)
source = vtkRTAnalyticSource()
writer = vtkXMLImageDataWriter()
writer.SetInputConnection(source.GetOutputPort())
writer.SetNumberOfPieces(8)
writer.SetFileName("/Users/demarle/tmp/wavelet.8.vti")
writer.Write()
then start a two process pvserver
then pvpython execfile(\"this script\")
"""

dooutline = False

SERVERONLY = 0x01
CLIENTONLY = 0x10
EVERYWHERE = 0x15

from paraview import servermanager

plugin_fname = "/Builds/ParaView/devel/streaming_clientside_cache/bin/libStreamingPlugin.dylib"
connection = servermanager.Connect("localhost")
servermanager.LoadPlugin(plugin_fname, remote=0)
servermanager.LoadPlugin(plugin_fname, remote=1)

pxm = servermanager.ProxyManager()

print "make a pipeline that mimics what streaming app makes"
print ""

print "the data processing portion"
print "reader"
reader = pxm.NewProxy("sources", "XMLImageDataReader")
#VTK object for this proxy will be instantiated only on server
reader.SetServers(SERVERONLY)
reader.GetProperty("FileName").SetElement(0,"/Users/demarle/tmp/wavelet.8.vti")
reader.UpdateProperty("FileName",1)

print "filter"
geomfilter = pxm.NewProxy("filters", "GeometryFilter")
geomfilter.SetServers(SERVERONLY)
geomfilter.AddInput(0, reader, 0, "SetInputConnection")
wprop = geomfilter.GetProperty("UseOutline")
if dooutline:
  prop.SetElement(0,1) #hould have 2 x8 points total (bbox render)
else:
  prop.SetElement(0,0) #should have 2 x726
geomfilter.UpdateProperty("UseOutline",1)


print ""
print "the display portion"
print "PCF"
pcf = pxm.NewProxy("filters", "PieceCache")
pcf.SetServers(SERVERONLY)
pcf.AddInput(0, geomfilter, 0, "SetInputConnection")
#pcf.AddInput(0, reader, 0, "SetInputConnection")

print "SUS"
sus = pxm.NewProxy("filters", "StreamingUpdateSuppressor")
sus.SetServers(SERVERONLY)
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
prop.SetElement(0,4) #numproc * numpasses must = numpieces in file
sus.UpdateProperty("SetNumberOfPasses")
#MADE ABC

print ""
print "make the bridge between server and client"
print "MPIMoveData"
MD = pxm.NewProxy("filters", "StreamingMoveData")
MD.SetServers(EVERYWHERE)
xprop = MD.GetProperty("MoveMode")
#THIS IS VERY IMPORTANT
#it distinquishes this test from testdeliveredflow
xprop.SetElement(0,0) #0 = passthrough - for server side rendering
MD.UpdateProperty("MoveMode",1)
#configure output type
prop = MD.GetProperty("OutputDataType")
prop.SetElement(0, 0) #0=PD,4=UG,6=ID
MD.UpdateProperty("OutputDataType", 1)

#configure server side of MD. It has an input and it knows it is the server.
rMD = MD.NewInstance()
rMD.InitializeAndCopyFromProxy(MD)
rMD.SetServers(SERVERONLY)
rMD.AddInput(0, sus, 0, "SetInputConnection")
newprop = servermanager.vtkSMProperty()
newprop.SetCommand("SetServerToDataServer")
rMD.AddProperty("SetServerToDataServer", newprop)
rMD.UpdateProperty("SetServerToDataServer",1)

#configure client side of MD. It has NO input and it knows it is the client.
lMD = MD.NewInstance()
lMD.InitializeAndCopyFromProxy(MD)
lMD.SetServers(CLIENTONLY)
newprop = servermanager.vtkSMProperty()
newprop.SetCommand("SetServerToClient")
lMD.AddProperty("SetServerToClient", newprop)
lMD.UpdateProperty("SetServerToClient",1)
llmd = lMD.GetClientSideObject()
#MADE D,J

print "Make Parallel Piece Cache on client"
ppc = pxm.NewProxy("filters", "ParallelPieceCache")
ppc.SetServers(CLIENTONLY)
ppc.AddInput(0, MD, 0, "SetInputConnection")
#MADE L

print "Make PostCollect Update Suppressor"
sus2 = pxm.NewProxy("filters", "StreamingUpdateSuppressor")
sus2.SetServers(EVERYWHERE)
#sus2.AddInput(0, MD, 0, "SetInputConnection")
#server side
rsus2 = sus2.NewInstance()
rsus2.InitializeAndCopyFromProxy(sus2)
rsus2.SetServers(SERVERONLY)
rsus2.AddInput(0, MD, 0, "SetInputConnection")
#MADE E
#client side
lsus2 = sus2.NewInstance()
lsus2.InitializeAndCopyFromProxy(sus2)
lsus2.SetServers(CLIENTONLY)
lsus2.AddInput(0, ppc, 0, "SetInputConnection")
#MADE M

print "inspect the SUS"
printpropL = sus2.GetProperty("PrintMe")
printpropL.Modified()
sus2.UpdateProperty("PrintMe", 1)

print "set the number of passes"
prop = sus2.GetProperty("SetNumberOfPasses")
prop.SetElement(0,4)
sus2.UpdateProperty("SetNumberOfPasses")

print "Set up viewpoint"
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
#DEFINED CAMERA

print "set up the view frustum"
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
#DEFINED FRUSTUM

printpropL.Modified()
sus2.UpdateProperty("PrintMe", 1)

print ""
print "Exercise the pipeline"

print "compute the pipeline priorities"
newprop = servermanager.vtkSMProperty()
newprop.SetCommand("ComputeLocalPipelinePriorities")
sus.AddProperty("compute", newprop)
sus.UpdateProperty("compute",1)
printpropR.Modified()
sus.UpdateProperty("PrintMe",1)

print "look on server, should be 8 total, 4 on each of 2 processes"
print "pieces should have sane bounds and ON priority (1,1,1) since "
print "nothing to reject yet"

print "Gather those computed priorities to the client"
sprop = sus.GetProperty("Serialize")
sprop.Modified()
sus.UpdateProperty("Serialize",1)
sres = sus.GetProperty("SerializedLists")
sus.UpdatePropertyInformation(sres)
n = sres.GetNumberOfElements()
print "NUMELEMENT = ", n
thelist = sres.GetElement(0)
print "THE LIST = ", thelist

print "give that info to the client's SUS"
psprop = sus2.GetProperty("UnSerialize")
psprop.SetElement(0, thelist)
sus2.UpdateProperty("UnSerialize", 1)
sus2.UpdateProperty("PrintPieceLists",1)
print "remotes should have four elements each"
print "local should have four."
print "priorities should all be (1 1 1)"
#??print "local's bounds should be a pairwise aggregate of same indexed pieces "
#??print "from remote lists"

print "compute view priorities on the client"
#compute the view dependent priorities and inspect
newprop = servermanager.vtkSMProperty()
newprop.SetCommand("ComputeGlobalViewPriorities")
sus2.AddProperty("computeView", newprop)
sus2.UpdateProperty("computeView",1)
sus2.UpdateProperty("PrintPieceLists",1)
print "orders should shuffle and one piece in each list should have one non zero"
print "view priority (1 ~0.08 1)"

print "send them back to the server"
sus2.UpdateProperty("Serialize", 1)
sprop = sus2.GetProperty("SerializedLists")
sus2.UpdatePropertyInformation(sprop)
sprop.GetNumberOfElements()
thelist2 = sprop.GetElement(0)
print "THE LIST =". thelist2

print "send them to the SUS that lives on the (root node of the) server"
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
print "on server you should see the same view prioritized lists as on client"

print "run the whole thing through a few passes"
newprop = servermanager.vtkSMIntVectorProperty()
newprop.SetNumberOfElements(2)
newprop.SetCommand("SetPassNumber")
newprop.SetElement(0,0) #first of for passes, the remotes will figure out
newprop.SetElement(1,4) #what piece this corresponds to
rsus2.AddProperty("PassNumber", newprop)
rsus2.UpdateProperty("PassNumber",1)

upprop = servermanager.vtkSMProperty()
upprop.SetCommand("ForceUpdate")
upprop.Modified();
rsus2.AddProperty("FU", upprop)
rsus2.UpdateProperty("FU",1)
print "should see first piece on each come through"

result = servermanager.Fetch(sus2)
print result.GetNumberOfPoints()
print result.GetBounds();
print "should have bounds of 0..10,0..10,-10..10"

print "test if append works"
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
