#!/bin/bash

echo "STARTING"

export POPLAR_ENGINE_OPTIONS='{"autoReport.all":"true", "autoReport.directory":"./report"}'

example = '{"autoReport.directory":"./fout"}'

for task in {1..3}
do
    for dim in {2, 4, 8}
    do
        export POPLAR_ENGINE_OPTIONS=$example
        sleep 2s
        sbatch demo.batch
        sleep 5m
        make refresh
    done
done

echo "DONE"