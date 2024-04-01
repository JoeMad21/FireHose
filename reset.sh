#!/bin/bash

for file in ./results*.txt; do
  : > "$file"
done