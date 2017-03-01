colorscheme ron
silent! colorscheme d3contrast
filetype plugin indent off
syntax on

set autoindent
set backspace=indent,eol,start
set cursorline
set display=lastline
set encoding=utf8
set expandtab
set fileformat=unix
set fillchars+=vert:│
set hidden
set hlsearch
set ignorecase
set incsearch
set laststatus=0
set lazyredraw
set list
set listchars=tab:▷\ ,trail:⋅,nbsp:⋅
set modeline
set nobackup
set nofoldenable
set nojoinspaces
set nonumber
set noswapfile
set nowritebackup
set ruler
set sections=
set shiftwidth=4
set shortmess+=I
set smartcase
set softtabstop=4
set t_vb=""
set tabstop=4
set timeout timeoutlen=1000 ttimeoutlen=50
set viminfo=
set virtualedit=block
set visualbell
set wildmenu

silent! let &t_8b="\e[48;2;%lu;%lu;%lum"
silent! let &t_8f="\e[38;2;%lu;%lu;%lum"

cabbrev Q qa
cabbrev X x
inoremap qq <Esc>
nnoremap <C-k> :b#<CR>
nnoremap <silent> <C-l> :nohlsearch<CR><C-l>
nnoremap Q <Esc>
nnoremap Y y$
vnoremap / <Esc>/\%V
vnoremap ? <Esc>?\%V
vnoremap i I

if !&diff
    autocmd BufEnter * if exists("b:view") | call winrestview(b:view) | endif
    autocmd BufLeave * let b:view = winsaveview()
endif

autocmd FileType gitcommit setlocal textwidth=72 spell
autocmd FileType make,gitconfg,go,gitconfig setlocal noexpandtab
autocmd FileType markdown setlocal formatprg=
autocmd InsertEnter * set nohlsearch
autocmd InsertLeave * file
autocmd InsertLeave * set hlsearch
autocmd Syntax * syntax sync fromstart

set wildignore+=*.o,*.pyc,*.swp,*~,*.jpg,*.jpeg,*.png,*.gif,*.luac,*.manifest,
\*.bmp,__pycache__,*.tif,.DS_Store,Thumbs.db,*.pywc,*.exe,*.dll,*.class,*.jar,
\*.xcf,*.pdf

" Emacs-style bindings for command mode.
cnoremap <C-a> <Home>
cnoremap <C-b> <Left>
cnoremap <C-d> <Del>
cnoremap <C-e> <End>
cnoremap <C-f> <Right>
cnoremap <C-h> <BS>
cnoremap <C-n> <Down>
cnoremap <C-p> <Up>

" Emacs-style bindings for insert mode.
inoremap <C-a> <C-o>^
inoremap <C-b> <Left>
inoremap <C-d> <Del>
inoremap <C-e> <End>
inoremap <C-f> <Right>
inoremap <C-h> <BS>
inoremap <C-k> <C-o>C
inoremap <C-y> <C-r>=substitute(tr(@@, "\n", " "), "\\s\\+$", " ", "")<CR>
inoremap <C-Left> <C-o>b
inoremap <C-Right> <C-o>e<C-o><Space>

" Make global marks the default by remapping lowercase marks to uppercase.
for i in range(char2nr("a"), char2nr("z"))
    exec "nnoremap m" . nr2char(i) . " m" . toupper(nr2char(i))
    exec "nnoremap '" . nr2char(i) . " '" . toupper(nr2char(i))
    exec "vnoremap m" . nr2char(i) . " m" . toupper(nr2char(i))
    exec "vnoremap '" . nr2char(i) . " '" . toupper(nr2char(i))
endfor
unlet i

" When searching for text, change the value of scrolloff so there is always
" some context visible. When using "/" and "?", the scrolloff is reset to 0
" using updatetime since adding text after those characters in the mapping
" would pollute the prompt.
autocmd CursorHold * if &scrolloff | set scrolloff=0 updatetime=0 | endif
nnoremap / :set scrolloff=3 updatetime=1<CR>/
nnoremap ? :set scrolloff=3 updatetime=1<CR>?
for s:c in ["#", "g#", "*", "g#", "N", "n"]
    exec "nnoremap " . s:c ." :set scrolloff=3<CR>"
\                    . s:c . ":set scrolloff=0<CR>"
\                          . ":echo (v:searchforward ? '/' : '?') . @/<CR>"
endfor

" Replacement for autochdir that doesn't require netbeans_intg or sun_workshop
" compile-time options.
autocmd BufEnter * silent! lcd %:p:h

" Select the last changed text or the text that was just pasted
nnoremap gp `[v`]

" Make :q run :qa
cabbrev q <C-r>=getcmdtype() == ":" && getcmdpos() == 1 ? "qa" : "q"<CR>

" Use :E as an abbreviation for :e prepopulated with the expansion of $PWD.
cabbrev E e <C-r>=getchar(1) == 32 && getchar(0) ? $PWD . "/" : ""<CR>

" Make * and # searches case sensitive by default.
nnoremap * /\C\<<C-R>=expand("<cword>")<CR>\><CR>
nnoremap # ?\C\<<C-R>=expand("<cword>")<CR>\><CR>

" Remap gw to gq-equivalent so it uses formatprg
nnoremap gw{ gq{''
nnoremap gw} gq}''
