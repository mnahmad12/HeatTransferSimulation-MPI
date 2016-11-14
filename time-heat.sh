#!/bin/bash

# make

# row/col dimensions for the heat matrix to run
all_dims='1000,1000 10000,1000 1000,10000 10000,10000 10000,20000 20000,10000'
all_procs='1 2 4 8'
printf "%5s %5s %2s %5s\n" "rows" "cols" "p" "time"
for dims in $all_dims; do
    # echo Running dimensions $dims
    for procs in $all_procs; do
        dims=${dims/,/ }
        # echo /usr/bin/time -p ipc_heat $dims 0 $procs
        x=`/usr/bin/time -p ipc_heat $dims 0 $procs |& gawk '/real/{print $2}'`
        printf "%5s %5s %2s %5s\n" $dims $procs $x
    done
done
