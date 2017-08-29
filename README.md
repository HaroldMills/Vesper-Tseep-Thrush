# Vesper-Tseep-Thrush

This repository contains materials documenting the development of new versions of the [Old Bird Tseep and Thrush](http://oldbird.org/analysis.htm) nocturnal flight call detectors for the [Vesper](https://github.com/HaroldMills/Vesper) project.

The Tseep and Thrush detectors were created in the late 1990s by Steve Mitchell and Bill Evans for Old Bird, Inc. They were based on an algorithm developed in 1994 by Harold Mills at the Bioacoustics Research Program of the Cornell Lab of Ornithology. The detectors have been distributed for free from the [Old Bird web site](http://oldbird.org/analysis.htm) since the late 1990s. In the following we will refer to these detectors (more precisely, to the `Tseep-x.exe` and `Thrush-x.exe` Windows executables) as the *old detectors*, and to our new versions of them as the *new detectors*.

The old detectors have several limitations that are overcome by the new ones:

1. They run only on single-channel input sampled at 22050 hertz.
2. They run only on Windows computers.
3. They cannot run on more than one input file on the same computer at the same time.

Furthermore, the old detectors were implemented using [Simulink](https://www.mathworks.com/products/simulink/) and the Real-Time Workshop (now [Simulink Coder](https://www.mathworks.com/products/simulink-coder/)), which are prohibitively expensive for many developers. The new detectors are implemented in Python, which is free, thus eliminating the price barrier for developers who might want to improve the new detectors or adapt them for new applications.

To develop the new detectors, we worked with the Old Bird `Tseep-x.exe` and `Thrush-x.exe` programs, as well as some Simulink and C source code for similar detectors. The source code was graciously provided by Bill Evans of Old Bird, Inc. Unfortunately, the precise source code from which the `Tseep-x.exe` and `Thrush-x.exe` executables were generated could not be located. In its place we worked with what we believe to be very similar source code dating from around the time that the executables were generated. That source code includes some parameter values that we know differ from those of the executables, but we have assumed that the algorithms of the source code and the executables are the same. Both the source code and the executables are in the [Old Bird](https://github.com/HaroldMills/Vesper-Tseep-Thrush/tree/master/Old%20Bird) directory of this repository.

To determine the parameter values employed in the old detector executables, we inspected the parameter values of the available source code (some of which we believe are the same in the source code and the executables, and others of which are similar but not identical), consulted with Bill Evans (who generated the old detector executables and retains some memory of the parameter values he chose for them), extracted filter coefficients from the executables, and compared the output of the old and new detectors. At this point we are confident that we have identified the parameter values of the old detectors, since a great majority of the clips output by the old and new detectors for a given recording (often 98 percent or more, though sometimes only about 90 percent, depending on the recording) are *identical*, i.e. have the same start and end indices. We are not sure what is causing the remaining differences, but we suspect they are due to slight implementation differences in the old and new detectors and/or bugs in one or both, rather than in the parameter values. The file [differences.md](https://github.com/HaroldMills/Vesper-Tseep-Thrush/tree/master/differences.md) discusses possible sources of the differences in more detail.

The `Old Bird` directory of this repository contains the old detectors, as well as the available source code relating to them. The main repository directory contains several Python scripts and iPython notebooks that were used to study the old detectors and build the new ones. Some of these are:
* `convert_mdl_file_to_json.py` - Converts a Simulink MDL file to JSON for easier viewing.
* `extract_old_bird_detector_filter.py` - Extracts the bandpass FIR filter of an old detector executable to a text file.
* `Old Bird Detector Filter Frequency Response.ipynb` - Plots the frequency response of an extracted old detector filter.
* `Even Length Least Squares Filter Design.ipynb` - Tests the code that designs FIR filters in the new detectors.
* `Old Bird Detector Filter Comparison.ipynb` - Compares the frequency response of an extracted old detector filter to that of a filter designed for a new detector.
* `Old Bird Detector Reimplementation.ipynb` - Demonstrates the processing stages of the new detector.
* `test_detector.py` - Runs one or more tests that help identify old detector parameter values or compare the old and new detectors.

There are two versions of the new detectors in this repository, versions 0.0 and 1.1. Version 0.0, in module `new_detector_0_0`, processes an input signal in one chunk. Version 1.1, in module `new_detector_1_1`, processes an input signal in multiple chunks of a limited size, retaining state across chunks as needed so that the detected clips are the same as those that would have been detected if the input had been processed as a single chunk. Version 1.1 also fixes some bugs present in version 0.0. We retain version 0.0 mostly to document the development of the new detectors. Version 1.1 is derived from and should be functionally identical to version 1.1 of the [Vesper](https://github.com/HaroldMills/Vesper) repository.

Many thanks to [MPG Ranch](http://mpgranch.com), [Old Bird](http://oldbird.org), and an anonymous donor for financial support of the Vesper project.
