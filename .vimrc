"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
" .vimrc by Alex
"
" Updates:
" 2014/01/23: add Vundle
"
" """""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
set nocompatible " be vIMproved

" vundle
filetype off
set rtp+=~/.vim/bundle/Vundle.vim/
call vundle#begin()
Plugin 'gmarik/vundle'
"----- insert your own vundles
Plugin 'altercation/vim-colors-solarized'
Plugin 'tomasr/molokai'
Plugin 'nvie/vim-flake8'
Plugin 'kien/ctrlp.vim'
Plugin 'szw/vim-tags'
"-----
call vundle#end()
filetype plugin indent on
"------------ /vundle

" Disable some PEP8 warnings
" E122: continuation line missing indentation or outdented
" E123: closing bracket does not match indentation of opening bracket's line
" E124: closing bracket does not match visual indentation
" E126: align params with branches
" E127: continuation line over-indented for visual indent
" E128: continuation line under-indented for visual indent
" E241: multiple spaces after operators and ","
let g:flake8_ignore='E122,E123,E124,E126,E127,E128,E241'

" tags
set tags=./tags;

" backups
set swapfile
set backup
let g:backup_directory='~/.vim/backups/'
let g:backup_purge=10

" enable filetypes
filetype on
filetype plugin on
filetype indent on

" terminal settings
let solarized_termcolors=256
set t_Co=256
syntax on
let python_highlight_all=1

set encoding=utf-8
set termencoding=utf-8
set ffs=unix,dos,mac
set fileencodings=utf-8,cp1251,koi8-r,cp866

" write the old file out when switching between files
set autowrite

" display current cursor position in LR corner
set ruler

" tab stuff
set tabstop=4
set shiftwidth=4
set softtabstop=4
set smarttab
set expandtab
set list listchars=extends:>,precedes:<,tab:~~,trail:·,nbsp:_
set smartindent
set autoindent
set cindent
set colorcolumn=80

" formatting
set wrap
set linebreak
set wrapmargin=80
set textwidth=80
set formatoptions=qrn1

" status
set laststatus=2
set linespace=3

" show command in LR corner
set showcmd
set statusline=%<%f%h%m%r%=format=%{&fileformat}\ file=%{&fileencoding}\ enc=%{&encoding}\ %b\ 0x%B\ %l,%c%V\ %P

" show line numbers
set number

" bell
set visualbell

" smart backspace
set backspace=indent,eol,start whichwrap+=<,>,[,]

"Вырубаем черточки на табах
set showtabline=1

" search
set showmatch
set hlsearch
set incsearch
set ignorecase
set smartcase

" folding - za
set foldenable
set foldcolumn=1
set foldmethod=indent
set foldlevel=99

" change WD to document one
autocmd BufEnter * cd %:p:h

" print unprintable chars
set list

" <Solarized> colorcheme
set background=light
colorscheme solarized

"Перед сохранением вырезаем пробелы на концах (только в .py файлах)
autocmd BufWritePre *.py normal m`:%s/\s\+$//e ``

"В .py файлах включаем умные отступы после ключевых слов
autocmd BufRead *.py set smartindent cinwords=if,elif,else,for,while,try,except,finally,def,class

map [a [I:let nr = input("Which one: ")<Bar>exe "normal " . nr ."[\t"<CR>

