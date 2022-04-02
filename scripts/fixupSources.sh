#!/bin/zsh

for file in ../tests/src/**/*.png; do 
	echo fixup $file
	kram fixup -srgb -i $file;
done
