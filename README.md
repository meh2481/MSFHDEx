MSFHDEx
======

Extraction tools for Engine Black's .data file format used in various WayForward games, such as:
* Mighty Switch Force Hyper Drive Edition
* Shantae and the Pirate's Curse

Should work fine with other Engine Black titles as well.

There are several layers of packaging here; the .anim files contain actual sprite data, and are packed inside .vol files inside a giant .data blob file that contains everything. The .data file can be extracted with the (included) BMS script (See http://quickbms.aluigi.org if you don't know what a BMS script is), volEx extracts files from inside the .vol files, and animEx extracts images and sprites from .anim files (There are other file formats of interest, haven't gotten to them yet)


Usage:

volEx.exe [filenames.vol]

animEx.exe [filenames.anim]