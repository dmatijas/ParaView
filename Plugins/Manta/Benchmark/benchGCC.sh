#!/bin/sh


echo "RUN NUMBER=${DDM_RUNNUM}"

echo "ENV"
env

source ${HOME}/openmpipaths.sh
numnodes=$(grep -c ^ $PBS_NODEFILE)
let procs=numnodes
let procs*=$DDM_PROCS
echo "MPIENV"
cat $PBS_NODEFILE

echo "RUNNING PROGRAM"
bm="${HOME}/source_code/ParaView-3.8.1/buildGCC/bin/pvbatch ${HOME}/source_code/bench_scripts/testmanta.py -f GCC -p $procs -l $DDM_THREADS -r $DDM_RENDERMODE -o $DDM_OBJECT -t $DDM_TRIS -w $DDM_SIZE -# $DDM_RUNNUM"
echo "COMMAND"
echo $bm

mpirun -np $procs --hostfile $PBS_NODEFILE $bm

mv /tmp/bench* ${HOME}/tmp/run${DDM_RUNNUM}

echo "BYE"
