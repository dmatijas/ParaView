#exercises the expected flow of data in built in streaming

from paraview.simple import *


plugin_fname = "/Builds/ParaView/CSStreaming/buildAll/bin/libStreamingPlugin.dylib"
servermanager.LoadPlugin(plugin_fname)

pxm = servermanager.ProxyManager()

#make a simple pipeline that mirrors what streaming app makes


#the data processing portion
reader = XMLImageDataReader()
reader.FileName = "/Users/demarle/tmp/wavelet_8.vti"
realReader = reader.SMProxy.GetClientSideObject()
realReader.Update()
#print realReader.GetOutput()

geomfilter = GeometryFilter()
gproxy = geomfilter.SMProxy

#the display portion
#PCF
pcf = pxm.NewProxy("filters", "PieceCache")
pcftoplevel = servermanager._getPyProxy(pcf) #so can access it as if in .simple
pcf.AddInput(0, gproxy, 0, "SetInputConnection")

#SUS
sus = pxm.NewProxy("filters", "StreamingUpdateSuppressor")
sustoplevel = servermanager._getPyProxy(sus)
sus.AddInput(0, pcf, 0, "SetInputConnection")
sustoplevel.PieceCacheFilter = pcf
realsus = sus.GetClientSideObject()

#make a prop that I can call to inspect US with
printprop = servermanager.vtkSMProperty()
printprop.SetCommand("PrintMe")
sus.AddProperty("printme", printprop)

#make up a property and use it to set the number of passes
newprop = servermanager.vtkSMProperty()
prop = sus.GetProperty("SetNumberOfPasses")
prop.SetElement(0,8)
sus.UpdateProperty("SetNumberOfPasses")

#Set up an initial viewpoint
#camera located at 50,50,1000
cprop = sus.GetProperty("SetCamera")
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
sus.UpdateProperty("SetCamera",1)
#frustum setup to surround just two of the eight pieces
cprop = sus.GetProperty("SetFrustum")
frust = [2,2,100,1, 2,2,-100,1, 2,100,100,1, 2,100,-100,1, 100,2,100,1, 100,2,-100,1, 100,100,100,1, 100,100,-100,1]
for x in range(0,31):
  cprop.SetElement(x,frust[x])
sus.UpdateProperty("SetFrustum",1)

#Now that pipeline is setup, exercise it
#compute the pipeline priorities and inspect
newprop = servermanager.vtkSMProperty()
newprop.SetCommand("ComputeLocalPipelinePriorities")
sus.AddProperty("compute", newprop)
sus.UpdateProperty("compute",1)
printprop.Modified()
sus.UpdateProperty("printme",1)

#compute the view dependent priorities and inspect
newprop = servermanager.vtkSMProperty()
newprop.SetCommand("ComputeLocalViewPriorities")
sus.AddProperty("computeView", newprop)
sus.UpdateProperty("computeView",1)
printprop.Modified()
sus.UpdateProperty("printme",1)

#now run it through a few passes
sustoplevel.PassNumber = [0,8]
sus.ForceUpdate()
print realsus.GetOutput().GetNumberOfPoints()
print realsus.GetOutput().GetBounds()

sustoplevel.PassNumber = [1,8]
sus.ForceUpdate()
print realsus.GetOutput().GetNumberOfPoints()
print realsus.GetOutput().GetBounds()

#test if append works
newprop = servermanager.vtkSMProperty()
newprop.SetCommand("AppendPieces")
pcf.AddProperty("Append", newprop)
pcf.UpdateProperty("Append",1)

newprop = servermanager.vtkSMIntVectorProperty()
newprop.SetCommand("SetUseAppend")
newprop.SetElement(0,1)
sus.AddProperty("UseAppend", newprop)
sus.UpdateProperty("UseAppend",1)
sus.ForceUpdate()
print realsus.GetOutput().GetNumberOfPoints()
print realsus.GetOutput().GetBounds()
