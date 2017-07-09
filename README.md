# dwmstatus

This is a version of the dwmstatus code (originally found [here](http://dwm.suckless.org/dwmstatus/))

My version has been modified to my tastes (particularly in coding standards) and to fit my machines. 

Included:

* **sysavg:** 1 minute sys avg divided by number of cores
* **battery:** read from files under /sys
* **mail:** As reported by notmuch
* **media:** As reported by mpd/mopidy

## Building

* Clone this repository and cd into it
* Make sure you've got the right headers (remember that you may need to install `-dev` or `-devel` packages on your distro to be able to link against them
    - libnotmuch
    - libmpdclient
* Modify `dwmstatus.c` to fit your machine (the import bits are `#defines` at the top-ish)
* `make`
* Put the binary somewhere
