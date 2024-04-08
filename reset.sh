#!/bin/bash

for file in ./results*.txt; do
  : > "$file"
done

for file in ./input*.txt; do
  : > "$file"
done