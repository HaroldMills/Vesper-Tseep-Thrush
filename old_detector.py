"""Module containing function that runs the original Old Bird detectors."""


import itertools
import os
import re
import subprocess
import time
import wave

import numpy as np

import numpy_utils
import os_utils


_INPUT_DIR_PATH = r'C:\My Recordings'
_INPUT_FILE_PATH = os.path.join(_INPUT_DIR_PATH, 'Soundfile.wav')
_OUTPUT_DIR_PATH = r'C:\temp\calls'
_STOP_FILE_PATH = r'C:\stop.txt'
_WAVE_SAMPLE_DTYPE = np.dtype('<i2')

_MIN_CLIP_FILE_PROCESSING_DELAY = 1

_SLEEP_DURATION = .1

_DETECTOR_NAMES = ('Tseep', 'Thrush')
_CLIP_FILE_NAME_PATTERN_FORMAT = r'^{}_\d\d\d\.\d\d\.\d\d_\d\d\.wav$'

def _create_clip_file_name_re(detector_name):
    pattern = _CLIP_FILE_NAME_PATTERN_FORMAT.format(detector_name)
    return re.compile(pattern)

_CLIP_FILE_NAME_RES = dict(
    (name, _create_clip_file_name_re(name)) for name in _DETECTOR_NAMES)


def detect(samples, settings):
    
    """
    Runs one of the original Old Bird detectors on the specified samples.
    """
    
    sample_rate = settings.sample_rate
    length = len(samples)
    samples = _append_sentinel(samples, sample_rate)
    _write_input_file(samples, sample_rate)
    clips = _run_detector(settings.detector_name, samples, length)
    return clips
    
    
def _append_sentinel(samples, sample_rate):
    
    """
    Appends a sentinel noise burst to the end of the specified samples.
    
    Assumes that the first second of the input samples are background noise,
    and creates background noise for the sentinel by repeating that noise.
    
    The `_gather_clips` function knows that detection has finished when
    the detector produces a clip whose index range intersects that of the
    sentinel. The `_gather_clips` function assumes that the detector will
    always detect the sentinel pulse and produce such a clip.
    """
    
    # We start the sentinel with 30 seconds of background noise to ensure
    # that the sentinel is not ignored due to detector clip suppression.
    
    sentinel_duration = 32
    burst_time = 30
    burst_duration = .01
    burst_amplitude = 10000
    
    sentinel_length = int(round(sentinel_duration * sample_rate))
    sentinel = np.zeros(sentinel_length)
    
    noise_length = int(round(sample_rate))
    noise = samples[:noise_length]
    
    # Copy noise
    start_index = 0
    while start_index < sentinel_length:
        length = min(noise_length, sentinel_length - start_index)
        sentinel[start_index:start_index + length] = noise[:length]
        start_index += length
        
    start_index = int(round(burst_time * sample_rate))
    length = int(round(burst_duration * sample_rate))
    end_index = start_index + length
    factor = burst_amplitude / np.max(np.abs(sentinel[start_index:end_index]))
    sentinel[start_index:end_index] *= factor
    
    return np.concatenate((samples, sentinel))


def _write_input_file(samples, sample_rate):
    
    samples = np.array(samples, dtype=_WAVE_SAMPLE_DTYPE)
    
    with wave.open(_INPUT_FILE_PATH, 'wb') as writer:
        
        num_channels = 1
        sample_size = 2
        sample_rate = int(round(sample_rate))
        length = 0
        compression_type = 'NONE'
        compression_name = 'not compressed'
        
        writer.setparams((
            num_channels, sample_size, sample_rate, length,
            compression_type, compression_name))

        writer.writeframes(samples.tostring())
        
        
def _run_detector(detector_name, samples, sentinel_start_index):
    
    clip_lists = []
    found_sentinel = False
    
    detector_process = _start_detector(detector_name)
    
    while not found_sentinel:
        time.sleep(_SLEEP_DURATION)
        clips, found_sentinel = \
            _gather_clips(detector_name, samples, sentinel_start_index)
        clip_lists.append(clips)
        
    detector_process.terminate()

    clips = list(itertools.chain.from_iterable(clip_lists))
    
    return clips


def _start_detector(detector_name):
    _prepare_output_dir(detector_name)
    executable_name = _get_detector_executable_name(detector_name)
    return subprocess.Popen([executable_name], stderr=subprocess.PIPE)


def _prepare_output_dir(detector_name):
    
    log_path = _get_detector_log_path(detector_name)
    os_utils.delete_file(log_path)
    
    pattern = _CLIP_FILE_NAME_PATTERN_FORMAT.format(detector_name)
    os_utils.delete_files(_OUTPUT_DIR_PATH, pattern)
    
    os_utils.create_file(log_path)


def _get_detector_log_path(detector_name):
    file_name = 'Log{}.txt'.format(detector_name)
    return os.path.join(_OUTPUT_DIR_PATH, file_name)


def _get_detector_executable_name(detector_name):
    return '{}-x.exe'.format(detector_name)

    
def _gather_clips(detector_name, samples, sentinel_start_index):

    clip_file_name_re = _CLIP_FILE_NAME_RES[detector_name]
    
    file_paths = os_utils.list_files(_OUTPUT_DIR_PATH, clip_file_name_re)
    
    # Sort file paths in order of detection time.
    file_paths.sort()
    
    clips = []
    found_sentinel = False
    
    for file_path in file_paths:
        
        # We've had problems attempting to read sound files before
        # the Old Bird detector has finished writing them, so we
        # do not attempt to read a file if it was last modified
        # within a certain number of seconds of the current time.
        mod_time = os.path.getmtime(file_path)
        if time.time() - mod_time < _MIN_CLIP_FILE_PROCESSING_DELAY:
            break
        
        # print('processing clip file "{}"'.format(file_path))
        
        clip_samples = _read_clip_file(file_path)
        
        start_indices = numpy_utils.find(clip_samples, samples, 1)
        
        if len(start_indices) > 0:
            
            start_index = start_indices[0]
            length = len(clip_samples)
            
            if start_index + length >= sentinel_start_index:
                # clip intersects sentinel
            
                found_sentinel = True
                
            else:
                # clip precedes sentinel
                
                clips.append((start_indices[0], len(clip_samples)))
                
        else:
            print('Clip "{}" not found in input signal.'.format(file_path))

        os_utils.delete_file(file_path)

    return (clips, found_sentinel)


def _read_clip_file(file_path):
    
    with wave.open(file_path, 'rb') as reader:
        
        length = reader.getparams().nframes
        string = reader.readframes(length)
        samples = np.frombuffer(string, dtype=_WAVE_SAMPLE_DTYPE)
        return samples
