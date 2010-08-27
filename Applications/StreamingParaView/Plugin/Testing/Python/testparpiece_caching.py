print "This test is used to verify that the parallel piece cache filter has "
print "control over pipeline executions, can cache individual pieces that are "
print "given to it, and that it appends the various inputs together into a single "
print "polydata"
print "To run: run pvpython and execfile(\"this script\")"

from paraview import servermanager
from paraview import vtk

if servermanager.ActiveConnection == None:
    connection = servermanager.Connect()

fname = "/Builds/ParaView/devel/streaming_clientside_cache/bin/libStreamingPlugin.dylib"
servermanager.LoadPlugin(fname)

pxm = servermanager.ProxyManager()

print "Make a programmable source that produces pieces of synthetic polydata in a"
print "multiblock dataset"
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
print "Each time the python filter executes it produces a two piece multipiece"
print "data set. Both are polydata, flagged as being piece 0/4 on their respective"
print "processor."
print "Both pieces have exactly one VTK_VERTEX cell, which sits at -1,0,? for the"
print "first and print 1,0,? for the second."
print "The filter changes ?, the z coordinate every time it runs and that number "
print "is also found in the point centered array called execcount"

pys.UpdatePipeline()
lpys = pys.GetClientSideObject()
print "OUTPUT OF Programmable filter is"
print lpys.GetOutput().GetClassName()
print(lpys.GetOutput())
print "The above should have two datasets with one vert, the first at"
print "-1,0,0 and the other at 1,0,0."

print "Create the parallel piece cache filter that I am trying to test"
ppc = pxm.NewProxy("filters", "ParallelPieceCache")
lppc = ppc.GetClientSideObject()
ppc.AddInput(0, pys, 0, "SetInputConnection")
ppc.UpdatePipeline()
lppc.Update()

print "print its output"
print "OUTPUT OF PPCF IS"
print lppc.GetOutput().GetClassName()
print lppc.GetOutput()
print("BOUNDS ARE")
print(lppc.GetOutput().GetBounds())
pdaout = lppc.GetOutput().GetPointData().GetArray("execcount")
print ("execcount is " + str(pdaout.GetValue(0)) + " " + str(pdaout.GetValue(1)))
print "This should be a single (merged) polydata consisting of two VTK_VERTEX cells"
print "and have bounds of -1..1,0,0"

lppc.Update()
print "OUTPUT OF PPCF IS"
print lppc.GetOutput().GetClassName()
print("BOUNDS ARE")
print(lppc.GetOutput().GetBounds())
pdaout = lppc.GetOutput().GetPointData().GetArray("execcount")
print ("execcount is " + str(pdaout.GetValue(0)) + " " + str(pdaout.GetValue(1)))
print "Since the source was NOT modified, the PPCF should return exactly what it"
print "did before"

lpys.Modified()
lppc.Update()
print "OUTPUT OF PPCF IS"
print lppc.GetOutput().GetClassName()
print("BOUNDS ARE")
print(lppc.GetOutput().GetBounds())
pdaout = lppc.GetOutput().GetPointData().GetArray("execcount")
print ("execcount is " + str(pdaout.GetValue(0)) + " " + str(pdaout.GetValue(1)))
print "Since the source WAS modified, the PPCF should discard what is in the case"
print "and return a new data set like the one above, but incremented in z and value"

print("SET PIECES TO REQUEST");
ppc.GetProperty("MakeDummyList")
prop.Modified()
ppc.UpdateProperty("MakeDummyList",1)
print "The PPC will now look for pieces {proc=0,pn=0,np=4} and {proc=1,pn=0,np=4}"
print "which is exactly what we produced."

print("CHECKING IF IT HAS CONTENTS OF DUMMY LIST")
prop = ppc.GetProperty("HasRequestedPieces")
ppc.UpdatePropertyInformation(prop)
ret = prop.GetElement(0)
if ret:
    print("It HAS REQUESTED PIECES")
else:
    print("It DOES NOT HAVE PIECES")
print "It should have the requested pieces"

lpys.Modified()
lpys.Update()
print "OUTPUT OF Programmable filter is"
print lpys.GetOutput().GetClassName()
print(lpys.GetOutput())
print "The third execution of the source should move it to -1..1,0,2"

lppc.Update()
print("BOUNDS OF PPCF ARE")
print(lppc.GetOutput().GetBounds())
pdaout = lppc.GetOutput().GetPointData().GetArray("execcount")
print ("execcount is " + str(pdaout.GetValue(0)) + " " + str(pdaout.GetValue(1)))
print "Even thought he source was modified, since we are asking for specific pieces"
print "from the cache it should return the same thing again (bounds -1..1,0,1)"
