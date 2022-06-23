#!/usr/bin/bash
set -e
maike2
dir=$(mktemp -d)
: > ../data/slopedir.log
items=('ural_north' 'ural_south' 'scandinavian_north' 'scandinavian_south' 'alps' 'karakoram' 'himalaya_west' 'himalaya_central' 'himalaya_east')
outputs=()

for item in "${items[@]}"; do
	echo Processing $item >> ../data/slopedir.log
	file_pair=../data/$item.tif','../data/${item}_mask.data
	outputs+=(../data/${item}_slopedir.txt)
	__targets/slopedir $file_pair > ../data/${item}_slopedir.txt
	echo "" >> ../data/slopedir.log
done

./plot_slopedir.py $dir/slask.pdf "${outputs[@]}" >> ../data/slopedir.log
pdf2ps $dir/slask.pdf $dir/slask.ps
ps2pdf $dir/slask.ps ../data/slopedir.pdf
