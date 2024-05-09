#!/bin/bash

echo "STARTING"

for value in {1..3}
do
    sbatch demo.batch
    sleep 2m
    make refresh
done

echo "DONE"