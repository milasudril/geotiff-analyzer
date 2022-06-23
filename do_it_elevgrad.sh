#!/usr/bin/bash
set -e
maike2
dir=$(mktemp -d)
: > ../data/elevgrad.log
items=('ural_north' 'ural_south' 'scandinavian_north' 'scandinavian_south' 'alps' 'karakoram' 'himalaya_west' 'himalaya_central' 'himalaya_east')
file_pairs=()

for item in "${items[@]}"; do
	echo Processing $item >> ../data/elevgrad.log
	file_pair=../data/$item.tif','../data/${item}_mask.data
	file_pairs+=($file_pair)
	__targets/grad_at_points $file_pair > ../data/${item}_elevgrad.txt
	./plot_grad_data.py ../data/${item}_elevgrad.txt $dir/slask.pdf >> ../data/elevgrad.log
	pdf2ps $dir/slask.pdf $dir/slask.ps
	ps2pdf $dir/slask.ps ../data/${item}_elevgrad.pdf
	echo "" >> ../data/elevgrad.log
done

echo "Processing all" >> ../data/elevgrad.log
__targets/grad_at_points "${file_pairs[@]}" > ../data/all_elevgrad.txt
./plot_grad_data.py ../data/all_elevgrad.txt $dir/slask.pdf >> ../data/elevgrad.log
pdf2ps $dir/slask.pdf $dir/slask.ps
ps2pdf $dir/slask.ps ../data/all_elevgrad.pdf
echo "" >> ../data/elevgrad.log

