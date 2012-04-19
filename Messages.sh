#! /usr/bin/env bash

$EXTRACTRC `find muon/ -name \*.rc` >> muonrc.cpp
$EXTRACTRC `find updater/ -name \*.rc` >> updaterrc.cpp
$EXTRACTRC `find kded/ -name \*.rc` >> notifierrc.cpp
$EXTRACTRC --context="Category" --tag-group=none --tag=Name libmuon/categories.xml >> categoriesxml.cpp
$XGETTEXT categoriesxml.cpp `find libmuon -name \*.cpp` -o $podir/libmuon.pot
$XGETTEXT rc.cpp muonrc.cpp `find muon -name \*.cpp` -o $podir/muon.pot
$XGETTEXT rc.cpp updaterrc.cpp `find updater -name \*.cpp` -o $podir/muon-updater.pot
$XGETTEXT rc.cpp `find installer -name \*.cpp` -o $podir/muon-installer.pot
$XGETTEXT rc.cpp notifierrc.cpp `find kded/ -name \*.cpp` -o $podir/muon-notifier.pot
$XGETTEXT `find discover -name \*.cpp -o -name \*.qml` -o $podir/muon-discover.pot
rm -f muonrc.cpp
rm -f notifierrc.cpp
rm -f updaterrc.cpp
rm -f categoriesxml.cpp
