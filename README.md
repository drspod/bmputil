bmputil
=======

`bmputil` is a CLI program for processing raw RGB data where no size information is known, for example when extracting game textures. `bmputil` can automatically detect the width of the image represented by such header-less data streams, and write a formatted image to disk in any image format supported by ImageMagick (`.png` by default).

Supported input formats are High Color (15bpp/16bpp), both big- and little-endian byte orderings, True Color (24bpp), and with arbitrary color channel orderings (default: BGR).

Usage
-----

```
$ ./bmputil --help
Usage: bmputil [OPTION...] input-file [output-file]

  Automatically detects the image width of raw RGB data, and writes a formatted
  image to a file. If the width is already known then detection can be disabled
  with [-w] to just write the output image file. If only width detection is
  required then writing of output file can be disabled [--detect-only].
  
  input-file                 input bitmap file path
  output-file                output file path. File extension determines image
                             format [default: png]
  Input options:
  -b, --bpp=int              bits per pixel of the input file (valid values:
                             15, 16, 24) [default: 15]
      --big-endian           15/16bpp values are in big-endian order [default:
                             yes]
      --little-endian        15/16bpp values are in little-endian order
                             [default: no]
  -c, --channels=BGR|RGB|... Ordering of color channels [default: BGR]
  -o, --offset=int           Offset into the file of the start of RGB data
                             [default: 0]
  Output options:
      --flip-h               Flip output horizontally
      --flip-v               Flip output vertically
  -n, --detect-only          Do not write output file, just detect and print
                             the width
  Width Detection Options:
      --max-width=int        maximum image width. Widths up to three times this
                             value will still be detected, but with diminishing
                             accuracy [default: 640]
      --min-width=int        minimum image width. [default: 16]
  -w, --width=int            Specify image width (disables auto-detection)
                             [default: auto]
  
  -q, --quiet                suppress all output
  -v, --verbose              display verbose output
  -x, --debug                write auto-correlation data to debug output file
  
  -?, --help                 Give this help list
      --usage                Give a short usage message
  -V, --version              Print program version

Mandatory or optional arguments to long options are also mandatory or optional
for any corresponding short options.

```

Build
-----

Building from source is tested under Linux and Cygwin. Building under WSL, MSYS or any other Unix-likes is possible but not tested.

You will need the development files for ImageMagick MagickCore version 6.x.

For example on Debian/Ubuntu:

    sudo apt install libmagickcore-dev

Under Cygwin install the following packages (in addition to `binutils`, `gcc`, `make` etc.):

    * pkg-config
    * libargp
    * libargp-devel
    * libMagickCore6_6
    * libMagick-devel (6.x)

ImageMagick v6.x packages for other platforms can be found here: https://legacy.imagemagick.org/script/download.php

If you are building on a system which does not provide `libargp` then you will need to build and install it from: https://github.com/alexreg/libargp . When building argp from these sources, you should first run the `./update-source` script to fetch the latest `gnulib` sources. This requires the `gnulib-tool` (`sudo apt install gnulib`).

To build `bmputil` simply:

```
$ cd bmputil
$ make
```

