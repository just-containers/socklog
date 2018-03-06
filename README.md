# socklog

This is a fork of Gerrit Pape's [socklog](http://smarden.org/socklog/) with a few changes.

* Instead of bringing its own copy of DJB's tools for handling strings, buffers, etc, it uses
  [skalibs](https://skarnet.org/software/skalibs/).
* Instead of using the old "slashpackage" format, there's a more familiar "configure" script.

Outside of the new build system, I don't plan on adding new features --
just fixing bugs, silencing compiler warnings, etc.

## Usage

See the original [socklog docs](https://skarnet.org/software/skalibs/), mostly
everything still applies.

This version of socklog removes the `socklog-check` and `socklog-conf` program,
since those are specific to `runit`, so those items don't apply.

Feel free to look at [socklog-overlay](https://github.com/just-containers/socklog-overlay)
for a simple example of using socklog with [s6](https://skarnet.org/software/s6/).

## Releases

socklog is available as static binaries, please see the `Releases` tab.

## Licensing

socklog itself is available under a BSD 3-clause license, see `COPYING`.

The compile script is from Laurent Bercot's "skarnet' build system, it's
available under an ISC license. The text of that license is available in
the configure script.
