" Use Tab for completion and indenting.
function! TabOrComplete(direction)
    if col('.') > 1 && strpart(getline('.'), col('.') - 2, 3) =~ '^\w'
        if a:direction < 0
            return "\<C-P>"
        else
            return "\<C-N>"
        endif
    else
        return "\<Tab>"
    endif
endfunction

inoremap <S-Tab> <C-R>=TabOrComplete(-1)<CR>
inoremap <Tab> <C-R>=TabOrComplete(+1)<CR>
