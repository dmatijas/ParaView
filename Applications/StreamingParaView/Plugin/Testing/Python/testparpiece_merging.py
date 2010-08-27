print "This test is used to verify that the parallel piece cache filter will"
print "accept a multipiece data set as input and properly cache the results."
print "To run: run pvpython and execfile(\"this script\")"

from paraview import servermanager
from paraview import vtk

ss= vtk.vtkSphereSource()
ss.SetCenter(-1,0,0)
ss.Update()
s1 = vtk.vtkPolyData()
s1.DeepCopy(ss.GetOutput())
infokey = s1.DATA_PIECE_NUMBER()
s1.GetInformation().Set(infokey, 0)
infokey = s1.DATA_NUMBER_OF_PIECES()
s1.GetInformation().Set(infokey, 2)
ss.SetCenter(1,0,0)
ss.Update()
s2 = vtk.vtkPolyData()
s2.DeepCopy(ss.GetOutput())
infokey = s2.DATA_PIECE_NUMBER()
s2.GetInformation().Set(infokey, 0)
infokey = s2.DATA_NUMBER_OF_PIECES()
s2.GetInformation().Set(infokey, 2)
mpds = vtk.vtkMultiPieceDataSet()
mpds.SetPiece(0, s1)
mpds.SetPiece(1, s2)

print(mpds)
print "The result should be a two piece multipiecedataset, containing two polydatas"
print "both should have 50 points and 96 polygons. The x axis bounds should be "
print "-1.5 to -.5 for the first and .5 to 1.5 for the second."

connection = servermanager.Connect()
fname = "/Builds/ParaView/devel/streaming_clientside_cache/bin/libStreamingPlugin.dylib"
servermanager.LoadPlugin(fname)
pxm = servermanager.ProxyManager()
pl1 = pxm.SMProxyManager.NewProxy("filters", "ParallelPieceCache")
ppcs = pl1.GetClientSideObject()
ppcs.SetInput(mpds)
ppcs.Update()
print "You should see the piece cache filter check first if it was asked for"
print "specific pieces and not find that to be the case."
print "Then it should check its cache, find that to be empty and fill it with the"
print "two polydatas generated above."

ppcs.Update()
print "The PCF should see that it has something in the cache and reuse that."

mpds.Modified()
ppcs.Update()
print "The PCF should now refill its cache with \"new\" polydata from the input"
