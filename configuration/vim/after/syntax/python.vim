" Description: Context-sensitive highlighting for `else` clauses.
"
"   else after for/while      -> same colour as the loop keyword   (pythonRepeat)
"   else after try/except     -> same colour as `except`           (pythonException)
"   any other else (if/else)  -> untouched                         (pythonConditional)
"
" Install as ~/.vim/after/syntax/python.vim
"
" AI Disclosure:
" - Authorship: AI-generated
" - Models: claude-sonnet-5-5 (Anthropic)
" - Scope: Entire file written by the AI to replace an earlier hand-written
"   attempt, working from the human's goal, test cases and performance feedback,
"   and checked automatically against Python's own parser on real files.
" - References: https://groups.google.com/g/vim_use/c/BJ4jfiEWR7Q/m/ao69FxAuyqQJ
"
" How it works
" ------------
" A block starts at a `for`/`while` (or `except`) and ends
"   * at an `else` with exactly the same indentation as the header, or
"   * at the first line that is dedented below the header, or that has the
"     same indentation but is not the matching `else` (for try: nor `except`).
" The `else` that ends a block is highlighted like the keyword that opened it.
" Every other `else` is not part of any block, so the base syntax file
" highlights it as before.
"
" The header's indentation is captured with \z( \) when the region starts and
" compared with \z1 in the end pattern, so any indentation width (spaces or
" tabs) and any nesting depth works, with just a handful of rules.
"
" Performance notes
" -----------------
" Vim evaluates these patterns on every line while it scrolls, so they are kept
" cheap: the start patterns are anchored with ^ (rejected at once everywhere but
" the start of a line), there is no look-behind of unbounded length, and the
" start patterns ask for Vim's backtracking regexp engine with \%#=1 (Vim 7.4 or
" later), which is about twice as fast as the default NFA engine for such short
" anchored patterns. (The end patterns use the default engine; it is faster for
" them.)

" First character of a line that counts as code. Blank lines (also those with a
" stray CR or form feed), comments (which may sit at any column) and closing
" brackets (Black puts the `)` of a multi-line `for x in f(...)` header at the
" indentation of the `for`) never end a block.
let s:code = '[^ \t\r\x0c#)\]}]'

" End patterns. Each is "plain end" or "the matching else".
"
" Indented header (\z1 is the header's indentation):
"   not deeper than the header:   \%(\z1[ \t]\)\@!
"   not the matching else:        \%(\z1else\>\)\@!
" The plain end is zero width and sits at column 0, so it never swallows the
" first word of the next line (`def`, `class`... keep their own highlighting).
" The `else` itself is matched by the END pattern (an end pattern beats the
" built-in `else` keyword) and highlighted via matchgroup.
let s:loopEnd = '\%(^\ze\%(\z1[ \t]\)\@!\%(\z1else\>\)\@![ \t]*' . s:code . '\)'
\             . '\|\%(^\z1\zselse\>\)'
let s:tryEnd  = '\%(^\ze\%(\z1[ \t]\)\@!\%(\z1\%(else\|except\)\>\)\@![ \t]*' . s:code . '\)'
\             . '\|\%(^\z1\zselse\>\)'

" Header at column 0: there is no indentation to compare with.
let s:loopEnd0 = '\%(^\ze\%(else\>\)\@!' . s:code . '\)\|\%(^\zselse\>\)'
let s:tryEnd0  = '\%(^\ze\%(\%(else\|except\)\>\)\@!' . s:code . '\)\|\%(^\zselse\>\)'

" Indented headers. The start is ^-anchored and begins at column 0, ahead of the
" keyword, so the keyword does not interfere. The [ \t]\@! after the captured
" indentation makes the (greedy) match fail at once instead of retrying with
" less indentation.
"
" contains=TOP means "every ordinary top-level group", so there is no list of
" excluded groups to maintain. There is deliberately no `keepend`: with it,
" low-indented text inside a triple-quoted string would close the block (and
" the string) in the middle of the string.
execute 'syn region pythonLoopBlock'
\   . ' start="\%#=1^\z([ \t]\+\)[ \t]\@!\%(async\s\+\)\=\%(for\|while\)\>"'
\   . ' matchgroup=pythonLoopElse end="' . s:loopEnd . '"'
\   . ' transparent contains=TOP'

execute 'syn region pythonTryBlock'
\   . ' start="\%#=1^\z([ \t]\+\)except\>"'
\   . ' matchgroup=pythonTryElse end="' . s:tryEnd . '"'
\   . ' transparent contains=TOP'

" Headers at column 0. The keyword is at column 0 as well, and a keyword beats a
" match or region that starts in the same column, so a start there would never
" fire. Instead the keyword itself hands over (nextgroup) to a zero-width region
" that starts right after it. These keywords are added to the existing groups,
" so their colour and group name are unchanged, and Vim only looks at the start
" patterns after one of these three words.
"
" `for` is 3 letters, so a `for` that ends in column 4 began in column 1;
" likewise `while` (5 letters, column 6) and `except` (6 letters, column 7). The
" short look-behind makes sure the line really starts with the keyword: Vim also
" tries these patterns inside `.attribute` matches, where the column alone is
" not enough.
syn keyword pythonRepeat    for    nextgroup=pythonLoopBlock0For
syn keyword pythonRepeat    while  nextgroup=pythonLoopBlock0While
syn keyword pythonException except nextgroup=pythonTryBlock0

execute 'syn region pythonLoopBlock0For contained start="\%#=1\%4c\%(^for\)\@3<="'
\   . ' matchgroup=pythonLoopElse end="' . s:loopEnd0 . '"'
\   . ' transparent contains=TOP'

execute 'syn region pythonLoopBlock0While contained start="\%#=1\%6c\%(^while\)\@5<="'
\   . ' matchgroup=pythonLoopElse end="' . s:loopEnd0 . '"'
\   . ' transparent contains=TOP'

execute 'syn region pythonTryBlock0 contained start="\%#=1\%7c\%(^except\)\@6<="'
\   . ' matchgroup=pythonTryElse end="' . s:tryEnd0 . '"'
\   . ' transparent contains=TOP'

" The colours: an `else` that closes a block looks exactly like the keyword that
" opened it, whatever the colour scheme does with those groups. The first two
" lines are fallbacks, only used if the base syntax file did not already define
" the groups.
hi def link pythonRepeat    Repeat
hi def link pythonException Exception
hi def link pythonLoopElse  pythonRepeat
hi def link pythonTryElse   pythonException

unlet s:code s:loopEnd s:tryEnd s:loopEnd0 s:tryEnd0
