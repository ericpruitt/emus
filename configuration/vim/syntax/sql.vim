" Vim syntax file
" Language:     SQL with SQLite and other additions.
" Author:       Jessica K McIntosh AT gmail DOT com
" Maintainer:   Eric Pruitt; https://github.com/ericpruitt

" More complete SQL matching with error reporting.
" Only matches types inside 'CREATE TABLE ();'.
" Highlights functions. Unknown functions are an error.
" Based on the SQL syntax files that come with Vim.

" For version 5.x: Clear all syntax items
" For version 6.x: Quit when a syntax file was already loaded
if version < 600
  syntax clear
elseif exists("b:current_syntax")
  finish
endif

syn case ignore

" All non-contained SQL syntax.
syn cluster sqlALL          contains=TOP

" Various error conditions.
syn match   sqlError        "\<\w\+("           " Not a known function.
syn match   sqlError        ")"                 " Lonely closing paren.
syn match   sqlError        ",\(\_\s*[;)]\)\@=" " Comma before a paren or semicolon.
syn match   sqlError        " $"                " Space at the end of a line.
" Comma before certain words.
syn match   sqlError        ",\_\s*\(\<\(asc\|desc\|exists\|for\|from\)\>\)\@="
syn match   sqlError        ",\_\s*\(\<\(group by\|into\|limit\|order\)\>\)\@="
syn match   sqlError        ",\_\s*\(\<\(table\|using\|where\)\>\)\@="

" Special words.
syn keyword sqlSpecial      false null true

" Keywords
syn keyword sqlKeyword
\   access add after aggregate as asc authorization begin by cache cascade
\   check cluster collate collation column compress conflict connect connection
\   constraint current cursor database debug decimal default desc each else
\   elsif escape exception exclusive explain external file for foreign from
\   function group having identified if immediate increment index initial inner
\   into is join key left level loop maxextents mode modify nocompress nowait
\   object of off offline on online option order outer pctfree primary
\   privileges procedure public references referencing release resource return
\   role row rowid rowlabel rownum rows schema session share size start
\   successful synonym then to transaction trigger uid user using validate
\   values view virtual whenever where with

syn match   sqlKeyword      "\<prompt\>"
syn match   sqlKeyword      "\<glob\>"
" Do special things with CREATE TABLE ( below.
syn match   sqlKeyword      "\<table\>"

" SQLite Pragmas - Treat them as keywords.
syn keyword sqlKeyword
\   automatic_index auto_vacuum cache_size case_sensitive_like checkpoint_fullfsync
\   collation_list compile_options count_changes database_list default_cache_size
\   empty_result_callbacks encoding foreign_key_list foreign_keys freelist_count
\   full_column_names fullfsync ignore_check_constraints incremental_vacuum
\   index_info index_list integrity_check journal_mode journal_size_limit
\   legacy_file_format locking_mode max_page_count page_count page_size
\   parser_trace quick_check read_uncommitted recursive_triggers
\   reverse_unordered_selects schema_version secure_delete short_column_names
\   synchronous table_info temp_store temp_store_directory user_version
\   vdbe_listing vdbe_trace wal_autocheckpoint wal_checkpoint writable_schema

" Operators
syn keyword sqlOperator
\   all and any between case distinct elif else end all and any between case
\   distinct elif else end exit exists if in intersect is like match matches
\   minus not or out prior regexp some then union unique when
syn match   sqlOperator     "||\|:="

" Conditionals
syn match   sqlConditional  "=\|<\|>\|+\|-"

