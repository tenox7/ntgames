#!/bin/bash
# https://github.com/mezpusz/sokohard
mkdir -p levels
for i in $(seq 0 99); do
  num=$(printf "%02d" $i)
  width=$((2 + $i / 40))
  height=$((2 + $i / 40))
  boxes=$((2 + $i / 30))
  if [ $((i % 2)) -eq 0 ]; then
    box_changes="--box-changes"
  else
    box_changes=""
  fi
  seed=$((1000 + $i))
  echo "Generating level $num (width: $width, height: $height, boxes: $boxes)"
  ./out/sokohard -w $width -h $height -b $boxes -s $seed $box_changes -o "levels/l$num"
done
