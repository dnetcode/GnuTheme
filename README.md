## GnuTheme

GnuTheme is a set of services for IRC networks designed for large IRC networks with high
scalability requirements.  It is relatively mature software, with some code and design
derived from another package called Atheme and Shrike.

GnuTheme's behavior is tunable using modules and a highly detailed configuration file.
Almost all behavior can be changed at deployment time just by editing the configuration.

If you are running this code from Git, you should read GIT-Access for instructions on
how to fully check out the GnuTheme tree, as it is spread across many repositories.

## basic build instructions for the impatient

Whatever you do, make sure you do *not* install GnuTheme into the same location as the source.
GnuTheme will default to installing in `$HOME/GnuTheme`, so make sure you plan accordingly for this.

    $ git submodule update --init
    $ ./configure
    $ make
    $ make install

If you're still lost, read [INSTALL](INSTALL) or [GIT-Access](GIT-Access) for hints. 

## SWI (services web interface) Web Panel for GnuTheme

If you are interested in having a Services Web Interface (or Services Web Panel), SWI
*IS* compatible with GnuTheme and other Atheme forks, and is available at:
http://www.GnuTheme.org/SWI/ or http://github.com/GnuThemeOrg/SWI/

## links / contact

 * [GitHub](http://www.github.com/GnuThemeOrg/GnuTheme)
 * [Website](http://www.GnuTheme.org/GnuTheme/)
 * [IRC](irc://irc.devnet.pw/#GnuTheme) irc.devnet.pw #GnuTheme
 * [IRC](irc://chat.freenode.net/#GnuTheme) chat.Freenode.net #GnuTheme
