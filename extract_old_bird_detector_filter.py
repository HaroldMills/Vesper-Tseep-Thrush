"""
Script that extracts the bandpass FIR filter of an Old Bird detector executable.

The name of the detector ("Tseep" or "Thrush") is specified on the
command line. The detector executable ("Tseep-r.exe" or "Thrush-r.exe")
should be in the working directory, typically the directory that holds the
script. The filter coefficients are written to a text file
("Old Bird Tseep Detector Filter.txt" or "Old Bird Thrush Detector Filter.txt")
in the same directory.

The filter of an Old Bird detector executable is stored as the double
precision complex length-16384 DFT of the zero-padded filter coefficients.
The real part of the DFT is stored before the complex part. The real
part has even symmetry, and we locate it within the file by searching
for an even-symmetric sequence of double precision floating point
numbers of length 16383 (the real part of the DC bin of the DFT is
stored just before this symmetric sequence). Once we locate the DFT
we take its inverse and truncate the real part of the result to 100
elements (the filter length) to recover the filter coefficients.
"""


import sys

import numpy as np


DETECTOR_FILE_NAME_TEMPLATE = 'Old Bird/{}-r.exe'
FILTER_FILE_NAME_TEMPLATE = 'Old Bird {} Detector Filter.txt'
FILTER_DFT_SIZE = 16384
NUMBER_SIZE = 8
FILTER_LENGTH = 100
SAMPLE_RATE = 22050.


def main():
    detector_name = sys.argv[1]
    extract_old_bird_detector_filter(detector_name)
#     _test_find_symmetric_sequences()
    
    
def extract_old_bird_detector_filter(detector_name):
    H = _extract_filter_dft_from_file(detector_name)
    h = _compute_filter_coefficients(H)
    _write_filter_coefficients_to_file(detector_name, h)
    print('done')
    
    
def _extract_filter_dft_from_file(detector_name):
    
    # Read detector executable into memory.
    detector_file_name = DETECTOR_FILE_NAME_TEMPLATE.format(detector_name)
    print('reading detector executable "{}"...'.format(detector_file_name))
    with open(detector_file_name, 'rb') as file_:
        data = file_.read()
        
    # Find symmetric part (all but DC coefficient) of real part of filter DFT.
    # This is an even-symmetric sequence of 16383 double-precision floating
    # point numbers.
    print('searching for filter DFT...')
    index = _find_symmetric_sequences(data, FILTER_DFT_SIZE - 1, NUMBER_SIZE)[0]
    
    # Adjust index to include real part of DC coefficient, which immediately
    # precedes the sequence just found.
    index -= NUMBER_SIZE
    
    print('extracting filter DFT...')
    
    # Read filter DFT. The DFT comprises a length 16384 real part followed
    # by a length 16384 imaginary part.
    H = data[index:index + 2 * FILTER_DFT_SIZE * NUMBER_SIZE]
    H = np.frombuffer(H, dtype='float64')
    H = H[:FILTER_DFT_SIZE] + 1j * H[FILTER_DFT_SIZE:]
    
    return H


def _find_symmetric_sequences(data, seq_length, num_size):
    indices = []
    n = len(data)
    chunk_size = seq_length * num_size
    i = 0
    while i + chunk_size <= n:
        seq = data[i:i + chunk_size]
        if _is_sequence_symmetric(seq, num_size) and _is_not_all_zeros(seq):
            indices.append(i)
        i += 1
    return indices
    
    
def _is_sequence_symmetric(seq, num_size):
    n = len(seq)
    m = int(n / num_size) // 2
    for i in range(m):
        j = i * num_size
        k = n - (i + 1) * num_size
        if seq[j:j + num_size] != seq[k:k + num_size]:
            return False
    return True

    
def _is_not_all_zeros(seq):
    for i in seq:
        if i != 0:
            return True
    return False


def _compute_filter_coefficients(H):

    print('computing filter coefficients...')
    
    # Compute filter coefficients.
    h = np.fft.ifft(H).real[:FILTER_LENGTH]
    
    # _show_filter_asymmetries(h)
    
    # Make filter symmetric. Taking the filter's DFT (when the detector
    # was created) and then inverting that DFT (above) has made it
    # ever so slightly asymmetric due to limited precision arithmetic.
    # Making the filter symmetric again probably isn't necessary, since
    # the introduced asymmetry is so small (the differences between
    # coefficients that are supposed to be the same are several orders
    # of magnitude smaller than the errors we would introduce if we
    # rounded the coefficients to single precision, which would be
    # perfectly acceptable), but I've chosen to do it anyway.
    half = h[:FILTER_LENGTH // 2]
    h = np.concatenate((half, np.flipud(half)))
    
    return h


def _write_filter_coefficients_to_file(detector_name, h):
    
    # Write filter coefficients to file.
    filter_file_name = FILTER_FILE_NAME_TEMPLATE.format(detector_name)
    print('writing filter coefficients to file "{}"...'.format(
        filter_file_name))
    with open(filter_file_name, 'w') as file_:
        for c in h:
            file_.write('{:.20e}\n'.format(c))


def _show_filter_asymmetries(h):
    n = len(h) // 2
    for i in range(n):
        print(h[i] - h[-(i + 1)])
        
    
def _test_find_symmetric_sequences():
    
    cases = [
        (([0, 1, 2, 3, 4, 3, 2, 0, 1], 5, 1), [2]),
        (([0, 1, 2, 0, 0, 1, 1, 2, 2, 1, 1, 0, 0, 1, 2, 3], 5, 2), [3])
    ]
    
    for args, expected in cases:
        result = _find_symmetric_sequences(*args)
        if result != expected:
            print('test failed: {} is not {}.'.format(result, expected))    


if __name__ == '__main__':
    main()
    