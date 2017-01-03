let g:buftabline_numbers = 1
let g:is_bash = 1
let g:netrw_dirhistmax = 0
let g:netrw_list_hide = '^\.git$'

hi link BufTabLineActive Comment
hi link BufTabLineCurrent Normal
hi link BufTabLineFill Comment
hi link BufTabLineHidden Comment

" This keeps brackets embedded in parentheses from being highlighted as an
" error in C files.
hi def link cErrInParen Normal
