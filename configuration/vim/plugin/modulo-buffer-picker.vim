" Jump through buffers based on the modulus of the input. For input values
" less than 10, this function advances to the next buffer that ends in the
" same digit as the input. For example, if "n" is 5, the open buffers are [1,
" 2, 5, 7, 15] and the current buffer is 2, ModuloBufferPicker(5) will jump to
" buffer 5 then 15 then wrap around to 5 again. If "n" is greater than 10,
" buffer selection is based on multiples of the input; with a buffer list of
" [12, 13, 14, 24, 27, 36] and the current buffer is 24,
" ModuloBufferPicker(12) will jump to buffer 36 then wrap around to 12 then 24
" again.
function ModuloBufferPicker(n)
    let l:bufnr = bufnr("%")
    let l:inflection = 0

    let l:bufs = map(filter(copy(getbufinfo()), "v:val.listed"), "v:val.bufnr")
    let l:max = max(l:bufs)

    if l:bufnr > l:max
        let l:bufs = l:bufs + [l:bufnr]
    endif

    " Append the buffer number list to itself to simplify wrap-around handling.
    let l:bufs = l:bufs + l:bufs

    for l:index in l:bufs
        if !l:inflection && l:bufnr == l:index
            let l:inflection = 1
        elseif l:inflection && ((a:n <= 10 && (a:n % 10) == (l:index % 10)) ||
\         (a:n > 10 && !(l:index % a:n)))
            exec "b" . l:index
            echo
            break
        endif
    endfor
endfunction
