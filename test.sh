#!/bin/bash

echo "STARTING"

for task in {1..3}
do
    for dim in 2, 4, 8
    do
        # Set the directory dynamically based on task and dim
        export POPLAR_ENGINE_OPTIONS='{"autoReport.all":"true", "autoReport.directory":"./report_task'${task}'_dim'${dim}'"}'
        echo "Running task ${task} with dimension ${dim}"
        
        sleep 2s
        
        # Submit the job
        sbatch demo.batch
        
        # Wait for the job to complete
        sleep 5m
        
        # Refresh the environment or application
        make refresh
    done
done

echo "DONE"