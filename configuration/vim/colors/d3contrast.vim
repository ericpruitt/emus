" vim: set syntax=vim:
" Name:         d3contrast
" Maintainer:   Eric Pruitt <eric.pruitt@gmail.com>
" License:      MIT License
" Original:     Diablo3 (http://www.vim.org/scripts/script.php?script_id=3606)

if version > 580
  hi clear
  if exists("syntax_on")
    syntax reset
  endif
endif
let g:colors_name="d3contrast"

if &t_Co > 255
    highlight Boolean         cterm=bold                    ctermfg=141
    highlight Character                                     ctermfg=185
    highlight Number                                        ctermfg=141
    highlight String                                        ctermfg=220
    highlight Conditional     cterm=bold                    ctermfg=68
    highlight Constant        cterm=bold                    ctermfg=148
    highlight Cursor                          ctermbg=231   ctermfg=16
    highlight Debug           cterm=bold                    ctermfg=250
    highlight Define                                        ctermfg=81
    highlight Delimiter                                     ctermfg=245
    highlight DiffAdd                         ctermbg=236
    highlight DiffChange                      ctermbg=238   ctermfg=244
    highlight DiffDelete                      ctermbg=232   ctermfg=89
    highlight DiffText        cterm=bold      ctermbg=238

    highlight Directory       cterm=bold                    ctermfg=155
    highlight Error                           ctermbg=232   ctermfg=89
    highlight ErrorMsg        cterm=bold      ctermbg=235   ctermfg=161
    highlight Exception       cterm=bold                    ctermfg=155
    highlight Float                                         ctermfg=141
    highlight FoldColumn                      ctermbg=16    ctermfg=239
    highlight Folded                          ctermbg=16    ctermfg=239
    highlight Function                                      ctermfg=148
    highlight Identifier      cterm=none                    ctermfg=172
    highlight Ignore                                        ctermfg=244
    highlight IncSearch                       ctermbg=16    ctermfg=144

    highlight Keyword         cterm=bold                    ctermfg=161
    highlight Label           cterm=none                    ctermfg=185
    highlight Macro                                         ctermfg=144
    highlight SpecialKey                                    ctermfg=148

    highlight MatchParen      cterm=bold      ctermbg=208   ctermfg=16

    highlight ModeMsg                                       ctermfg=185
    highlight MoreMsg                                       ctermfg=185
    highlight Operator                                      ctermfg=68

    " complete menu
    highlight Pmenu                           ctermbg=16    ctermfg=81
    highlight PmenuSel                        ctermbg=244
    highlight PmenuSbar                       ctermbg=232
    highlight PmenuThumb                                    ctermfg=81

    highlight PreCondit       cterm=bold                    ctermfg=155
    highlight PreProc                                       ctermfg=155
    highlight Question                                      ctermfg=81
    highlight Repeat          cterm=bold                    ctermfg=161
    highlight Search                          ctermbg=32    ctermfg=231
    " marks column
    highlight SignColumn                      ctermbg=235   ctermfg=155
    highlight SpecialChar     cterm=bold                    ctermfg=161
    highlight SpecialComment  cterm=bold                    ctermfg=239
    highlight Special                                       ctermfg=81
    highlight SpecialKey                                    ctermfg=245

    if has("spell")
        highlight SpellBad    cterm=underline               ctermbg=240
        highlight SpellCap    cterm=underline
        highlight SpellLocal  cterm=underline
        highlight SpellRare   cterm=underline
    endif

    highlight Statement       cterm=bold                    ctermfg=68
    highlight htmlStatement                                 ctermfg=110
    highlight StatusLine                                    ctermfg=240
    highlight StatusLineNC                    ctermbg=232   ctermfg=244
    highlight StorageClass                                  ctermfg=208
    highlight Structure                                     ctermfg=81
    highlight Tag                                           ctermfg=161
    highlight Title                                         ctermfg=209
    highlight Todo            cterm=bold      ctermbg=235   ctermfg=231

    highlight Typedef                                       ctermfg=81
    highlight Type            cterm=none                    ctermfg=81
    highlight Underlined      cterm=underline               ctermfg=244

    highlight VertSplit       cterm=bold      ctermbg=232   ctermfg=244
    highlight VisualNOS                       ctermbg=237
    highlight Visual                          ctermbg=237
    highlight WarningMsg      cterm=bold      ctermbg=236   ctermfg=231
    highlight WildMenu                        ctermbg=16    ctermfg=81

    highlight Normal                          ctermbg=0     ctermfg=255
    highlight Comment                                       ctermfg=241
    highlight CursorLine      cterm=none      ctermbg=236
    highlight CursorColumn                    ctermbg=236
    highlight LineNr                          ctermbg=234   ctermfg=250
    highlight NonText                         ctermbg=234   ctermfg=234

    highlight LongLineWarning cterm=underline ctermbg=53    ctermfg=1
end
