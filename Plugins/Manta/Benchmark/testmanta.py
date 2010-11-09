from paraview.simple import *
from optparse import OptionParser
import paraview.benchmark
import math
import sys
import time

parser = OptionParser()
parser.add_option("-#", "--run_id", action="store", dest="runid",type="int",
                  default=-1, help="an identifier for this run")
parser.add_option("-f", "--family", action="store", dest="family",
                  type="choice", choices=["ICC", "GCC"],
                  help="which family of libraries to use")
parser.add_option("-r", "--rendermode",
                  type="choice", choices=["manta", "icet", "bswap"],
                  help="render mode choice should be on of manta|icet|bswap")
parser.add_option("-i", "--interactive", action="store_true", default="False", dest="interactive",
                  help="turn on when running in interactive mode")
parser.add_option("-l", "--mantathreads", action="store", dest="threads",type="int",
                  default=8, help="set number of manta threads")
parser.add_option("-p", "--mpiprocs", action="store", dest="processes",type="int",
                  default=1, help="keep a record of number of procs")
parser.add_option("-o", "--object", action="store", dest="object",
                  type="choice", choices=["cloud", "sphere"],
                  help="which object to draw")
parser.add_option("-t", "--triangles", action="store", dest="triangles",type="int",
                  default=1, help="millions of triangles to render")
parser.add_option("-w", "--windowsize", action="store", dest="windowsize",type="int",
                  default=1024, help="windows size n^2")
parser.add_option("-s", "--save-images",
                  action="store_true", dest="save_images", default=False,
                  help="Save's images to /tmp/tmpimage-* when set")

(options, args) = parser.parse_args()
 
print "########################################"
print "RUNID = ", options.runid
print "CURRENT_TIME = ", time.localtime()
print "ARGS = ", sys.argv
print "OPTIONS = ", options
print "########################################"

toolset = options.family
if toolset == None:
   toolset = "ICC"
display = options.rendermode
if display == None:
   display = "mantathreads"
interactive = options.interactive == True
windowsize = [options.windowsize, options.windowsize]
save_images = options.save_images

mantadir = "/home/Xddemar/source_code/ParaView-3.8.1/build"+toolset+"/bin/"
gdir = "/home/Xddemar/source_code/GenericViewPlugin/build"+toolset+"/"

if interactive:
    print "Connecting"
    Connect("localhost")

paraview.benchmark.maximize_logs()

if display == "manta":
    if interactive:
        LoadPlugin(mantadir+"libMantaView.so", False)
        LoadPlugin(mantadir+"libMantaView.so", True)
        view = paraview.simple._create_view("MantaIceTDesktopRenderView")
    else:
        LoadPlugin(mantadir+"libMantaView.so", True)
        view = paraview.simple._create_view("MantaBatchView")
    view.Threads = options.threads

if display == "bswap":
    if interactive:
        LoadPlugin(gdir+"libGenericView.so", False)
        LoadPlugin(gdir+"libGenericView.so", True)
        view = paraview.simple._create_view("GenericIceTDesktopRenderView")
    else:
        LoadPlugin(gdir+"libGenericView.so", True)
        view = paraview.simple._create_view("GenericBatchView")
view.ViewSize = windowsize

if options.object == "cloud":
    TS = TriangleSource()
    TS.Triangles = options.triangles
    TS.Fuzziness = 0.0
else:
    TS = Sphere()
    side=math.sqrt(options.triangles*1000000/2)
    TS.PhiResolution = side
    TS.ThetaResolution = side
dr = Show()
view = Render()
view.UseImmediateMode = 0

cam = GetActiveCamera()
for i in range(0,50):
  cam.Azimuth(3)
  Render()
  if i==2 or save_images:
     WriteImage('/tmp/bench_%d_image_%d.jpg' % (options.runid, i))

print "total Polygons:" + str(dr.SMProxy.GetRepresentedDataInformation(0).GetPolygonCount())
print "view.ViewSize:" + str(view.ViewSize)

paraview.benchmark.get_logs()
logname="/tmp/bench_" + str(options.runid) + "_rawlog.txt"
paraview.benchmark.dump_logs(logname)

print "#######"
print "CURRENT_TIME = ", time.localtime()
