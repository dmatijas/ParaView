print "This test exercises the flow of data in built in streaming."
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
then pvpython execfile(\"this script\")
"""
from paraview.simple import *

plugin_fname = "/Builds/ParaView/devel/streaming_clientside_cache/bin/libStreamingPlugin.dylib"
servermanager.LoadPlugin(plugin_fname)

pxm = servermanager.ProxyManager()

print "making a pipeline that mirrors what streaming app makes"

print ""

print "make the data processing portion of the pipeline"

print "make the reader and update it"
reader = XMLImageDataReader()
reader.FileName = "/Users/demarle/tmp/wavelet.8.vti"
realReader = reader.SMProxy.GetClientSideObject()
realReader.Update()
#print realReader.GetOutput()

print "make the geometry filter"
geomfilter = GeometryFilter()
gproxy = geomfilter.SMProxy

print ""

print "make the display portion of the pipeline"
print "PCF"
pcf = pxm.NewProxy("filters", "PieceCache")
pcftoplevel = servermanager._getPyProxy(pcf) #so can access it as if in .simple
pcf.AddInput(0, gproxy, 0, "SetInputConnection")

print "SUS"
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
print "set number of passes"
newprop = servermanager.vtkSMProperty()
prop = sus.GetProperty("SetNumberOfPasses")
prop.SetElement(0,8)
sus.UpdateProperty("SetNumberOfPasses")

print "set up an initial viewpoint"
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

print ""

print "Now exercise the pipeline"

print ""
print "compute the pipeline priorities and inspect"
newprop = servermanager.vtkSMProperty()
newprop.SetCommand("ComputeLocalPipelinePriorities")
sus.AddProperty("compute", newprop)
sus.UpdateProperty("compute",1)
#printprop.Modified()
#sus.UpdateProperty("printme",1)
print "Should see cache misses and then see a list of 8 pieces with proper "
print "bounding boxes, all with priority of (1 1 1) since nothing to reject"

print "compute the view dependent priorities and inspect"
newprop = servermanager.vtkSMProperty()
newprop.SetCommand("ComputeLocalViewPriorities")
sus.AddProperty("computeView", newprop)
sus.UpdateProperty("computeView",1)
#printprop.Modified()
#sus.UpdateProperty("printme",1)
print "should see two pieces (7/8 and 3/8) that lie in the frustum, the rest culled"

print "now run it through two passes"
sustoplevel.PassNumber = [0,8]
sus.ForceUpdate()
print realsus.GetOutput().GetNumberOfPoints()
print realsus.GetOutput().GetBounds()
print "should see piece 7/8 with 726 polys pupdated and put in the cache"

sustoplevel.PassNumber = [1,8]
sus.ForceUpdate()
print realsus.GetOutput().GetNumberOfPoints()
print realsus.GetOutput().GetBounds()
print "should see piece 3/8 with 726 polys pupdated and put in the cache"

print "test if append works"
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
print "should get one piece with 1452 verts"
