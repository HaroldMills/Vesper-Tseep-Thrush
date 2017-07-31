# Vesper-Tseep-Thrush

This repository contains materials documenting the development of new versions of the [Old Bird Tseep and Thrush](http://oldbird.org/analysis.htm) nocturnal flight call detectors for the [Vesper](https://github.com/HaroldMills/Vesper) project. While the new detectors were initially developed here, in late June, 2017 they were transferred to the [Vesper repository](https://github.com/HaroldMills/Vesper), where their development has continued. This repository will remain, but for the latest detector code please see [here](https://github.com/HaroldMills/Vesper/tree/master/vesper/old_bird).

The Tseep and Thrush detectors were created in the late 1990s by Steve Mitchell and Bill Evans for Old Bird, Inc. They have several limitations that will be overcome by the new detectors:

1. They run only on single-channel input sampled at 22050 hertz.
2. They run only on Windows computers.
3. They cannot run on more than one input file on the same computer at the same time.

Furthermore, the old detectors were implemented using [Simulink](https://www.mathworks.com/products/simulink/) and the Real-Time Workshop (now [Simulink Coder](https://www.mathworks.com/products/simulink-coder/)), which are prohibitively expensive for many developers. The new detectors are implemented in Python, which is free, thus eliminating the price barrier for developers who might want to improve the new detectors or adapt them for new applications.

The development of the new detectors required a careful examination of the old ones to determine both the detection algorithm and the particular sets of algorithm parameter values employed. Unfortunately, this examination was complicated by the fact that the precise source code from which the old detectors were created is not available. Source code very similar to that code is available, however, implementing the same algorithm but with some differences in parameter values. Through a combination of inspection of the available source code, consultations with Bill Evans, who created the old detectors and retains some memory of the parameter values he chose for them, extraction of filter coefficients from the old detector executables, and tests of the old and new detectors on carefully designed synthetic inputs, we were able to identify the parameter values employed by the old detectors.

This repository contains the Old Bird Tseep and Thrush detectors, as well as the available source code relating to them, in the `Old Bird` directory. The main repository directory contains several Python scripts and iPython notebooks that were used to study the old detectors and build the new ones. Some of these are:
* `convert_mdl_file_to_json.py` - Converts a Simulink MDL file to JSON for easier viewing.
* `extract_old_bird_detector_filter.py` - Extracts the bandpass FIR filter of an old detector executable to a text file.
* `Old Bird Detector Filter Frequency Response.ipynb` - Plots the frequency response of an extracted old detector filter.
* `Even Length Least Squares Filter Design.ipynb` - Tests the code that designs FIR filters in the new detectors.
* `Old Bird Detector Filter Comparison.ipynb` - Compares the frequency response of an extracted old detector filter to that of a filter designed for a new detector.
* `Old Bird Detector Reimplementation.ipynb` - Demonstrates the processing stages of the new detector.
* `test_detector.py` - Runs one or more tests that help identify old detector parameter values or compare the old and new detectors.

There are two new versions of the Old Bird detectors in this repository. One, in module `new_detector`, processes an input signal in one chunk. The other, in module `old_bird_detector_redux`, processes an input signal in multiple chunks of a limited size. The latter version also fixes some bugs present in the portion of the former version that finds transients from a sequence of threshold crossings, and should be preferred for actual use.

Many thanks to [MPG Ranch](http://mpgranch.com), [Old Bird](http://oldbird.org), and an anonymous donor for financial support of the Vesper project.
