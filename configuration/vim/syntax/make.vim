" Vim syntax file
" Language: Make (POSIX, GNU and BSD)
" Canonical Repository: https://github.com/slopvat/make.vim
"
" Highlights Makefiles with an understanding of the shell code they contain:
" "$$" escapes, shell strings, variables and command substitutions are
" highlighted the way the shell will eventually see them, and an unterminated
" quote or parenthesis can only affect the logical line it appears on. In the
" body of a "define", where a reference can continue onto later lines, one
" that's never closed is flagged at the "endef".
"
" AI Disclosure:
" - Authorship: AI-generated
" - Models: claude-opus-5-5 (Anthropic)
" - Scope: Wrote the syntax file from scratch based on the user's requirements
"   and answers to design questions.
" - References: https://github.com/vim/vim/blob/master/runtime/syntax/make.vim
"   (0124320)
"
" Every pattern starts with "\%#=1" to select the backtracking regular
" expression engine. The NFA engine allocates memory for each search, and in
" Vim builds that use an allocator like musl's, that made each search an
" order of magnitude slower. Patterns also start with a literal character or
" "^" whenever possible: the backtracking engine skips ahead to the first
" occurrence of a leading literal and only tries patterns starting with "^" at
" the start of a line, but it tries a leading lookbehind at every column. For
" the same reason, end patterns that look for the end of a line or a comment
" start with ".*\zs" or "[^#]*\zs", which find it in one attempt. Runs of
" ordinary characters are matched with "\+", which the engine handles much
" faster than an alternation repeated for each character.

if exists("b:current_syntax")
  finish
endif

let s:cpo_save = &cpo
set cpo&vim

" Lets "-include" and "-load" be matched as keywords, and keeps keywords from
" matching inside words like "clean-if" and "unexport.out", which Make doesn't
" treat as directives either.
syn iskeyword @,48-57,_,192-255,-,.,/

" Pieces of patterns used to recognize the different kinds of lines. All of
" them begin at the start of a line, optionally after Autoconf or Automake
" substitutions like "@am__fastdepCC_TRUE@". The substitutions are part of
" the match rather than being skipped with "\zs": after an item that starts
" earlier on the line, Vim searches for the next match from where that item
" ended, and a pattern starting with "^" can't match there.
let s:bol = '^\%(\%(@\h\w*@\)\+\s*\|\s*\zs\)'
" Text with up to four levels of balanced parentheses or braces, which may
" continue onto other lines, keyed by the opening character, and the same
" with up to eight levels in s:balancedDeep. The runs of ordinary characters
" are atomic ("\@>") so that a reference that isn't closed doesn't make the
" engine try every way of splitting them up. The deeper version is used where
" a reference nested too deeply would be flagged as an error. The shorter one
" is used everywhere else since it's repeated in so many patterns that the
" deeper one would make the syntax file noticeably slower to load.
let s:balanced = {}
let s:balancedDeep = {}
for [s:open, s:close] in [['(', ')'], ['{', '}']]
  let s:text = '[^' . s:open . s:close . ']*'
  for s:depth in range(1, 8)
    let s:text = '\%(\%([^' . s:open . s:close . '\\]\+\)\@>'
      \ . '\|\\\n\|\\\|' . s:open . s:text . s:close . '\)*'
    if s:depth == 4
      let s:balanced[s:open] = s:text
    endif
  endfor
  let s:balancedDeep[s:open] = s:text
endfor
unlet s:open s:close s:depth s:text
" The "endef" that ends a "define", and a lookbehind for the start of the
" line after one. An "endef" after a line ending with a backslash doesn't
" count since Make joins the two lines.
let s:endef = '\%( [ \t]*\)\=endef\>'
let s:after_endef = '\%(^\%(\\\n\)\@2<!' . s:endef . '.*\n\)\@<='
" A variable reference with up to five levels of nesting.
let s:ref = '\$\%((' . s:balanced['('] . ')'
  \ . '\|{' . s:balanced['{'] . '}\|[^({]\)'
" The same in the body of a "define", where references escaped with "$$" are
" included and "$$" isn't otherwise allowed before "(" or "{", so a ":" or
" "=" in a deferred function call isn't mistaken for the end of a target or
" variable name. It's slower, so it's only used there.
let s:refDef = '\%(\$\$\=\%((' . s:balanced['('] . ')'
  \ . '\|{' . s:balanced['{'] . '}\)\|\$[^({$]\|\$\$[({]\@!\)'
" Assignment operators, including GNU Make's "::=" and ":::=" and OpenBSD's
" combinations like "?!=", "+!!=", "+:=" and ":sh=". Other makes only treat
" the character before "=" as part of the operator, so "+:=" and "?:=" aren't
" recognized after another "+" or "?", which keeps names like "libc++" in
" "libc++:=" intact.
let s:op = '\%(:sh\|:\{1,3}\|[+?]\=!\{,2}\|!\{1,2}?\|[+?]\@1<![+?]:\)='
" The name of a variable being assigned, and the targets of a rule, which
" may continue onto other lines, in s:var and s:targets, and their versions
" for "define" bodies in s:varDef and s:targetsDef. "+", "?" and "!" are
" allowed in variable names, but not where an operator starts. Quotes
" aren't allowed in targets outside of references so that a line like "echo
" 'Targets:'" in a canned recipe isn't taken for a rule, and white space is
" only part of the targets when another target follows it.
"
" Both are atomic ("\@>"): when one isn't followed by an operator, a shorter
" one wouldn't be either, and backtracking a character at a time to find
" that out made these patterns, which are tried at the start of nearly every
" line, the slowest ones in the file.
for s:suffix in ['', 'Def']
  let s:var{s:suffix} = '\%(\%([^[:space:]:#=$+?!]\+\|\%(' . s:op
    \ . '\)\@![+?!]\|' . s:ref{s:suffix} . '\)\+\)\@>'
  let s:target = '\%([^[:space:]:#=$&''"\\]\+\|\\\n\@!\|&:\@!\|'
    \ . s:ref{s:suffix} . '\)'
  let s:targets{s:suffix} = '\%(' . s:target . '\%(' . s:target
    \ . '\|\%(\s\|\\\n\)\+\%([^[:space:]:#=&''"\\]\|\\\n\@!\|&:\@!\)\@='
    \ . '\)*\)\@>'
endfor
unlet s:suffix s:target
" The operator may be on a continuation line after the name.
let s:assign = '\%(\s\|\\\n\)*' . s:op
" The operator that follows a rule's targets.
let s:ruleop = '\s*&\=::\=[:=]\@!'
" Whether this is a file that configure processes, e.g. Makefile.in.
let s:autoconf = expand('%:e') =~# '^\%(in\|am\)$'
" Lines that keep a rule's recipe going: recipe lines, comments, blank lines
" and conditionals, including Autoconf's.
let s:inrule = '\%(@\h\w*@\)*\%(\t\|[ \t]*\%(#\|$\)\)'
  \ . '\|[ \t]*\%(ifn\=eq\|ifn\=def\|else\|endif\)\>'
  \ . '\|\.\s*\%(\%(el\)\=if\|else\>\|endif\>\|for\>\|endfor\>\)'
  \ . (s:autoconf ? '\|@\%(end\)\=if\s' : '')
" Lines in a rule's recipe that are indented with spaces instead of a tab and
" aren't valid Make lines either, which GNU Make reports as a "missing
" separator": they don't start with a directive, comment or function call,
" and they don't have a ":" or "=" outside of a reference. Lines starting
" with Autoconf substitutions are never flagged since there's no telling what
" they'll be replaced with, e.g. "@am__include@" becomes "include".
let s:misindented = ' [ \t]*\%(\%(ifn\=eq\|ifn\=def\|else\|endif'
  \ . '\|-\=include\|sinclude\|export\|unexport\|override\|private'
  \ . '\|undefine\|vpath\|-\=load\|define\|endef\)\>'
  \ . '\|\$[({]\l[a-z-]*[ \t]\|#\)\@!\S\@='
  \ . '\%([^:=#$\\]\|\\.\|\\$\|' . s:ref . '\)\+\%(#.*\)\=$'

let s:functions = join([
  \ 'abspath', 'addprefix', 'addsuffix', 'and', 'basename', 'call', 'dir',
  \ 'error', 'eval', 'file', 'filter', 'filter-out', 'findstring',
  \ 'firstword', 'flavor', 'foreach', 'guile', 'if', 'info', 'intcmp', 'join',
  \ 'lastword', 'let', 'notdir', 'or', 'origin', 'patsubst', 'realpath',
  \ 'shell', 'sort', 'strip', 'subst', 'suffix', 'value', 'warning',
  \ 'wildcard', 'word', 'wordlist', 'words'
  \ ], '\|')

" Functions whose arguments use "%" in patterns.
let s:pattern_functions = 'patsubst\|filter\%(-out\)\='
" Functions whose names aren't also shell commands or keywords, so "$$(" in
" front of one in a variable value is a call deferred until the value is
" expanded again, e.g. by $(eval ...), not a command substitution.
let s:deferred_functions = join([
  \ 'abspath', 'addprefix', 'addsuffix', 'and', 'call', 'error',
  \ 'findstring', 'firstword', 'flavor', 'foreach', 'info', 'intcmp',
  \ 'lastword', 'let', 'notdir', 'or', 'origin', 'subst', 'suffix', 'value',
  \ 'warning', 'wildcard', 'word', 'wordlist', 'words', s:pattern_functions
  \ ], '\|')

let s:special_targets = join([
  \ 'BEGIN', 'DEFAULT', 'DELETE_ON_ERROR', 'END', 'ERROR', 'EXEC',
  \ 'EXPORT_ALL_VARIABLES', 'IGNORE', 'INCLUDES', 'INTERMEDIATE', 'INTERRUPT',
  \ 'JOIN', 'LIBS', 'LOW_RESOLUTION_TIME', 'MADE', 'MAIN', 'MAKE',
  \ 'MAKEFLAGS', 'META', 'MFLAGS', 'NOMETA', 'NOMETA_CMP', 'NOPATH',
  \ 'NOREADONLY', 'NOTINTERMEDIATE', 'NOTMAIN', 'NOTPARALLEL', 'NO_PARALLEL',
  \ 'NULL', 'OBJDIR', 'ONESHELL', 'OPTIONAL', 'ORDER',
  \ 'PATH\%(\.[^[:space:]:]*\)\=', 'PHONY', 'POSIX', 'PRECIOUS', 'READONLY',
  \ 'SCCS_GET', 'SECONDARY', 'SECONDEXPANSION', 'SHELL', 'SILENT', 'STALE',
  \ 'SUFFIXES', 'USE', 'USEBEFORE', 'WAIT'
  \ ], '\|')

" Clusters -------------------------------------------------------------------

" Vim decides whether an item can contain a pattern by walking the item's
" "contains" list and, recursively, the clusters in it, for every pattern
" each time it looks for the next match. That took a fifth of the time spent
" highlighting, so the items in each cluster are kept in s:clusters, and the
" items that contain clusters are defined with "Syn", which replaces
" references like "@makeExpansions" with the items they stand for.
let s:clusters = {}
" A backslash that continues a line, or one that was probably meant to but
" is followed by white space.
let s:clusters.makeLineEnd = 'makeLineCont,makeContError'
" Things Make expands wherever they appear. References are checked for being
" unterminated, but not inside other references, where the outermost one is
" checked instead. In the body of a "define", where references can continue
" onto later lines, the ones that aren't closed on their own line are
" followed to their end instead (see makeRefLines).
let s:clusters.makeNestedExpansions = 'makeRef,makeCall,makeShellCall,'
  \ . 'makeEscDollar' . (s:autoconf ? ',makeConfig' : '')
let s:clusters.makeExpansions = s:clusters.makeNestedExpansions
  \ . ',makeRefError'
let s:clusters.makeDefExpansions = s:clusters.makeNestedExpansions
  \ . ',makeRefLines'
" References and function calls escaped with "$$".
let s:clusters.makeEscExpansions = 'makeEscRef,makeEscCall,makeEscAutoVar'
" What can come where a shell command starts.
let s:clusters.makeShCommand = 'makeShConditional,makeShRepeat,'
  \ . 'makeShCommandLineCont'
" Shell syntax outside of strings, with and without backticks, which are
" separate so a backtick can't start a nested command substitution where it
" should end one, and shell code that isn't nested in an expansion. There's
" a version of each for "define" bodies, where "$$(...)" is a reference.
for [s:suffix, s:expansions, s:cmdsub] in [
  \ ['', 'makeExpansions', 'makeShCmdSub,makeShArith'],
  \ ['Def', 'makeDefExpansions', s:clusters.makeEscExpansions]]
  let s:clusters['makeShWords' . s:suffix] = s:clusters[s:expansions] . ','
    \ . s:clusters.makeLineEnd . ',makeShEscape,makeShVar,makeShParam'
    \ . s:suffix . ',' . s:cmdsub . ',makeShSQ' . s:suffix . ',makeShDQ'
    \ . s:suffix . ',makeShSeparator,makeShRefLineCont'
  let s:clusters['makeShell' . s:suffix] = s:clusters['makeShWords'
    \ . s:suffix] . ',makeShBacktick' . s:suffix
  let s:clusters['makeShTop' . s:suffix] = s:clusters['makeShell'
    \ . s:suffix] . ',makeShParenSeparator'
endfor
unlet s:suffix s:expansions s:cmdsub
" The lines in the body of a "define".
let s:clusters.makeDefineLines = s:clusters.makeDefExpansions . ','
  \ . s:clusters.makeEscExpansions . ',' . s:clusters.makeLineEnd
  \ . ',makeComment,makeTargetDef,makeVariableDef,makeModifierDef,'
  \ . 'makeRecipeDef,makeConditional,makeInclude,makeStatement,'
  \ . 'makeBSDConditional,makeDefine'

function! s:Expand(command) abort
  return substitute(a:command, '\%#=1@\(make\w*\)',
    \ '\=s:clusters[submatch(1)]', 'g')
endfunction
" A buffer-local command so that it can't replace one the user defined.
command! -buffer -nargs=+ Syn exe 'syn' s:Expand(<q-args>)

" Comments, escapes and substitutions ----------------------------------------

syn keyword makeTodo contained TODO FIXME XXX
" A comment, which a trailing backslash continues onto the next line.
Syn region makeComment start=/\%#=1#/ skip=/\%#=1\\$/ end=/\%#=1.*\zs$/
  \ keepend contains=makeTodo,@makeLineEnd,@Spell
" A backslash that continues a line. One that's followed by white space,
" "makeContError", is defined with the shell code so it takes priority over
" shell escapes.
syn match makeLineCont /\%#=1\\$/
syn match makeEscDollar /\%#=1\$\$/
" Autoconf substitutions like "@CC@" are only recognized in the files that
" configure processes, e.g. Makefile.in; elsewhere, text like "@VERSION@" is
" more likely to be a placeholder a recipe replaces. Lines like "@if
" gcc-bootstrap" and "@endif gcc-bootstrap", which GCC and Binutils use even
" in recipes, tell configure which lines to keep.
if s:autoconf
  syn match makeConfig /\%#=1@\h\w*@/
  syn match makeConfigIf /\%#=1^@\%(end\)\=if\s.*/
endif

" Variable references and function calls -------------------------------------

" Make counts nested parentheses (or braces) in a reference without regard to
" shell quoting, so references contain transparent regions that do the same.
" References never extend past the end of a logical line. Their end patterns
" check for a backslash at the end of the line themselves rather than using a
" skip pattern: inside a "keepend" region, Vim searches for the end again
" after a contained backslash, where a skip pattern can no longer match it.
" They also use "excludenl": otherwise, an end pattern that matches the end
" of a line makes the item that contains the reference continue onto the
" next line too, e.g. when a reference in a "define" continues onto later
" lines.
"
" Regions whose delimiters could be mistaken for contained items use
" "matchgroup" since contained items are otherwise also matched inside the
" text of the start and end patterns.
syn match makeRef /\%#=1\$[^({$]/
Syn region makeRef matchgroup=makeRef
  \ start=/\%#=1\$(/ excludenl end=/\%#=1)\|\_$\%(\\\)\@1<!/
  \ contains=@makeNestedExpansions,@makeEscExpansions,@makeLineEnd,makeParens
Syn region makeRef matchgroup=makeRef
  \ start=/\%#=1\${/ excludenl end=/\%#=1}\|\_$\%(\\\)\@1<!/
  \ contains=@makeNestedExpansions,@makeEscExpansions,@makeLineEnd,makeBraces
syn region makeParens contained transparent
  \ start=/\%#=1(/ excludenl end=/\%#=1)\|\_$\%(\\\)\@1<!/
syn region makeBraces contained transparent
  \ start=/\%#=1{/ excludenl end=/\%#=1}\|\_$\%(\\\)\@1<!/

" References and function calls escaped with "$$" so they're expanded later,
" e.g. by "$(eval ...)" or ".SECONDEXPANSION". These are recognized in the
" body of a "define", in prerequisites and inside other references; the
" automatic variables are defined with the shell code so they take priority
" over shell variables.
Syn region makeEscRef contained matchgroup=makeRef
  \ start=/\%#=1\$\$(/ excludenl end=/\%#=1)\|\_$\%(\\\)\@1<!/
  \ contains=@makeNestedExpansions,@makeEscExpansions,@makeLineEnd,makeParens
Syn region makeEscCall contained transparent matchgroup=makeRef
  \ start=/\%#=1\$\$(\ze\l[a-z-]*[ \t]/
  \ excludenl end=/\%#=1)\|\_$\%(\\\)\@1<!/
  \ contains=makeCallName,@makeNestedExpansions,@makeEscExpansions,
  \@makeLineEnd,makeParens
exe 'Syn region makeEscCall contained transparent matchgroup=makeRef'
  \ 'start=/\%#=1\$\$(\ze\%(' . s:pattern_functions . '\)[ \t]/'
  \ 'excludenl end=/\%#=1)\|\_$\%(\\\)\@1<!/'
  \ 'contains=makeCallName,@makeNestedExpansions,@makeEscExpansions,'
  \ . '@makeLineEnd,makePattern,makeParens'

" Function calls take on the color of their surroundings, e.g. a recipe's.
" Anything shaped like a call is treated as one, but only the names of GNU
" Make's functions are highlighted: checking for the names in the start
" patterns would slow down every variable reference. "%" is only highlighted
" in the arguments of the functions that take patterns.
for [s:open, s:close, s:nest] in [['(', ')', 'makeParens'],
                                \ ['{', '}', 'makeBraces']]
  let s:end = 'excludenl end=/\%#=1' . s:close . '\|\_$\%(\\\)\@1<!/'
  let s:contains = 'contains=makeCallName,@makeNestedExpansions,'
    \ . '@makeEscExpansions,@makeLineEnd,' . s:nest
  exe 'Syn region makeCall transparent matchgroup=makeRef'
    \ 'start=/\%#=1\$' . s:open . '\ze\l[a-z-]*[ \t]/' s:end s:contains
  exe 'Syn region makeCall transparent matchgroup=makeRef'
    \ 'start=/\%#=1\$' . s:open . '\ze\%(' . s:pattern_functions . '\)[ \t]/'
    \ s:end s:contains . ',makePattern'
  exe 'Syn region makeShellCall transparent matchgroup=makeRef'
    \ 'start=/\%#=1\$' . s:open . '\zeshell[ \t]/' s:end
    \ 'contains=makeShellCallName,@makeShell,' . s:nest
endfor
unlet s:open s:close s:nest s:end s:contains
exe 'syn match makeCallName contained'
  \ '/\%#=1\%(\$[({]\)\@2<=\%(' . s:functions . '\)[ \t]\@=/'
" The name in "$(shell ...)", after which a command starts.
Syn match makeShellCallName contained
  \ /\%#=1shell\%(\$[({]shell\)\@7<=[ \t]\@=/
  \ nextgroup=@makeShCommand skipwhite

" References that aren't closed by the end of their logical line, which is
" checked for up to nine levels of nesting. This is defined after the other
" references so it takes priority.
exe 'syn region makeRefError'
  \ 'start=/\%#=1\$\%((\%(' . s:balancedDeep['('] . ')\)\@!\|{\%('
  \ . s:balancedDeep['{'] . '}\)\@!\)/ skip=/\%#=1\\$/'
  \ 'excludenl end=/\%#=1.*\zs$/'

" Shell code -----------------------------------------------------------------

" Keywords are only recognized where a command can start, so "echo done"
" doesn't highlight "done". That's after an operator, after a keyword that
" precedes a command or ends a compound command, at the start of a recipe
" line, a variable's value, a command substitution or a subshell, and at the
" start of a continuation line when the previous line ended with one of
" those. A continuation line after a Make reference is also included: a
" reference at the end of a line, like Automake's "$(am__cd) \", usually
" expands to a complete command.
"
" Parentheses and closing braces only count as operators outside of command
" substitutions and parameter expansions since contained items would
" otherwise preempt the closing delimiters of the regions that track nesting.
Syn match makeShSeparator contained transparent /\%#=1[;&|{]/
  \ nextgroup=@makeShCommand skipwhite
Syn match makeShParenSeparator contained transparent /\%#=1[()}]/
  \ nextgroup=@makeShCommand skipwhite
" The start of a command after "`", "$$(" or the operator of an assignment.
" It also matches after "(" and "=" elsewhere in them, e.g. "--opt=if",
" which is rarely followed by a keyword. When the "nextgroup" of a
" zero-width match like this one doesn't match, Vim doesn't look for other
" items at the same column, so it only matches before white space, a
" backslash at the end of a line or a lowercase letter, which nothing else
" starts with. The character before it is leading context ("lc=1") rather
" than a lookbehind, which would be tried at every column.
Syn match makeShCommandStart contained transparent
  \ /\%#=1[`(=]\ze\%([ \t]\|\\$\|\l\)/lc=1
  \ nextgroup=@makeShCommand skipwhite
" In Autoconf files, a continuation includes substitutions at the start of
" the next line, like "@am__fastdepCC_TRUE@", so that the command after them
" can start with a keyword.
exe 'Syn match makeShCommandLineCont contained /\%#=1\\'
  \ . (s:autoconf ? '\%(\n\%(@\h\w*@\)\+\|$\)' : '$') . '/'
  \ 'contains=makeConfig nextgroup=@makeShCommand skipwhite skipnl'
Syn match makeShRefLineCont contained /\%#=1\\\%([)}][ \t]*\\\)\@40<=$/
  \ nextgroup=@makeShCommand skipwhite skipnl
Syn keyword makeShConditional contained if then else elif fi esac
  \ nextgroup=@makeShCommand skipwhite
syn keyword makeShConditional contained case
  \ nextgroup=makeShCaseWord skipwhite
Syn keyword makeShRepeat contained while until do done
  \ nextgroup=@makeShCommand skipwhite
syn keyword makeShRepeat contained for nextgroup=makeShForName skipwhite
syn match makeShForName contained transparent /\%#=1\h\w*/
  \ nextgroup=makeShIn skipwhite
Syn match makeShCaseWord contained transparent /\%#=1\S\+/
  \ contains=@makeShWords nextgroup=makeShIn skipwhite
syn keyword makeShIn contained in

" A backslash before "$$" escapes the dollar sign the shell will see; before
" "$(...)" it escapes the first character of the expansion, not the "$".
syn match makeShEscape contained /\%#=1\\\%(\$\$\|[^$]\)/
" In Autoconf files, "$$@NAME@" is a "$" followed by a substitution rather
" than the shell's "$@".
exe 'syn match makeShVar contained /\%#=1\$\$\%(\$\$\|[*#?!0-9-]\|\h\w*\|@'
  \ . (s:autoconf ? '\%(\h\w*@\)\@!' : '') . '\)/'
" "$$?" is left alone since it's commonly the shell's exit status.
syn match makeEscAutoVar contained /\%#=1\$\$[@<^+|*%]/
syn region makeShBraces contained transparent start=/\%#=1{/ end=/\%#=1}/
Syn region makeShCmdSub contained matchgroup=makeShCmdSubDelim
  \ start=/\%#=1\$\$(/ end=/\%#=1)/
  \ contains=makeShCommandStart,@makeShell,makeShParens
syn region makeShParens contained transparent start=/\%#=1(/ end=/\%#=1)/
" Arithmetic expansions, which are defined after command substitutions so
" they take priority.
Syn region makeShArith contained matchgroup=makeShCmdSubDelim
  \ start=/\%#=1\$\$((/ end=/\%#=1))/
  \ contains=@makeExpansions,@makeLineEnd,makeShVar,makeShParam,makeShCmdSub,
  \makeShBacktick,makeShParens

" Deferred function calls in variable values, which are defined after shell
" command substitutions so they take priority. "%" is only highlighted in
" the arguments of the functions that take patterns.
for [s:names, s:pattern] in [[s:deferred_functions, ''],
                           \ [s:pattern_functions, ',makePattern']]
  exe 'Syn region makeDeferredCall contained transparent matchgroup=makeRef'
    \ 'start=/\%#=1\$\$(\ze\%(' . s:names . '\)[ \t]/'
    \ 'excludenl end=/\%#=1)\|\_$\%(\\\)\@1<!/'
    \ 'contains=makeCallName,@makeNestedExpansions,@makeEscExpansions,'
    \ . '@makeLineEnd,makeParens' . s:pattern
endfor
unlet s:names s:pattern

" Escapes in double-quoted strings.
syn match makeShDQEscape contained /\%#=1\\\%(\$\$\|[\\"`]\)/

" A shell comment ends at the end of the physical line even when Make
" continues the logical line; the shell never sees the backslash-newline.
Syn match makeShComment contained /\%#=1#\%([^[:space:];&|()]#\)\@2<!.*/
  \ contains=@makeExpansions,makeTodo,@makeLineEnd
Syn match makeShCommentAfterPrefix contained /\%#=1#.*/
  \ contains=@makeExpansions,makeTodo,@makeLineEnd

syn match makeContError /\%#=1\\\s\+$/

" Recipes --------------------------------------------------------------------

" The tab at the start of a recipe line, which is followed by a command and
" optionally prefixes like "@" or a reference like "$(Q)" or Autoconf
" substitution like "@ECHO_CC@" that expands to one. Continuation lines,
" which commonly start with options like "-o", are excluded.
exe 'Syn match makeRecipeStart contained transparent'
  \ '/\%#=1^\%(\\\n\)\@2<!\%(@\h\w*@\)*\t\%(\$\%((\h\w*)\|{\h\w*}\)\ze\S'
  \ . (s:autoconf ? '\|\%(@\h\w*@\)\+' : '') . '\)\=/'
  \ 'contains=makeConfig,makeRef'
  \ 'nextgroup=makeRecipePrefix,@makeShCommand skipwhite'
Syn match makeRecipePrefix contained /\%#=1[@+-]\%([ \t]*[@+-]\)*/
  \ nextgroup=makeShCommentAfterPrefix,makeShPrefixKeyword,@makeShCommand
  \ skipwhite
" Vim only looks for keywords after a character that can't be part of a
" word, and "-" can be so that "-include" is a keyword, so keywords right
" after a "-" prefix are matched separately. Matches like this are too slow
" to use after every operator: Vim searches the rest of the line for them.
" The "-" is leading context ("lc=1") so that the engine skips ahead to the
" next "-" instead of trying a lookbehind at every column.
Syn match makeShPrefixKeyword contained
  \ /\%#=1-\%(if\|while\|until\|for\|case\)\>/lc=1
  \ nextgroup=@makeShCommand,makeShForName skipwhite
exe 'syn match makeMisindented contained /\%#=1^' . s:misindented . '/'

" Directive arguments --------------------------------------------------------

" The arguments of directives, which end at a comment or the end of the
" logical line. Defined before assignments, which take priority after
" "export" and similar keywords.
"
" Zero-width regions in a "nextgroup" can start before the white space that
" "skipwhite" skips, so they start with "\S" to keep from preempting items
" that follow the white space. They don't start at a "#" either: if an empty
" region ended where it started, Vim would skip the comment. Vim ignores
" regions with zero-width starts like these if they have a "nextgroup".
Syn region makeDirArgs contained start=/\%#=1\ze[^[:space:]#]/
  \ skip=/\%#=1\\\%([\\#]\|$\)\|\$\%(\$\_$\@!\|[({][^)}#]*#\)/
  \ end=/\%#=1[^#]*\zs\%(\ze#\|$\)/ keepend
  \ contains=@makeExpansions,@makeEscExpansions,@makeLineEnd,makePattern,
  \makeString,makeAngleString
Syn region makeString contained start=/\%#=1"/ end=/\%#=1"/ oneline
  \ contains=@makeExpansions
Syn region makeString contained start=/\%#=1'/ end=/\%#=1'/ oneline
  \ contains=@makeExpansions
" File names like ".include <bsd.prog.mk>", which aren't recognized in the
" expressions of BSD conditionals, where "<" and ">" compare numbers.
Syn match makeAngleString contained /\%#=1<[^>]*>/ contains=@makeExpansions

syn match makePattern contained /\%#=1%/
syn match makeRuleOp contained /\%#=1:/
syn match makeOrderOnly contained /\%#=1|/
" Special targets aren't recognized at the end of a word like "foo.PHONY"
" unless it's an Autoconf substitution like "@MAINT@".
exe 'syn match makeSpecialTarget contained'
  \ '/\%#=1\.\%([^[:space:]@]\.\)\@2<!\%(' . s:special_targets . '\)\>/'

" Rules, assignments and their shell code ------------------------------------

" These items are defined twice: once for most of the Makefile and once, with
" "Def" appended to their names, for the bodies of "define" directives, where
" "$$(...)" is a reference for "$(eval ...)" rather than a command
" substitution. Vim can't make an item behave differently depending on the
" region it's in, so everything that can contain one has a copy.
"
" In the template, "{S}" is replaced with the suffix, "{CONTAINED}" with
" "contained" for the copies of top-level items, "{CMDSUB}" with the group for
" "$$(...)", "{ESCAPED}" with the escaped references for single-quoted
" strings, "{EXPANSIONS}" with the things Make expands and the other names in
" braces with pieces of patterns, and then clusters are expanded. Lines
" starting with a backslash continue the previous line.
let s:template =<< trim END
  syn region makeShParam{S} contained matchgroup=makeShParam
    \ start=/\%#=1\$\${/ end=/\%#=1}/ contains=@makeShell{S},makeShBraces
  syn region makeShBacktick{S} contained matchgroup=makeShCmdSubDelim
    \ start=/\%#=1`/ skip=/\%#=1\\[\\`]/ end=/\%#=1`/
    \ contains=makeShCommandStart,@makeShWords{S}
  " Make still expands variables in shell strings, even single-quoted ones.
  syn region makeShSQ{S} contained start=/\%#=1'/ end=/\%#=1'/
    \ contains={EXPANSIONS},@makeLineEnd{ESCAPED}
  syn region makeShDQ{S} contained start=/\%#=1"/ skip=/\%#=1\\[\\"]/
    \ end=/\%#=1"/ contains={EXPANSIONS},@makeLineEnd,makeShDQEscape,
    \makeShVar,makeShParam{S},{CMDSUB},makeShBacktick{S}

  syn region makeRecipe{S} contained start=/\%#=1^\ze\%(@\h\w*@\)*\t/
    \ skip=/\%#=1\\$/ end=/\%#=1.*\zs$/ keepend
    \ contains=makeRecipeStart,@makeShTop{S},makeShComment

  " A rule's recipe continues until a line that isn't a recipe line,
  " comment, blank line or conditional. Recipe lines can't be recognized by
  " themselves: a line starting with a tab is only a recipe line in the
  " context of a rule. In the body of a "define", it also ends after an
  " "endef" that an unterminated reference took.
  syn region makeRecipeBlock{S} contained
    \ start=/\%#=1^\ze\%({INRULE}\|{MISINDENTED}\)/
    \ end=/\%#=1^\%(\%({INRULE}\|{MISINDENTED}\)\@!{AFTERENDEF}\)/
    \ contains=makeRecipe{S},makeMisindented,makeComment,makeConfig,
    \makeConfigIf,makeConditional,makeBSDConditional

  " Rules are defined before the other kinds of lines, which take priority
  " when they start at the same place.
  syn match makeTarget{S} {CONTAINED} /\%#=1{BOL}{TARGETS}\ze{RULEOP}/
    \ contains=makeSpecialTarget,makePattern,{EXPANSIONS},@makeLineEnd
    \{ESCAPED}
    \ nextgroup=makeRuleBody{S},makeTargetVarOp{S} skipwhite
  syn region makeRuleBody{S} contained matchgroup=makeRuleOp
    \ start=/\%#=1&\=::\=/ skip=/\%#=1\\$/ end=/\%#=1.*\zs$/ keepend
    \ contains=makeSpecialTarget,makePattern,makeRuleOp,makeOrderOnly,
    \{EXPANSIONS},@makeEscExpansions,@makeLineEnd,makeComment,
    \makeInlineRecipe{S}
    \ nextgroup=makeRecipeBlock{S} skipnl
  syn region makeInlineRecipe{S} contained matchgroup=makeRuleOp
    \ start=/\%#=1;/ skip=/\%#=1\\$/ end=/\%#=1.*\zs$/ keepend
    \ contains=@makeShTop{S},makeShComment

  " Target-specific variable assignments, e.g. "prog: CFLAGS += -g", which
  " don't start a recipe. The operator is defined after rule bodies so it
  " takes priority when both match.
  syn match makeTargetVarOp{S} contained /\%#=1&\=::\=\ze\s*
    \\%(\%(export\|override\|private\|unexport\)\s\+\)*{VAR}{ASSIGN}/
    \ nextgroup=makeModifier{S},makePrefixedVariable{S} skipwhite

  " Variables after keywords like "export" and in target-specific
  " assignments have a separate group so that what contains the line-start
  " version doesn't also contain this one.
  syn match makeVariable{S} {CONTAINED} /\%#=1{BOL}{VAR}\ze{ASSIGN}/
    \ contains={EXPANSIONS}{ESCAPED} nextgroup=makeValue{S} skipwhite
  syn match makePrefixedVariable{S} contained /\%#=1{VAR}\ze{ASSIGN}/
    \ contains={EXPANSIONS}{ESCAPED} nextgroup=makeValue{S} skipwhite
  " Values are highlighted as shell code since that's how most of them end up
  " being used, except for calls to Make functions deferred with "$$", e.g.
  " "$$(error ...)". A value starts with the assignment operator, so the
  " shell code after it starts where a command can. A "#" outside of a
  " reference starts a comment even in what looks like a shell string, so the
  " value ends there, and the comment is matched by whatever contains the
  " line. Since GNU Make 4.3, "#" inside a reference doesn't start a comment,
  " so the skip pattern handles references like "$(shell echo '#')" that
  " don't contain other references. It doesn't skip "$$" at the end of a
  " line: a skip match that reaches the end of a line continues the region.
  " When the operator is on a continuation line, the backslash before it is
  " highlighted as part of the operator.
  syn region makeValue{S} contained matchgroup=makeAssignOp{S}
    \ start=/\%#=1\%(\\\n\s*\)\={OP}/
    \ skip=/\%#=1\\\%([\\#]\|$\)\|\$\%(\$\_$\@!\|[({][^)}#]*#\)/
    \ end=/\%#=1[^#]*\zs\%(\ze#\|$\)/ keepend
    \ contains=makeShCommandStart,@makeShTop{S},makeDeferredCall

  syn keyword makeModifier{S} {CONTAINED} export unexport override private
    \ nextgroup=makeModifier{S},makeStatementAfterModifier,makeDirArgs,
    \makePrefixedVariable{S},makeDefineAfterModifier skipwhite
END

" Continuation lines are joined and comments and blank lines removed with
" substitutions on the whole template, which is much faster than processing
" it a line at a time. In a string, "." and "[^\n]" match newlines, so the
" newline is excluded with "[^\x0a]".
let s:template = substitute(substitute(join(s:template, "\n"),
  \ '\%#=1\n\s*\\', '', 'g'),
  \ '\%#=1\%(^\|\n\)\s*\%("[^\x0a]*\)\=\ze\%(\n\|$\)', '', 'g')
let s:values = {'BOL': s:bol, 'OP': s:op, 'ASSIGN': s:assign,
  \ 'RULEOP': s:ruleop, 'INRULE': s:inrule, 'MISINDENTED': s:misindented}
for [s:values.S, s:values.CONTAINED, s:values.CMDSUB, s:values.ESCAPED,
  \ s:values.EXPANSIONS, s:values.AFTERENDEF] in
  \ [['', '', 'makeShCmdSub,makeShArith', '', '@makeExpansions', ''],
  \  ['Def', 'contained', '@makeEscExpansions', ',@makeEscExpansions',
  \   '@makeDefExpansions', '\|' . s:after_endef]]
  let s:values.VAR = s:var{s:values.S}
  let s:values.TARGETS = s:targets{s:values.S}
  for s:command in split(s:Expand(substitute(s:template,
    \ '\%#=1{\(\u\+\)}', '\=s:values[submatch(1)]', 'g')), "\n")
    exe s:command
  endfor
endfor
unlet s:template s:values s:command

" Directives -----------------------------------------------------------------

syn keyword makeConditional ifeq ifneq ifdef ifndef
  \ nextgroup=makeDirArgs skipwhite
syn keyword makeConditional else nextgroup=makeConditional skipwhite
syn keyword makeConditional endif
" GNU Make only treats these words as directives when they're followed by
" white space or the end of the line and not by an assignment operator, so
" "include:" starts a rule and "include = x" is an assignment. They're
" matches rather than keywords for that reason, and they're anchored to the
" start of the line since a match that can start anywhere makes Vim search
" every line for it. "undefine" can also follow keywords like "override".
let s:directive_end = '\ze\%($\|\s\+\%(' . s:op . '\)\@!\)/'
exe 'syn match makeInclude /\%#=1' . s:bol . '\%(-\=include\|sinclude\)'
  \ . s:directive_end 'contains=makeConfig nextgroup=makeDirArgs skipwhite'
exe 'syn match makeStatement /\%#=1' . s:bol
  \ . '\%(vpath\|undefine\|-\=load\)' . s:directive_end
  \ 'contains=makeConfig nextgroup=makeDirArgs skipwhite'
unlet s:directive_end
syn keyword makeStatementAfterModifier contained undefine
  \ nextgroup=makeDirArgs skipwhite

" BSD directives, which may have whitespace after the ".". They're defined
" after rules so a line like '.if "a:b"' isn't taken for one.
syn match makeBSDConditional
  \ /\%#=1^\.\s*\%(el\)\=if\%(n\=def\|n\=make\)\=\>/
  \ nextgroup=makeBSDExpr skipwhite
syn match makeBSDConditional /\%#=1^\.\s*\%(else\|endif\|endfor\)\>/
syn match makeBSDConditional /\%#=1^\.\s*for\>/
  \ nextgroup=makeBSDFor skipwhite
syn match makeInclude /\%#=1^\.\s*[-sd]\=include\>/
  \ nextgroup=makeDirArgs skipwhite
exe 'syn match makeStatement /\%#=1^\.\s*\%(' . join([
  \ 'break', 'error', 'export', 'export-all', 'export-env', 'export-literal',
  \ 'info', 'poison', 'undef', 'unexport', 'unexport-env', 'warning'
  \ ], '\|') . '\)\>/ nextgroup=makeDirArgs skipwhite'

Syn region makeBSDExpr contained start=/\%#=1\ze[^[:space:]#]/
  \ skip=/\%#=1\\\%([\\#]\|$\)\|\$\%(\$\_$\@!\|[({][^)}#]*#\)/
  \ end=/\%#=1[^#]*\zs\%(\ze#\|$\)/ keepend
  \ contains=@makeExpansions,@makeLineEnd,makeString,makeBSDFunction
syn keyword makeBSDFunction contained defined make empty exists target
  \ commands
Syn region makeBSDFor contained start=/\%#=1\ze[^[:space:]#]/
  \ skip=/\%#=1\\\%([\\#]\|$\)\|\$\%(\$\_$\@!\|[({][^)}#]*#\)/
  \ end=/\%#=1[^#]*\zs\%(\ze#\|$\)/ keepend
  \ contains=@makeExpansions,@makeLineEnd,makeBSDIn
syn keyword makeBSDIn contained in

" Multi-line variables -------------------------------------------------------

" "define" is only recognized at the start of a line or after keywords like
" "override"; otherwise, a word like "define" in a line that isn't recognized
" would start a "define" that continues to the end of the file.
syn match makeDefine /\%#=1^\s*\zsdefine\>/ nextgroup=makeDefineName skipwhite
syn match makeDefineAfterModifier contained /\%#=1define\>/
  \ nextgroup=makeDefineName skipwhite
exe 'Syn match makeDefineName contained /\%#=1' . s:var . '/'
  \ 'contains=@makeExpansions nextgroup=makeDefineOp,makeDefineBody'
  \ 'skipwhite skipnl'
syn match makeDefineOp contained /\%#=1\%(:::\|::\|[:+?!]\)\==/
  \ nextgroup=makeDefineBody skipwhite skipnl
" The body of a "define" is highlighted like the rest of the Makefile, using
" the "Def" copies of items, except that a line starting with a tab is always
" a recipe line since bodies are often canned recipes. The body starts on the
" line after "define" since a pending "nextgroup" is discarded at the end of a
" line unless "skipnl" is used. A line starting with a tab can't end a
" "define". The body also ends after an "endef" that an unterminated
" reference took.
exe 'Syn region makeDefineBody contained start=/\%#=1^/'
  \ 'matchgroup=makeDefine'
  \ 'end=/\%#=1^\%(' . s:endef . '\|' . s:after_endef . '\)/'
  \ 'contains=@makeDefineLines'

" References in the body of a "define" that aren't closed on their own line,
" which Make allows since it expands the whole body at once. "extend" lets
" one continue past the end of the item it starts in, and the lines it
" continues onto are highlighted like the rest of the body. "keepend" lets it
" find its end inside the items on those lines: its skip pattern passes over
" text and balanced parentheses and braces, so the end pattern only finds the
" one that closes it. One that's still open at the "endef" takes it,
" highlighting it as an error, and a reference that contains one like that
" ends right after it. Flagging the start of the reference instead would
" take a pattern that looks at later lines, and Vim doesn't highlight a line
" again when a later one changes. References with parentheses and braces
" share the region since each pattern makes highlighting a little slower
" everywhere, so a stray closing brace in one with parentheses, or the
" reverse, ends it early.
exe 'Syn region makeRefLines contained transparent keepend extend'
  \ 'matchgroup=makeRef'
  \ 'start=/\%#=1\$\%((\%(' . s:balancedDeep['('] . ')\)\@!'
  \ . '\|{\%(' . s:balancedDeep['{'] . '}\)\@!\)/'
  \ 'skip=/\%#=1\%(\s*endef\>\|\%(endef\)\@5<=\)\@!\%(\%([^(){}]\+\)\@>'
  \ . '\|(' . s:balancedDeep['('] . ')\|{' . s:balancedDeep['{'] . '}\)\+/'
  \ 'matchgroup=NONE end=/\%#=1[)}]'
  \ . '\|\%(^\%(\\\n\)\@2<!\%( [ \t]*\)\=endef\)\@20<=\>/'
  \ 'matchgroup=makeRefError'
  \ 'end=/\%#=1^\%(\\\n\)\@2<!\%( [ \t]*\)\=\zsendef\>/'
  \ 'contains=makeCallName,@makeDefineLines'

" Synchronization ------------------------------------------------------------

syn sync fromstart

" Highlighting ---------------------------------------------------------------

hi def link makeAngleString makeString
hi def link makeAssignOp Operator
hi def link makeBSDConditional makeConditional
hi def link makeBSDFunction Function
hi def link makeBSDIn makeConditional
hi def link makeCallName Function
hi def link makeComment Comment
hi def link makeConditional PreCondit
hi def link makeConfig PreCondit
hi def link makeConfigIf makeConfig
hi def link makeContError Error
hi def link makeDefine Define
hi def link makeDefineAfterModifier makeDefine
hi def link makeDefineName makeVariable
hi def link makeDefineOp makeAssignOp
hi def link makeEscDollar Special
hi def link makeEscAutoVar makeRef
hi def link makeEscRef makeRef
hi def link makeInclude Include
hi def link makeInlineRecipe makeRecipe
hi def link makeLineCont Special
hi def link makeMisindented Error
hi def link makeModifier Statement
hi def link makeOrderOnly makeRuleOp
hi def link makePrefixedVariable makeVariable
hi def link makePattern Special
hi def link makeRecipe Number
hi def link makeRecipePrefix Special
hi def link makeRef Identifier
hi def link makeRefError Error
hi def link makeRuleOp Operator
hi def link makeShComment Comment
hi def link makeShCommentAfterPrefix makeShComment
hi def link makeShCmdSubDelim Special
hi def link makeShCommandLineCont makeLineCont
hi def link makeShConditional Conditional
hi def link makeShDQ String
hi def link makeShDQEscape Special
hi def link makeShEscape Special
hi def link makeShIn Statement
hi def link makeShParam PreProc
hi def link makeShellCallName makeCallName
hi def link makeShPrefixKeyword makeShConditional
hi def link makeShRefLineCont makeLineCont
hi def link makeShRepeat Repeat
hi def link makeShSQ String
hi def link makeShVar PreProc
hi def link makeSpecialTarget Statement
hi def link makeStatement Statement
hi def link makeStatementAfterModifier makeStatement
hi def link makeString String
hi def link makeTarget Function
hi def link makeTargetVarOp makeRuleOp
hi def link makeTodo Todo
hi def link makeVariable Identifier

for s:group in ['makeAssignOp', 'makeInlineRecipe', 'makeModifier',
              \ 'makePrefixedVariable', 'makeRecipe', 'makeShDQ',
              \ 'makeShParam', 'makeShSQ', 'makeTarget', 'makeTargetVarOp',
              \ 'makeVariable']
  exe 'hi def link' s:group . 'Def' s:group
endfor
unlet s:group

let b:current_syntax = "make"

delcommand -buffer Syn
delfunction s:Expand

let &cpo = s:cpo_save
unlet s:cpo_save s:autoconf s:clusters s:balanced s:balancedDeep s:endef
  \ s:after_endef s:bol s:ref s:refDef s:var s:varDef s:op s:assign s:targets
  \ s:targetsDef s:ruleop s:inrule s:misindented s:functions
  \ s:pattern_functions s:deferred_functions s:special_targets
