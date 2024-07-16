#!/bin/bash

echo "STARTING"

for run in {0..3}
do
    for task in {0..3}
    do
        for dim in 2 4 8 16 32 64 128 256
        do
            # Set the directory dynamically based on task and dim
            export POPLAR_ENGINE_OPTIONS='{"autoReport.all":"true", "autoReport.directory":"./reports/r'${run}'_t'${task}'_d'${dim}'"}'
            echo "Trial ${run} Running task ${task} with dimension ${dim}"
        
            sleep 2s
        
            # Submit the job
            sbatch demo.batch ${task} ${dim}
        
            # Wait for the job to complete
            sleep 5m
        
            # Refresh the environment or application
            make refresh
        done
    done
done

echo "DONE"