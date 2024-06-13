#!/bin/bash

for file in ./IPU_INPUTS*.out; do
  : > "$file"
done

for file in ./IPU_OUTPUTS*.out; do
  : > "$file"
done