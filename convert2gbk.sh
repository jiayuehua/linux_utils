#!/bin/sh
mkdir -p $2

cd $1

for i in $(ls)
do
  iconv -f utf8 -t gbk $i > $2/$i
done
