" jq colors
au BufNewFile,BufRead .jqcolors,jqcolors
\ setf dircolors |
\ syntax clear dircolorsKeyword |
\ syntax keyword dircolorsKeyword
\   STRING ARRAY OBJECT FIELD NULL FALSE TRUE NUMBER
