find . -name "*rawlog*" -exec /Builds/ParaView/devel/build/bin/pvpython /Source/Work/DeMarle/SC10/INTEL/results/analyze.py {} \; > & results.txt
