"""Reimplementation of Old Bird Tseep and Thrush detectors."""


import math

import numpy as np
import scipy.signal as signal

import dsp_utils


# TODO: Figure out how to run the detector on signals too large to
# process as a single chunk. One approach would be to run the detector
# on a sequence of overlapping chunks and discard duplicate clips.
# Another approach would be to process contiguous chunks, carrying
# state across the chunks.


def detect(samples, settings):
    
    """Runs the Old Bird detector reimplementation on the specified samples."""
    
    # See note in the `_normalize_samples` documentation below
    # explaining why input normalization is disabled.
#     samples = _normalize_samples(samples, settings)

    crossings = _get_threshold_crossings(samples, settings)
    _append_end_crossing(crossings, len(samples))
    clips = _get_clips(crossings, settings)
    clips = _merge_clips(clips)
    return clips
    
    
def _normalize_samples(samples, settings):
    
    """
    Optionally normalizes detector input samples.
    
    Because of the way MATLAB reads audio files, the original Old Bird
    detectors normalized audio samples on input to the range [-1, 1].
    I'm not sure whether the samples are normalized by dividing by 32768
    or mapping [-32768, 32767] linearly to the range [-1, 1], so I have
    implemented both options. (I searched online, including in the
    MATLAB documentation, for details about the normalization but did
    not find a clear explanation. We could, of course, figure it out
    if we had MATLAB, but I don't think we really need to, as explained
    below.)
    
    In various tests of both the Tseep and Thrush detectors on many
    different transients, I saw no case in which the output of a detector
    varied with the normalization. This is not surprising. One of the
    normalization types is a simple scaling, which should make no
    difference since the detector is scale-invariant. The other
    normalization type is almost a scaling but also adds a slight offset.
    The offset should be removed by the detector's bandpass filter,
    however, so that the filter output should only be a scaled version
    of what it would be without normalization.
    
    For the above reasons I have disabled normalization in the `detect`
    function.
    """
    
    normalization = settings.input_normalization
    
    if normalization == 'None':
        print('no input normalization')
        return samples
    
    elif normalization == 'Scale':
        print('input normalization "{}".'.format(normalization))
        return samples / 32768.
    
    elif normalization == 'Scale and Offset':
        print('input normalization "{}".'.format(normalization))
        low = -32768.
        high = 32767.
        return -1 + 2 * (samples - low) / (high - low)
    
    else:
        raise ValueError(
            'Unrecognized input normalization type "{}".'.format(normalization))
    

def _get_threshold_crossings(x, settings):

    s = settings
    
    # Design filter.
    f0 = s.f0
    f1 = s.f1
    bw = s.bw
    fs2 = s.sample_rate / 2
    bands = np.array([0, f0 - bw, f0, f1, f1 + bw, fs2]) / fs2
    desired = np.array([0, 0, 1, 1, 0, 0])
    h = dsp_utils.firls(s.filter_length, bands, desired)
    
    # Filter.
    x = signal.fftconvolve(x, h)
    
    # Take magnitude squared.
    x = x * x
    
    # Integrate.
    integration_time = s.integration_time
    x = np.cumsum(x) # How long must x be for this to fail?
    x = (x[integration_time:] - x[:-integration_time]) / integration_time
    
    # Take ratios.
    # We use `math.floor` here rather than `round` since the Simulink
    # .mdl files we have access to suggest that the Simulink detector
    # used MATLAB's `fix`  function, which rounds toward zero.
    delay = math.floor(s.delay * s.sample_rate)
    x = x[delay:] / x[:-delay]
    
    # Compare ratios to threshold.
    rf = np.zeros(len(x))
    rf[x > s.threshold] = 1
    rf[x < 1. / s.threshold] = -2
    
    # Take differences to mark outward-going threhold crossings.
    # Outward crossings of the upper threshold will be marked with
    # a 1 while outward crossings of the lower threshold will be
    # marked with a -2.
    rf = np.diff(rf)
    
    # Find indices of outward-going threshold crossing events.
    # The offset that is added to each event index compensates for the
    # toffsets that are effectively subtracted by the integration time
    # difference, power ratio, and rf difference computations above.
    offset = integration_time + delay + 1
    rises = np.where(rf == 1)[0] + offset
    falls = np.where(rf == -2)[0] + offset
    
    crossings = sorted([(r, True) for r in rises] + [(f, False) for f in falls])
    
    return crossings
    
    
