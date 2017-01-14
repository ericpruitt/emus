" Configure format script based on the current settings for the buffer.
function! ConfigureFormatScript()
    if !empty(matchstr("^(java|c|cpp|php)$", &l:filetype))
        let l:format = "c"
    elseif !len(&filetype) ||
\     !empty(matchstr("^(gitcommit|markdown|text)$", &l:filetype))
        let l:format = "plain"
    else
        let l:format = "generic"
    endif
    let &formatprg = "vifmt"
\                  . " -v FORMAT=" . l:format
\                  . " -v MARKDOWN=" . (&filetype == "markdown")
\                  . " -v TAB_STOP=" . &l:tabstop
\                  . " -v TEXT_WIDTH=" . &l:textwidth
endfunction

autocmd BufEnter * call ConfigureFormatScript()
autocmd InsertLeave * call ConfigureFormatScript()
