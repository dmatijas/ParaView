#This test is used to verify that the client can cache and reuse lots of pieces out of the parallel piece cache filter.

from paraview import servermanager
from paraview import vtk

if servermanager.ActiveConnection == None:
    connection = servermanager.Connect()

#fname = "C:/Documents and Settings/demarle/Desktop/Builds/ParaView/gitDevel/CSStreaming/bin/Debug/StreamingPlugin.dll"
fname = "/Builds/ParaView/CSStreaming/buildAll/bin/libStreamingPlugin.dylib"
servermanager.LoadPlugin(fname)

pxm = servermanager.ProxyManager()

#a programmable source that produces pieces of synthetic polydata in a multiblock dataset
pys = pxm.NewProxy("sources", "ProgrammableSource")
pys.GetProperty("OutputDataSetType").SetElement(0,25)
pys.UpdateProperty("OutputDataSetType",1)

prop = pys.GetProperty("Script")
prop.SetElement(0,"""
#a static int to catch re-executions
global execcount
try:
   execcount = execcount + 1
except:
   execcount = 0
print("UPSTREAM IS EXECUTING " + str(execcount))

mpds = self.GetOutput();
mpds.SetNumberOfPieces(2)

pd = vtk.vtkPolyData()
pts = vtk.vtkPoints()
pts.InsertNextPoint(-1,0,execcount)
pd.SetPoints(pts)
pda = vtk.vtkPointData()
array = vtk.vtkIntArray()
array.SetNumberOfComponents(1)
array.SetName("execcount")
array.InsertNextValue(execcount)
pda = pd.GetPointData()
pda.AddArray(array)
verts = vtk.vtkCellArray()
verts.InsertNextCell(1)
verts.InsertCellPoint(0)
pd.SetVerts(verts)
infokey = pd.DATA_PIECE_NUMBER()
pd.GetInformation().Set(infokey, 0)
infokey = pd.DATA_NUMBER_OF_PIECES()
pd.GetInformation().Set(infokey, 4)

mpds.SetPiece(0, pd)

pd = vtk.vtkPolyData()
pts = vtk.vtkPoints()
pts.InsertNextPoint(1,0,execcount)
pd.SetPoints(pts)
pda = vtk.vtkPointData()
array = vtk.vtkIntArray()
array.SetNumberOfComponents(1)
array.SetName("execcount")
array.InsertNextValue(execcount)
pda = pd.GetPointData()
pda.AddArray(array)
verts = vtk.vtkCellArray()
verts.InsertNextCell(1)
verts.InsertCellPoint(0)
pd.SetVerts(verts)
infokey = pd.DATA_PIECE_NUMBER()
pd.GetInformation().Set(infokey, 0)
infokey = pd.DATA_NUMBER_OF_PIECES()
pd.GetInformation().Set(infokey, 4)

mpds.SetPiece(1, pd)
""")
pys.UpdateProperty("Script",1)

pys.UpdatePipeline()
lpys = pys.GetClientSideObject()
#print(lpys)

#print(lpys.GetOutput())

#the parallel piece cache filter that I am trying to test
ppc = pxm.NewProxy("filters", "ParallelPieceCache")
lppc = ppc.GetClientSideObject()
ppc.AddInput(0, pys, 0, "SetInputConnection")
ppc.UpdatePipeline()
#lppc.SetInputConnection(lpys.GetOutputPort(0))
lppc.Update()

print("BOUNDS ARE")
print(lppc.GetOutput().GetBounds())
pdaout = lppc.GetOutput().GetPointData().GetArray("execcount")
print ("execcount is " + str(pdaout.GetValue(0)) + " " + str(pdaout.GetValue(1)))

lpys.Modified()
lppc.Update()
print("BOUNDS ARE")
print(lppc.GetOutput().GetBounds())
pdaout = lppc.GetOutput().GetPointData().GetArray("execcount")
print ("execcount is " + str(pdaout.GetValue(0)) + " " + str(pdaout.GetValue(1)))

print("SET PIECES TO REQUEST");
ppc.GetProperty("MakeDummyList")
prop.Modified()
ppc.UpdateProperty("MakeDummyList",1)
#lppc.MakeDummyList()

print("CHECKING IF IT HAS CONTENTS OF DUMMY LIST")
prop = ppc.GetProperty("HasRequestedPieces")
ppc.UpdatePropertyInformation(prop)
ret = prop.GetElement(0)
if ret:
    print("It HAS REQUESTED PIECES")
else:
    print("It DOES NOT HAVE PIECES")

lpys.Modified()
lppc.Update()
print("BOUNDS ARE")
print(lppc.GetOutput().GetBounds())
pdaout = lppc.GetOutput().GetPointData().GetArray("execcount")
print ("execcount is " + str(pdaout.GetValue(0)) + " " + str(pdaout.GetValue(1)))

lpys.Modified()
lppc.Update()
print("BOUNDS ARE")
print(lppc.GetOutput().GetBounds())
pdaout = lppc.GetOutput().GetPointData().GetArray("execcount")
print ("execcount is " + str(pdaout.GetValue(0)) + " " + str(pdaout.GetValue(1)))
