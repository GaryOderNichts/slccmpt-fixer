# Warning: This is now part of [vWii NAND Restorer](https://github.com/GaryOderNichts/vWii-NAND-Restorer) and is no longer in development!

## slccmpt-fixer
This fixes the modes for slccmpt01 (vwii) on the wii u

## How to use
- Place the .elf to sd:/wiiu/apps
- Run cfw (mocha or haxchi)
- Run, press A

## Why?
When restoring a slccmpt file backup the correct modes needs to be set.
This is exactly what this program does.
There are already other methods (for Example with wupserver) but this is probably the easiest

## Disclaimer
I am not responsible for any additional bricks.

## Build
Download devkitpro ppc, portlibs, libogc and build and install latest libiosuhax.
Then run `make`
