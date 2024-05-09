#!/bin/bash

echo "STARTING\n"

for value in {1..3}
do
    sbatch demo.batch
    sleep 2m
    make clean
done

echo "DONE\n"