#!/bin/bash


for task in {0..3}
do
    for dim in 2 4 8 16 32 64 128 256
    do
        "'./reports/report_run0''_task'${task}'_dim'${dim}'"
    done
done