#!/bin/bash

echo "STARTING"

make

for value in {1..3}
do
    sbatch demo.batch
    sleep 2m
    make refresh
done

echo "DONE"