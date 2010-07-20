#execfile("/Source/ParaView/gitDevel/CSStreaming/Applications/StreamingParaView/Plugin/Testing/Python/testparpiececache.py")

#This test is used to verify that the client can cache and reuse lots of pieces out of the parallel piece cache filter.

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

#connection = servermanager.Connect()
fname = "C:/Documents and Settings/demarle/Desktop/Builds/ParaView/gitDevel/CSStreaming/bin/Debug/StreamingPlugin.dll"
#fname = "/Builds/ParaView/CSStreaming/buildAll/bin/libStreamingPlugin.dylib"
servermanager.LoadPlugin(fname)
pxm = servermanager.ProxyManager()
pl1 = pxm.SMProxyManager.NewProxy("filters", "ParallelPieceCache")
ppcf = pl1.GetClientSideObject()
ppcs = pl1.GetClientSideObject()
ppcs.SetInput(mpds)
ppcs.Update()

mpds.Modified()
ppcs.Update()
