# History
* Program:   NDI Combined API C Interface Library
* Creator:   David Gobbi
* Language:  English
* Authors:    
  * David Gobbi
  * Andras Lasso <lassoan@queensu.ca>
  * Adam Rankin <arankin@robarts.ca>
* Version: 1.4
  * Date: 2005/07/01
* Version: 1.5
  * Date: 2015/05/30
* Version: 1.6
  * Date: 2016/03/08

# Overview

This package provides a portable C library that provides a straightforward interface to AURORA, POLARIS, and VEGA systems manufactured by Northern Digital Inc. This library is provided by the Plus library, and is not supported by Northern Digital Inc.

The contents of this package have been built successfully under a wide range of compilers. It is a [CMake](https://cmake.org/download/) project and can be configured and built as such.

## Building
To build, configure first using CMake, then build according to your chosen generator.

### Python
To build the [Python][python] extension module of this library:

1. Build **and** install the C++ library first.
1. Then build and install the Python extension: `pip install .` (assuming the command is called in your repo clone directory)

[python]: http://www.python.org

You can test your installation by running `python -c 'import ndicapy'`

## Contents
The main contents of this package are as follows:

1) A C library (libndicapi.a, ndicapi.lib/dll) that provides a set of C functions for communicating with an NDI device via the NDI Combined API.  The documentation for this library is provided in the ndicapi_html directory.

2) Two C++ header files (ndicapi.h and ndicapi_math.h) that provide and interface, via libndicapi.a, to an NDI device via the NDICAPI Serial Communications API that predated the Combined API. Documentation is provided in the polaris_html directory.

4) A python interface to the ndicapi library.  However, only the original POLARIS API is supported through python.  The full ndicapi interface is not yet supported.