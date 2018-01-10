#!/bin/bash


val=$(grep '(\*write)' $1 | awk '{print $1}')

val=''

if [[ $val == "" ]]; then
	echo 'aaaaaaaaaaaaaaaaaaaaaaaa'
	val='ssize_t'
fi

sed -i -e "s/static .* hi1616_dfx_write/static ${val} hi1616_dfx_write/g" ./proc/proc.c

