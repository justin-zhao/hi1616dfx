#!/bin/sh


val=$(grep '(\*write)' $1 | awk '{print $1}')


sed -i -e "s/static .* hi1616_dfx_write/static ${val} hi1616_dfx_write/g" ./proc/proc.c

