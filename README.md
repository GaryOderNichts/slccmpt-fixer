# slccmpt-fixer
This fixes the modes for slccmpt01 (vwii) on the wii u

## How to use
- Place the .elf to sd:/wiiu/apps
- Run mocha cfw (haxchi isn't supported at the moment)
- Run, press A

## Why?
When restoring a slccmpt file backup the correct modes needs to be set.
This is exactly what this program does.
There are already other methods (for Example with wupserver) but this is probably the easiest

## TODO
- haxchi support (freezes console for me when implemented)

## Disclaimer
I am not responsible for any additional bricks. This is still in early development and I am not an expirienced Wii U developer.

## Build
Download devkitpro ppc, portlibs, libogc and build and install latest libiosuhax.
Then run `make`