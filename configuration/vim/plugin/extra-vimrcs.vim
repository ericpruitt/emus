" Source additional configuration files matching the pattern "~/.*.vimrc".
for path in split(glob("~/.*.vimrc"), "\n")
    if filereadable(path)
        exec "source" path
    endif
endfor
