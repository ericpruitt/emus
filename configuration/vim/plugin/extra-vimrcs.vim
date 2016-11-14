" Source additional configuration files matching the pattern "~/.*.vim".
for path in split(glob("~/.*.vim"), "\n")
    if filereadable(path)
        exec "source" path
    endif
endfor
