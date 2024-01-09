#!/usr/bin/env bash
for code in 1 2 3 4 5 8 8 9 "38;2;40;100;255" "48;2;40;100;255"; do
    printf '\033[%smAAA BBB CCC DDD EEE\nFFF GGG HHH III JJJ\033[m\n' "$code"
done
tput sgr0
