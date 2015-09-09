
Built on top of a command line guitar tuner (see info about it below) to make a program to aid musicians with hitting pitches more accurately. The program takes microphone input and scores the user based on pitch accuracy. Two scores are generated--one operates on a "hit or miss" basis; the other gives points based on degree of sharpness or flatness. The program also provides information on "problem" notes (notes that the user tends to miss) and "problem" intervals (intervals that cause the user to miss notes).

guitartuner
===========

Simple guitar tuner sample app with BSD license.

There are plenty of tuners out there, this one is designed to be easy to understand and follow.
It should also be easy to port to other platforms, though right now it compiles only on OS X and Linux.

To Compile:
-----------

0. OS X and Linux only :(patches for other apps accepted!)
1. Download and install portaudio.
   - For OS X, use homebrew or macports to install -- `port install portaudio`.
   - For Ubuntu, use `apt-get install portaudio19-dev`.
2. run "make"
3. the output is ./tuner

Copyright
---------

Tuner Copyright (C) 2012 by Bjorn Roche

FFT Copyright (C) 1989 by Jef Poskanzer

Permission to use, copy, modify, and distribute this software and its documentation for any purpose and without fee is hereby granted, provided that the above copyright notice appear in all copies and that both that copyright notice and this permission notice appear in supporting documentation. This software is provided "as is" without express or implied warranty.

More Info
---------

This tuner app works by calculating the magnatude of the FFT, which is not the ideal method, but
it works well enough.

You can find more info about this code on Bjorn Roche's blog:
http://blog.bjornroche.com/2012/07/frequency-detection-using-fft-aka-pitch.html
