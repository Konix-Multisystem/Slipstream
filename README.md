
# Slipstream - The Flare One, Konix Multisystem & MSU era emulator

This is the public archive of the Slipstream emulator, it's been developed on and off for 12+ years, and covers a number of different iterations of the hardware.

## Building

I'll update these notes in the future, but for now if you want to compile it locally, look at the .github actions to get an idea of how to grab the various pre-requisites.

## Status

A note on accuracy, the emulator doesn't respect the timings of the original hardware in a number of areas (If you want accuracy, look at the verilator/MiSTer version) - This was put together before we had discovered the NET Lists, and often I took shortcuts to get things running when software turned up.

* Flare One
    * Mostly complete
        * Incorrect blitter stop - see Ark-A-Hack
    * Possible bugs in keyboard handling
* Konix 88 Era
    * Mostly complete
* Konix 89 Era
    * Mostly complete
        * Lacks DPS/Floppy interaction, so bios security check is bypassed at present
* MSU
    * A Lot of missing bits
        * SS3/4 hardware incomplete
        * BIOS areas incomplete
        * Soft emulation of CD bios for testing against MSU Robocod
  
Other Notable things buried in the sources :

* PDS Emulator
    * There is an XT emulator designed to load the PDS.EXE's which were used to assemble and upload demos to the Flare One and Konix Multisystem
* Shared Memory Debugger
    * There is a full debugger remote interface, it was designed for a tool I wrote in dotnet : https://github.com/Konix-Multisystem/Slipstream_Remote

* Presentations
    * [LIVE Stream of fixing the emulator for Magicians Apprentice - 2025](https://www.youtube.com/watch?v=-wFu7-jc_O0)
