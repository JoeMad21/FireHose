#!/bin/bash

echo "STARTING"

export POPLAR_ENGINE_OPTIONS='{"autoReport.all":"true", "autoReport.directory":"./report"}'

for task in {1..3}
do
    for dim in {2, 4, 8}
    do
        export POPLAR_ENGINE_OPTIONS='{"autoReport.directory":"./report_${task}_${dim}"}'
        sbatch demo.batch
        sleep 5m
        make refresh
    done
done

echo "DONE"