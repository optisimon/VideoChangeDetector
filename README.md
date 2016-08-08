# VideoChangeDetector

This is a tool specifically written to extract possible timestamps of close range lightning strikes
from recorded video.


## Description

This initial naive implementation looks for changes of the average intensity level for the entire frame of video. Frames which have a sufficiently large deviation in intensity from the previous frame is classified as a possible lightning strike.

This was enough for all the lightning videos I needed to process this time, so I might not improve it any further any time soon.

## Compiling

Compiles without warning on ubuntu 14.04 with gcc 4.8.4

  sudo make prepare
  make
  sudo make install

## Usage
  Usage: VideoChangeDetector [options] [-f input_video_filename]
    -v, --verbose                Show additional debugging output
    -b, --brief                  Do not show additional debugging output (default)
    -h, --help                   Show this help text
    -f FILE, --file FILE         Specify input video file to look for flashes in
    -t value, --threshold value  Average global intensity increase above given value
                                 is counted as a lightning event


## Alternative use (as a gstreamer example application)

You could also view this as an example application for how to use gstreamer-1.0, to forward all video frames from any video file (in a format supported by gstreamer) into a receiver callback. Since I only cared about the intensity values, this application only forwards grayscale samples to our callback (but changing it to forward color frames is straightforward).


## Disclaimer

Works on my machine, with my lightning videos. YMMV.


## Copyright

Copyright (c) 2016 Simon Gustafsson (www.optisimon.com)

Do whatever you like with this code, but please refer to me as the original author.
