# apparix
Fast command-line file system navigation by bookmarking directories and jumping to a bookmark directly

## Note
- This repository is created with aim to archive code source of apparix.
Current debian supported version is [07-261](https://pkgs.org/download/apparix) (2007) which contains bugs and somehow useless.
The latest official version 11-062 (2011) is really old but it works fine. So it's recommended to install manually

- For more information please visit the official site [micans.org](https://micans.org/apparix/)

## Instalation
Simply run:
```sh
./configure && sudo make install
```

Or install locally if do not have root access:
```sh
./configure --prefix=$HOME/.local --exec-prefix=$HOME && make install
```

To make it works, the apparix functions need to be created and source from .bashrc (gnu/linux) or .bash_profile (osx)
```sh
apparix --shel-examples > ~/.apparix.bash
echo -e "\n#Use apparix\n[ -f ~/.apparix.bash ] && source ~/.apparix.bash >> ~/.bashrc"
```
