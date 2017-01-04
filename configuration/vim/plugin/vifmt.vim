" Configure format script based on the current settings for the buffer.
function! ConfigureFormatScript()
    if !empty(matchstr("^(java|c|cpp|php)$", &l:filetype))
        let l:format = "c"
    elseif !len(&filetype) || &filetype == "markdown" ||
\     &filetype == "gitcommit"
        let l:format = "plain"
    else
        let l:format = "generic"
    endif
    let &l:formatprg = "vifmt"
\                    . " -v FORMAT=" . l:format
\                    . " -v TAB_SIZE=" . &l:tabstop
\                    . " -v TEXT_WIDTH=" . &l:textwidth
endfunction

autocmd BufEnter * call ConfigureFormatScript()
autocmd InsertLeave * call ConfigureFormatScript()
