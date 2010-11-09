#!/bin/sh

echo "RUN NUMBER=${DDM_RUNNUM}"

echo "ENV"
env

source ${HOME}/intelpaths.sh

export I_MPI_DEVICE=rdma
nodes=$(sort -u $PBS_NODEFILE|grep -c ^)
numnodes=$(grep -c ^ $PBS_NODEFILE)
let procs=numnodes
let procs*=$DDM_PROCS

rm -rf /tmp/bench*

echo "STARTMPD"
mpdboot -r ssh -n $numnodes -f $PBS_NODEFILE

echo "RUNNING PROGRAM"
bm="${HOME}/source_code/ParaView-3.8.1/buildICC/bin/pvbatch ${HOME}/source_code/bench_scripts/testmanta.py -f ICC -p $procs -l $DDM_THREADS -r $DDM_RENDERMODE -o $DDM_OBJECT -t $DDM_TRIS -w $DDM_SIZE -# $DDM_RUNNUM"

mpiexec -nolocal -perhost $DDM_PROCS -n $procs $bm

echo "STOP MPD"
mpdallexit

mv /tmp/bench* ${HOME}/tmp/run${DDM_RUNNUM}

echo "BYE"
