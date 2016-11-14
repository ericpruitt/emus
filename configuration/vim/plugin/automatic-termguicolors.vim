" Set termguicolors based on whether or not it appears the current color scheme
" has true-color support.
function! DetectRGBColorscheme()
    redir => _
        silent! highlight Normal
    redir END
    silent! let &termguicolors = match(_, "guifg=[^N]") != -1
    unlet _
endfunction

autocmd ColorScheme * call DetectRGBColorscheme()
call DetectRGBColorscheme()
