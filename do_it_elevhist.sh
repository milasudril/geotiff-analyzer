#!/usr/bin/bash
set -e
maike2
dir=$(mktemp -d)
: > ../data/elevhist.log
items=('ural_north' 'ural_south' 'scandinavian_north' 'scandinavian_south' 'alps' 'karakoram' 'himalaya_west' 'himalaya_central' 'himalaya_east')
outputs=()

for item in "${items[@]}"; do
	echo Processing $item >> ../data/elevhist.log
	file_pair=../data/$item.tif','../data/${item}_mask.data
	outputs+=(../data/${item}_elevhist.txt)
	__targets/elev_hist $file_pair > ../data/${item}_elevhist.txt
	echo "" >> ../data/elevhist.log
done

./plot_elevhist.py $dir/slask.pdf "${outputs[@]}" >> ../data/elevhist.log
pdf2ps $dir/slask.pdf $dir/slask.ps
ps2pdf $dir/slask.ps ../data/elevhist.pdf
