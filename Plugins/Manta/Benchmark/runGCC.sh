ID=$1
NNODES=$2
NTRIS=$3
TLIMIT=$4
RMODE=$5
NTHREADS=$6
NPROCS=$7

mkdir ~/tmp/run${ID}
qsub -N pvGCC${ID} -l "walltime=0:${TLIMIT}:0.0 select=${NNODES}:ncpus=24:arch=wds024c" -j eo -e ~/tmp/run${ID}/outstreams.log -v \
"DDM_RUNNUM=${ID}, DDM_THREADS=${NTHREADS}, DDM_RENDERMODE=${RMODE}, DDM_OBJECT=cloud, DDM_TRIS=${NTRIS}, DDM_SIZE=1024, DDM_PROCS=${NPROCS}" \
~/source_code/runscripts/benchGCC.sh
