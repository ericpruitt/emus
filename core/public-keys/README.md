PGP / GPG Keys
==============

The folder contains public keys used by [GPG][gpg] for cryptographic signature
verification are stored in "public-keys/". The GPG home directory used to store
the keys in their imported form is "gnupghome/" which will be created by the
Makefile.

Below is a list of the keys and where they were obtained. Each entry will
typically have two sub-items: the first is the indirect location of a key (i.e.
a page hosting a link to the key), and the second is the direct URL or other
means of actually fetching the key. The idea is that it will make finding
replacement keys easier if they are moved.

- bash-chet-ramey.asc (a.k.a readline-chet-ramey.asc)
  - <https://tiswww.case.edu/php/chet/>
  - <https://tiswww.case.edu/php/chet/gpgkey.asc>
- coreutils-pádraig-brady.asc
  - <https://savannah.gnu.org/forum/forum.php?forum_id=8445>
  - `gpg --keyserver keys.gnupg.net --recv-keys DF6FD971306037D9`
- findutils-keyring.asc
  - <https://savannah.gnu.org/project/memberlist-gpgkeys.php?group=findutils>
- gawk-arnold-robbins.asc
  - <https://savannah.gnu.org/project/memberlist.php?group=gawk>
  - <https://savannah.gnu.org/people/viewgpg.php?user_id=80653>
- gmp-niels-möller.asc
  - <https://gmplib.org/>
  - `gpg --keyserver keys.gnupg.net --recv-keys 343C2FF0FBEE5EC2EDBEF399F3599FF828C67298`
- grep-jim-meyering.asc (a.k.a. sed-jim-meyering.asc)
  - <https://savannah.gnu.org/users/meyering>
  - <https://savannah.gnu.org/people/viewgpg.php?user_id=133>
- less-mark-nudelman.asc
  - <http://www.greenwoodsoftware.com/less/download.html>
  - <http://www.greenwoodsoftware.com/less/pubkey.asc>
- libevent-azat-khuzhin.asc
  - <https://github.com/libevent/libevent/releases>
  - <https://github.com/libevent/libevent/releases/tag/release-2.1.8-stable>
  - `gpg --keyserver keys.gnupg.net --recv-keys B86086848EF8686D`
- libevent-nick-mathewson.asc
  - <http://www.wangafu.net/~nickm/>
  - <http://www.wangafu.net/~nickm/public_key.asc>
- libevent-niels-provos.asc
  - <http://www.citi.umich.edu/u/provos/>
  - <http://www.citi.umich.edu/u/provos/pgp.key>
- mpfr-vincent-lefevre.asc
  - <http://www.mpfr.org/mpfr-current/>
  - `gpg --keyserver keys.gnupg.net --recv-keys 07F3DBBECC1A39605078094D980C197698C3739D`
- musl.asc
  - <https://www.musl-libc.org/download.html>
  - <https://www.musl-libc.org/musl.pub>
- ncurses-thomas-e-dickey.asc
  - <http://invisible-island.net/public/public.html>
  - <http://invisible-island.net/public/dickey-invisible-island.txt>
- oath-toolkit-simon-josefsson.asc
  - <https://blog.josefsson.org/2021/05/01/openpgp-smartcard-with-gnome-on-debian-11-bullseye/>
  - <https://josefsson.org/key-20190320.txt>

  [gpg]: https://www.gnupg.org/ "GNU Privacy Guard"