def _append_end_crossing(crossings, end_index):
    
    """
    Append downward crossing at the end of the input.
    
    We include such a crossing to terminate clips that start more than
    the minimum clip duration before the end of the input but for which
    for whatever reason we do not see downward crossings.
    """
    
    crossings.append((end_index, False))
    
    
# TODO: Write unit tests for `_get_clips` function.


_STATE_DOWN = 0
_STATE_UP = 1
_STATE_HOLDING = 2


def _get_clips(crossings, settings):
    
    s = settings
    
    # We use `math.floor` here rather than `round` since the Simulink
    # .mdl files we have access to suggest that the Simulink detector
    # used MATLAB's `fix`  function, which rounds toward zero.
    min_length = int(math.floor(s.min_duration * s.sample_rate))
    max_length = int(math.floor(s.max_duration * s.sample_rate))

    state = _STATE_DOWN

    start_index = 0
    """index of start of current pulse."""

    clips = []
    
    for index, rise in crossings:

        if state == _STATE_DOWN:

            if rise:
                # rise while down

                # Start new pulse.
                start_index = index
                state = _STATE_UP

            # Do nothing for fall while down.

        elif state == _STATE_UP:

            if rise:
                # rise while up

                if index > start_index + max_length:
                    # rise after end of maximal pulse

                    # Emit maximal pulse.
                    clip = _create_clip(start_index, max_length, s)
                    clips.append(clip)

                    # Start new pulse.
                    start_index = index

                # Do nothing for rise before end of maximal pulse.

            else:
                # fall while up

                if index < start_index + min_length:
                    # fall before end of minimal pulse

                    state = _STATE_HOLDING

                else:
                    # fall at or after end of minimal pulse

                    length = index - start_index

                    # Truncate pulse if after end of maximal pulse.
                    if length > max_length:
                        length = max_length

                    clip = _create_clip(start_index, length, s)
                    clips.append(clip)
                    state = _STATE_DOWN

        else:
            # holding short pulse

            if rise:
                # rise while holding short pulse

                if index > start_index + min_length:
                    # rise after end of minimal pulse

                    # Emit minimal pulse.
                    clip = _create_clip(start_index, min_length, s)
                    clips.append(clip)

                    # Start new pulse.
                    start_index = index
                    state = _STATE_UP

                # Do nothing for rise at or before end of minimal pulse.

            else:
                # fall while holding short pulse

                if index >= start_index + min_length:
                    # fall at or after end of minimal pulse

                    # Emit minimal pulse.
                    clip = _create_clip(start_index, min_length, s)
                    clips.append(clip)

                    state = _STATE_DOWN

                # Do nothing for fall before end of minimal pulse.
                
    return clips


def _create_clip(start_index, length, settings):
    start_index -= settings.initial_padding
    length += settings.initial_padding + settings.final_padding
    return (start_index, length)
    
    
def _merge_clips(clips):
    
    """
    Merges overlapping and contiguous clips.
    
    The Old Bird detectors do this, though only implicitly when two
    or more of the pulses generated to control the clipping and saving
    of transients happen to merge into a single pulse. Because we
    implement the detector in a somewhat different way, we must
    merge clips in a separate, explicit step.
    
    I don't think the merging is really a good idea, so perhaps we
    should make it an option for those who want clips that match as
    closely as possible thase that would be generated by the
    original Old Bird detectors?
    """
    
    if len(clips) < 2:
        return clips
    
    else:
        # at least two clips
        
        merged_clips = []
        
        start_a, length_a = clips[0]
        
        for start_b, length_b in clips[1:]:
            
            end_a = start_a + length_a
            
            if end_a >= start_b:
                # clips a and b overlap or are contiguous
                
                # Merge clip b into clip a.
                end_b = start_b + length_b
                end_a = max(end_a, end_b)
                length_a = end_a - start_a
                
            else:
                # there is at least one sample between clips a and b
                
                # Append clip a to result.
                merged_clips.append((start_a, length_a))
                
                start_a = start_b
                length_a = length_b
                
        # Append final clip to result.
        merged_clips.append((start_a, length_a))
        
    return merged_clips