" Functions - Only valid with a '(' after them.
syn match   sqlFunction     "\<\(abs\|acos\|asin\|atan2\|avg\|cardinality\|
\cast\|ceil\|changes\|character_length\|char_length\|coalesce\|concat\|cos\|
\count\|date\|exp\|filetoblob\|filetoclob\|floor\|glob\|group_concat\|hex\|if\|
\ifnull\|initcap\|isnull\|julianday\|last_insert_rowid\|length\|log10\|logn\|
\lower\|lpad\|ltrin\|max\|min\|mod\|nullif\|octet_length\|pow\|quote\|random\|
\range\|replace\|root\|round\|rpad\|sin\|soundex\|sqrtstdev\|strftime\|substr\|
\substring\|sum\|sysdate\|tan\|time\|to_char\|to_date\|to_number\|total\|trim\|
\trunc\|typeof\|upper\|variance\|substring_index\)(\@="

" Oracle DBMS functions.
syn match   sqlFunction     "\<dbms_\w\+\.\w\+(\@="

" SQLite Functions
syn match   sqlFunction     "\<\(last_insert_rowid\|load_extension\|randomblob\)(\@="
syn match   sqlFunction     "\<\(sqlite_compileoption_get\|sqlite_compileoption_used\)(\@="
syn match   sqlFunction     "\<\(sqlite_source_id\|sqlite_version\|sqlite_version\)(\@="
syn match   sqlFunction     "\<\(zeroblob\|ltrim\|rtrim\)(\@="

" SQLite Command Line Client Functions
syn match   sqlFunction     "^\.\w\+"

" Statements
syn keyword sqlStatement
\   alter analyze audit begin comment commit delete alter analyze audit begin
\   comment commit delete drop execute explain grant insert lock noaudit rename
\   revoke rollback savepoint select truncate update vacuum
syn match   sqlStatement    "\<\(replace\|create\)\>"

" SQLite Statements
syn keyword sqlStatement    attach detach indexed pragma reindex

" Types - Only matched inside 'CREATE TABLE ();'.
syn keyword sqlType
\   contained bigint bit blob bool boolean byte char contained bigint bit blob
\   bool boolean byte char contained clob date datetime dec decimal enum
\   contained float int int8 integer interval long contained longblob longtext
\   lvarchar mediumblob contained mediumint mediumtext mlslabel money contained
\   multiset nchar number numeric nvarchar contained raw real rowid serial
\   serial8 set contained smallfloat smallint text time contained timestamp
\   tinyblob tinyint tinytext contained varchar varchar2 varray year
syn match   sqlType         contained "\<\(character\|double\|varying\)\>"
syn match   sqlType         contained "\<character\s\+varying\>"
syn match   sqlType         contained "\<double\s\+precision\>"

" Oracle Variables
syn match   sqlVariable     "&\a\w\+"
syn match   sqlVariable     ":\w\+"
syn match   sqlVariable     "SQL%\w\+"

" Strings
syn region sqlString        start=+"+  skip=+\\\\\|\\"+  end=+"+ contains=sqlVariable
syn region sqlString        start=+'+  skip=+\\\\\|\\'+  end=+'+ contains=sqlVariable
syn region sqlString        start=+`+  skip=+\\\\\|\\`+  end=+`+ contains=sqlVariable

" Numbers
syn match sqlNumber         "-\=\<[0-9]*\>"
syn match sqlNumber         "-\=\<[0-9]*\.[0-9]*\>"
syn match sqlNumber         "-\=\<[0-9][0-9]*e[+-]\=[0-9]*\>"
syn match sqlNumber         "-\=\<[0-9]*\.[0-9]*e[+-]\=[0-9]*\>"
syn match sqlNumber         "\<0x[abcdef0-9]*\>"

" Todo
syn keyword sqlTodo         contained DEBUG FIXME NOTE TODO XXX

" Comments
syn region sqlComment       start="/\*"  end="\*/" contains=sqlTodo
syn match  sqlComment       "--.*$" contains=sqlTodo
syn match  sqlComment       "rem.*$" contains=sqlTodo

" Mark correct paren use. Different colors for different purposes.
syn region  sqlParens       transparent matchgroup=sqlParen start="(" end=")"
syn match   sqlParenEmpty   "()"
syn region  sqlParens       transparent matchgroup=sqlParenFunc start="\(\<\w\+\>\)\@<=(" end=")"

" Highlight types correctly inside create table and procedure statements.
" All other SQL is properly highlighted as well.
syn region  sqlTypeParens   contained matchgroup=sqlType start="(" end=")" contains=@sqlALL
syn match   sqlTypeMatch    contained "\(\(^\|[,(]\)\s*\S\+\s\+\)\@<=\w\+\(\s*([^)]\+)\)\?" contains=sqlType,sqlTypeParens
syn match   sqlTypeMatch    contained "\(\(^\|[,(]\)\s*\S\+\s\+\)\@<=character\s\+varying\s*([^)]\+)" contains=sqlType,sqlTypeParens
syn region  sqlTypeRegion   matchgroup=sqlParen start="\(create\s\+table\s\+[^(]\+\s\+\)\@<=(" end=")" contains=@sqlALL,sqlTypeMatch
syn region  sqlTypeRegion   matchgroup=sqlParen start="\(create\s\+\(or\s\+replace\s\+\)\?procedure\s\+[^(]\+\s\+\)\@<=(" end=")" contains=@sqlALL,sqlTypeMatch

" SQL Embedded in a statement.
syn region  sqlquoteRegion  matchgroup=sqlParen start="\(execute\s\+immediate\s*\)\@<=('" end="')" contains=@sqlALL

" Special Oracle Statements
syn match   sqlStatement    "^\s*\(prompt\|spool\)\>" nextgroup=sqlAnyString
syn match   sqlStatement    "^\s*accept\s\+" nextgroup=sqlAnyVariable
syn match   sqlStatement    "declare\s\+" nextgroup=sqlDeclare
syn region  sqlDeclare      contained matchgroup=sqlVariable start="\a\w\+" end="$" contains=@sqlALL,sqlType
syn match   sqlOperator     "^@" nextgroup=sqlAnyString
syn match   sqlAnyVariable  contained "\a\w\+"
syn match   sqlAnyString    contained ".*" contains=sqlVariable

syn region  sqlSetRegion    matchgroup=sqlStatement start="^\s*set\>" matchgroup=NONE end="$" contains=sqlSetOptions,sqlSetValues
syn keyword sqlSetOptions   contained autorecovery colsep copytypecheck describe escchar flagger
syn keyword sqlSetOptions   contained instance logsource long null recsep recsepchar
syn keyword sqlSetOptions   contained 
syn match   sqlSetOptions   contained "\<\(app\w*\|array\w*\|auto\w*\|autop\w*\)\>"
syn match   sqlSetOptions   contained "\<\(autot\w*\|blo\w*\|cmds\w*\|con\w*\|copyc\w*\)\>"
syn match   sqlSetOptions   contained "\<\(def\w*\|echo\|editf\w*\|emb\w*\|errorl\w*\|esc\w*\)\>"
syn match   sqlSetOptions   contained "\<\(feed\w*\|flu\w*\|hea\w*\|heads\w*\|lin\w*\)\>"
syn match   sqlSetOptions   contained "\<\(lobof\w*\|longc\w*\|mark\w*\|newp\w*\|numf\w*\)\>"
syn match   sqlSetOptions   contained "\<\(pages\w*\|pau\w*\|serverout\w*\|shift\w*\|show\w*\)\>"
syn match   sqlSetOptions   contained "\<\(sqlbl\w*\|sqlc\w*\|sqlco\w*\|sqln\w*\|sqlpluscompat\w*\)\>"
syn match   sqlSetOptions   contained "\<\(sqlpre\w*\|sqlp\w*\|sqlt\w*\|suf\w*\|tab\)\>"
syn match   sqlSetOptions   contained "\<\(term\w*\|timi\w*\|und\w*\|ver\w*\|wra\w\?\)\>"
syn match   sqlSetOptions   contained "\<\(xquery\s\+\(baseuri\|ordering\|node\|context\)\)\>"
syn keyword sqlSetValues    contained all body byreference byvalue default
syn keyword sqlSetValues    contained entry fill head html identifier indent
syn keyword sqlSetValues    contained linenum local none off on size table truncate
syn match   sqlSetValues    contained "\<\(ea\w*\|wr\w*\|imm\w*\|trace\w*\|expl\w*\|stat\w*\)\>"
syn match   sqlSetValues    contained "\<\(intermed\w*\|pre\w*\|unl\w*\|for\w*\|wra\w*\|wor\w\?\)\>"
syn match   sqlSetValues    contained "\<\(vis\w*\|inv\w*\)\>"
syn match   sqlSetValues    contained "\<\(\(un\)\?ordered\)\>"

" Stolen from sh.vim.
if !exists("sh_minlines")
  let sh_minlines = 200
endif
if !exists("sh_maxlines")
  let sh_maxlines = 2 * sh_minlines
endif
exec "syn sync minlines=" . sh_minlines . " maxlines=" . sh_maxlines

" Define the default highlighting.
" For version 5.7 and earlier: only when not done already
" For version 5.8 and later: only when an item doesn't have highlighting yet
if version >= 508 || !exists("did_sql_syn_inits")
    if version < 508
        let did_sql_syn_inits = 1
        command -nargs=+ HiLink hi link <args>
    else
        command -nargs=+ HiLink hi def link <args>
    endif

    HiLink sqlComment       Comment
    HiLink sqlError         Error
    HiLink sqlFunction      Function
    HiLink sqlKeyword       Special
    HiLink sqlConditional   Conditional
    HiLink sqlNumber        Number
    HiLink sqlOperator      Operator
    HiLink sqlParen         Comment
    HiLink sqlParenEmpty    Operator
    HiLink sqlParenFunc     Function
    HiLink sqlSpecial       Keyword
    HiLink sqlStatement     Statement
    HiLink sqlString        String
    HiLink sqlTodo          Todo
    HiLink sqlType          Type
    HiLink sqlVariable      Identifier

    HiLink sqlAnyString     sqlString
    HiLink sqlAnyVariable   sqlVariable
    HiLink sqlSetOptions    Operator
    HiLink sqlSetValues     Special

    delcommand HiLink
endif

let b:current_syntax = "sql"
