import sys
print sys.argv
from paraview import benchmark
benchmark.start_frame = 4
benchmark.import_logs(sys.argv[1])
benchmark.parse_logs(tabular=1)
