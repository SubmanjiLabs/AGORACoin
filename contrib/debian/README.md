
Debian
====================
This directory contains files used to package agorad/agora-qt
for Debian-based Linux systems. If you compile agorad/agora-qt yourself, there are some useful files here.

## agora: URI support ##


agora-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install agora-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your agoraqt binary to `/usr/bin`
and the `../../share/pixmaps/agora128.png` to `/usr/share/pixmaps`

agora-qt.protocol (KDE)

