# dvplayer

This program plays back DV format video files via a FireWire/IEEE1394 port.

## Required libraries
```
libiec61883-dev
libraw1394-dev
libncurses-dev
```

## Building
```
make
```

## Running
The simplest way is to simply play back a video file from disk like so:
```
./dvplayer Video.dv
```

Other options are described in the internal help, accessible via `-h` or `--help`.