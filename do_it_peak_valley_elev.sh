#!/usr/bin/bash
set -e
maike2
dir=$(mktemp -d)
: > ../data/peak_valley_elev.log
items=('ural_north' 'ural_south' 'scandinavian_north' 'scandinavian_south' 'alps' 'karakoram' 'himalaya_west' 'himalaya_central' 'himalaya_east')
file_pairs=()

for item in "${items[@]}"; do
	echo Processing $item
	echo Processing $item >> ../data/peak_valley_elev.log
	file_pair=../data/$item.tif','../data/${item}_mask.data
	file_pairs+=($file_pair)
	__targets/peak_valley_elev $file_pair > ../data/${item}_peak_valley_elev.txt
	./plot_peak_valley_elev.py ../data/${item}_peak_valley_elev.txt $dir/slask.pdf >> ../data/peak_valley_elev.log
	pdf2ps $dir/slask.pdf $dir/slask.ps
	ps2pdf $dir/slask.ps ../data/${item}_peak_valley_elev.pdf
	echo "" >> ../data/peak_valley_elev.log
done

echo "Processing all"
echo "Processing all" >> ../data/elevgrad.log
__targets/peak_valley_elev "${file_pairs[@]}" > ../data/all_peak_valley_elev.txt
./plot_peak_valley_elev.py ../data/all_peak_valley_elev.txt $dir/slask.pdf >> ../data/peak_valley_elev.log
pdf2ps $dir/slask.pdf $dir/slask.ps
ps2pdf $dir/slask.ps ../data/all_peak_valley_elev.pdf
echo "" >> ../data/peak_valley_elev.log
