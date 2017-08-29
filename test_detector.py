"""
Script that runs one or more tests that help to identify parameter
values of the Old Bird Tseep or Thrush detector, or compare output
of one of the old detectors to that of its reimplementation.
"""


import math
import sys

import numpy as np

from bunch import Bunch
import clip_utils
import new_detector
import new_detector_1_1
import old_detector
import sound_file_utils


_DETECTOR_MODULES = {
    'Old': old_detector,
    'New': new_detector,
    'New 1.1': new_detector_1_1
}

_DETECTOR_VERSIONS = ('Old', 'New 1.1')

_SAMPLE_RATE = 22050

_DETECTOR_INFOS = {
    
    'Tseep': Bunch(
        
        detector_name='Tseep',
        
        just_detectable_impulse_amplitude=2489,
        just_detectable_tone_amplitude=108,
        
        integration_time_bracket=(500, 3000),
        example_inband_frequency=8000,
        passband_bracket=(4000, 11000),
        filter_length_bracket=(90, 120),
        
        sample_rate=_SAMPLE_RATE,
        f0=6000,
        f1=10000,
        bw=100,
        filter_length=100,
        integration_time=2000,
        delay=.02,
        threshold=2,
        min_duration=.100,
        max_duration=.400,
        initial_padding=3000,
        final_padding=0
        
    ),
                  
    'Thrush': Bunch(
        
        detector_name='Thrush',
        
        just_detectable_impulse_amplitude=1642,
        just_detectable_tone_amplitude=101,
        
        integration_time_bracket=(500, 8500),
        example_inband_frequency=3500,
        passband_bracket=(1000, 6000),
        filter_length_bracket=(70, 130),
        
        sample_rate=_SAMPLE_RATE,
        f0=2800,
        f1=5000,
        bw=100,
        filter_length=100,
        integration_time=4000,
        delay=.02,
        threshold=1.3,
        min_duration=.100,
        max_duration=.400,
        initial_padding=5000,
        final_padding=0
        
    )
                  
}

_INTEGRATION_TIME_TEST_IMPULSE_SEPARATION = 4000
"""
the integration time test impulse separation in samples.

The test described in the following paragraph seems to work for the
Tseep detector but not for the Thrush detector. I'm not sure why the
test fails for the Thrush detector.

You can find the approximate integration time of a detector by running the
`_run_integration_time_test` function manually with various values of
`_INTEGRATION_TIME_TEST_IMPULSE_SEPARATION`. For values smaller than some
threshold the test will yield a clip of maximal length, while for values
greater than or equal to the threshold it will yield a clip of minimal
length. The integration time is near the threshold.
"""

_DELAY_TEST_IMPULSE_SEPARATION = .0185
"""
the delay test impulse separation in seconds.

You can find the approximate delay of a detector by running the
`_run_delay_test` function manually with various values of
`_DELAY_TEST_IMPULSE_SEPARATION`. For values smaller than some threshold
the test will yield a clip, while for values greater than or equal to the
threshold it will yield no clips. The delay is near the threshold.
"""


def main():
    
    detector_name = sys.argv[1]
    detector_info = _DETECTOR_INFOS[detector_name]
    
    np.random.seed(0)

    print('for detector "{}":'.format(detector_name))
    
    # _run_short_noise_burst_test(detector_info)
    
    # _run_clip_duration_extrema_tests(detector_info)
    
    # _find_just_detectable_impulse_amplitude(detector_info)
    
    # _find_just_detectable_inband_tone_amplitude(detector_info)
    
    # _find_approximate_integration_time(detector_info)
    
    # _find_clip_suppressor_parameter_values(detector_info)
    
    # _create_impulse_train_sound_files(detector_info)
    
    # _run_delay_test(detector_info)
    
    # _run_passband_test(detector_info)
    
    # _run_filter_length_test(detector_info)
    
    # _estimate_threshold(detector_info)
    
    # _estimate_threshold_2(detector_info)
    
    _compare_detectors_on_impulses(_DETECTOR_VERSIONS, detector_info)
    
    # _compare_detectors_on_noise_bursts(_DETECTOR_VERSIONS, detector_info)

    # _compare_detectors_on_tones(_DETECTOR_VERSIONS, detector_info)
    
    # _compare_detectors_on_chirps(_DETECTOR_VERSIONS, detector_info)
        
    
def _run_clip_duration_extrema_tests(detector_info):
    
    print(
        'running test that should yield a single clip whose length is '
        'the larger of the minimum clip duration and the integration '
        'time (only approximately in the latter case) plus the padding...')
    
    s = Bunch(
        sample_rate=_SAMPLE_RATE,
        duration=10,
        background_noise_amplitude=100,
        impulse_times=[5],
        impulse_amplitudes=10000
    )
    
    samples = _create_background_noise(s)
    _add_impulses(samples, s)
    
    clips = old_detector.detect(samples, detector_info)
    
    print('clips: {}'.format(clips))
    
    print()
     
    print(
        'running test that should yield a single clip whose length is '
        'the maximum clip duration plus the padding...')
     
    s = Bunch(
        sample_rate=_SAMPLE_RATE,
        duration=20,
        background_noise_amplitude=100,
        noise_burst_times=[5],
        noise_burst_durations=10,
        noise_burst_amplitudes=10000
    )
     
    samples = _create_background_noise(s)
    _add_noise_bursts(samples, s)
     
    clips = old_detector.detect(samples, detector_info)
     
    print('clips: {}'.format(clips))
    
    
def _run_short_noise_burst_test(detector_info):
    
    # For this test the signal is a short noise burst.
    
    print('running the detector on a short noise burst...')
    
    for amplitude in range(100, 400, 10):
        
        s = Bunch(
            sample_rate=_SAMPLE_RATE,
            duration=10,
            background_noise_amplitude=100,
            noise_burst_times=[5],
            noise_burst_durations=.01,
            noise_burst_amplitudes=amplitude
        )
    
        samples = _create_background_noise(s)
        _add_noise_bursts(samples, s)
        
        clips = old_detector.detect(samples, detector_info)
    
        print(amplitude, clips)            

    
def _find_just_detectable_impulse_amplitude(detector_info):
    
    # For this test the signal is a single impulse.
    
    print('finding just-detectable impulse amplitude...')
    
    s = Bunch(
        sample_rate=_SAMPLE_RATE,
        duration=10,
        background_noise_amplitude=100,
        impulse_time=5
    )

    samples = _create_background_noise(s)
    amplitude = _find_just_detectable_impulse_amplitude_aux(
        detector_info, samples, s)
        
    print('just detectable impulse amplitude:', amplitude)

    
def _find_just_detectable_impulse_amplitude_aux(detector_info, samples, s):
    
    impulse_index = _round(s.impulse_time * s.sample_rate)

    low = 100
    high = 30000
    
    # Loop invariant: just detectable amplitude is in (low, high].
    
    while low != high - 1:
        
        # print('impulse amplitude is in ({}, {}]...'.format(low, high))
        
        mid = _round((low + high) / 2)
        
        samples[impulse_index] = mid
        
        clips = old_detector.detect(samples, detector_info)
        
        if len(clips) == 0:
            low = mid
        else:
            high = mid
            
    return high
            
            
def _find_just_detectable_inband_tone_amplitude(detector_info):
    
    # For this test the signal is a short tone.
    
    tone_frequency = detector_info.example_inband_frequency
    
    print('finding just-detectable amplitude of {} Hz tone...'.format(
        tone_frequency))
    
    s = Bunch(
        sample_rate=_SAMPLE_RATE,
        duration=10,
        background_noise_amplitude=100,
        tone_start_times=[5],
        tone_durations=.2,
        tone_frequencies=tone_frequency,
        tone_onset_duration=.01
    )

    samples = _create_background_noise(s)
    amplitude = _find_just_detectable_tone_amplitude_aux(
        detector_info, samples, s)
        
    print('just detectable tone amplitude:', amplitude)

    
def _find_just_detectable_tone_amplitude_aux(detector_info, noise_samples, s):
    
    low = 100
    high = 10000
    
    # Loop invariant: just detectable amplitude is in (low, high].
    
    while low != high - 1:
        
        # print('tone amplitude is in ({}, {}]...'.format(low, high))
        
        mid = _round((low + high) / 2)
        
        samples = np.array(noise_samples)
        s.tone_amplitudes = mid
        _add_tones(samples, s)
        
        clips = old_detector.detect(samples, detector_info)
        
        if len(clips) == 0:
            low = mid
        else:
            high = mid
            
    return high
            

def _find_approximate_integration_time(detector_info):
    
    print('finding approximate integration time...')
    
    low, high = detector_info.integration_time_bracket
    
    for sep in range(low, high, 50):
        clips = \
            _get_clip_length_for_integration_time_impulses(detector_info, sep)
        print(sep, clips)
        
    return

    max_clip_length = \
        _get_clip_length_for_integration_time_impulses(detector_info, low)
        
    # Loop invariant: approximate integration time is in (low, high].
    
    while low != high - 1:
        
        print('integration time is in ({}, {}]...'.format(low, high))
        
        mid = _round((low + high) / 2)
        
        clip_length = \
            _get_clip_length_for_integration_time_impulses(detector_info, mid)
        
        if clip_length == max_clip_length:
            low = mid
        else:
            high = mid
            
    print('approximate integration time:', high)
    
    
def _get_clip_length_for_integration_time_impulses(detector_info, separation):
    
    separation /= _SAMPLE_RATE
    num_impulses = 20
    
    amplitude = detector_info.just_detectable_impulse_amplitude
    first_amplitude = 1.1 * amplitude
    other_amplitude = .8 * amplitude
    amplitudes = [first_amplitude] + ([other_amplitude] * (num_impulses - 1))
    
    s = Bunch(
        sample_rate=_SAMPLE_RATE,
        duration=10 + (num_impulses - 1) * separation,
        background_noise_amplitude=100,
        impulse_times=5 + np.arange(num_impulses) * separation,
        impulse_amplitudes=amplitudes
    )

    np.random.seed(0)
    samples = _create_background_noise(s)
    _add_impulses(samples, s)
    
    return old_detector.detect(samples, detector_info)
    # return new_detector.detect(samples, detector_info)


def _find_clip_suppressor_parameter_values(detector_info):
    
    print('finding clip suppressor parameter values...')
    
    print('measuring count threshold...')
    threshold = _find_clip_suppressor_count_threshold(detector_info)
    print('count threshold is {}'.format(threshold))
    
    print('measuring period...')
    period = _find_clip_suppressor_period(detector_info, threshold)
    print('period is {}'.format(period))
    
    
def _create_impulse_train_sound_files(detector_info):
     
    duration = 120
    offset = 10
    separations = [.1, .12, .14, .16, .18, .2, .22, .24, .26, .3]
     
    for i, separation in enumerate(separations):
         
        span = duration - 2 * offset
        num_impulses = int(math.floor(span / separation)) + 1
         
        s = Bunch(
            sample_rate=_SAMPLE_RATE,
            duration=duration,
            background_noise_amplitude=100,
            impulse_times=offset + np.arange(num_impulses) * separation,
            impulse_amplitudes=10000
        )
         
        np.random.seed(0)
        samples = _create_background_noise(s)
        _add_impulses(samples, s)
        
        file_name = 'Test_201707{:02d}_210000.wav'.format(i + 1)
        sound_file_utils.write_sound_file(file_name, samples, _SAMPLE_RATE)
        
        
def _find_clip_suppressor_count_threshold(detector_info):
    
    num_impulses = 50
    separation = .5
    
    s = Bunch(
        sample_rate=_SAMPLE_RATE,
        duration=10 + (num_impulses - 1) * separation,
        background_noise_amplitude=100,
        impulse_times=5 + np.arange(num_impulses) * separation,
        impulse_amplitudes=10000
    )

    np.random.seed(0)
    samples = _create_background_noise(s)
    _add_impulses(samples, s)
    
    clips = old_detector.detect(samples, detector_info)
    
    return len(clips) + 1


def _find_clip_suppressor_period(detector_info, threshold):
    
    for period in range(threshold, 3 * threshold):
        
        num_impulses = threshold
        separation = (period - .5) / (threshold - 1)
    
        s = Bunch(
            sample_rate=_SAMPLE_RATE,
            duration=10 + (num_impulses - 1) * separation,
            background_noise_amplitude=100,
            impulse_times=5 + np.arange(num_impulses) * separation,
            impulse_amplitudes=10000
        )
    
        np.random.seed(0)
        samples = _create_background_noise(s)
        _add_impulses(samples, s)
        
        clips = old_detector.detect(samples, detector_info)
        
        if len(clips) == threshold:
            return period - 1
            
    
# def _run_integration_time_test(detector_info):
#     
#     # For this test the signal is a series of impulses separated by
#     # `_INTEGRATION_TIME_TEST_IMPULSE_SEPARATION` samples. The first
#     # impulse is powerful enough to trip the detector alone but the
#     # others are not. Detection will yield a clip of minimal duration if
#     # the integration time is less than the impulse separation, and a
#     # clip of maximal duration if the integration time is greater than
#     # the impulse separation.
# 
#     print('running integration time test...')
#     
#     num_impulses = 20
#     separation = _INTEGRATION_TIME_TEST_IMPULSE_SEPARATION / _SAMPLE_RATE
#     
#     amplitude = detector_info.just_detectable_impulse_amplitude
#     delta = 200
#     first_amplitude = amplitude + delta
#     other_amplitude = amplitude - delta
#     amplitudes = [first_amplitude] + ([other_amplitude] * (num_impulses - 1))
#     
#     s = Bunch(
#         sample_rate=_SAMPLE_RATE,
#         duration=10 + (num_impulses - 1) * separation,
#         background_noise_amplitude=100,
#         impulse_times=5 + np.arange(num_impulses) * separation,
#         impulse_amplitudes=amplitudes
#     )
# 
#     samples = _create_background_noise(s)
#     _add_impulses(samples, s)
#     
#     clips = old_detector.detect(samples, detector_info)
#     
#     print('clips: {}'.format(clips))

    
def _run_delay_test(detector_info):
    
    # For this test the signal is two equal-amplitude impulses separated by
    # `_DELAY_TEST_IMPULSE_SEPARATION` seconds. Neither impulse is powerful
    # enough to trip the detector alone but their combination will trip the
    # detector if they are separated by less than the delay.
    
    print('running delay test...')
    
    s = Bunch(
        sample_rate=_SAMPLE_RATE,
        duration=10,
        background_noise_amplitude=100,
        impulse_times=[5, 5 + _DELAY_TEST_IMPULSE_SEPARATION],
        impulse_amplitudes=
            detector_info.just_detectable_impulse_amplitude - 200
    )
    
    samples = _create_background_noise(s)
    _add_impulses(samples, s)
    
    clips = old_detector.detect(samples, detector_info)
    
    print('clips: {}'.format(clips))


def _run_passband_test(detector_info):
    
    s = Bunch(
        sample_rate=_SAMPLE_RATE,
        duration=10,
        background_noise_amplitude=100,
        tone_start_times=[5],
        tone_durations=.2,
        tone_amplitudes=1.414 * detector_info.just_detectable_tone_amplitude,
        tone_onset_duration=.01
    )
    
    f0, f1 = detector_info.passband_bracket
    f_inc = 100
    
#     f0 = 2700
#     f1 = 2800
#     f_inc = 10
    
    for f in range(f0, f1, f_inc):
        
        samples = _create_background_noise(s)
        s.tone_frequencies = f
        _add_tones(samples, s)
        
        clips = old_detector.detect(samples, detector_info)
        
        middle = ' not' if len(clips) == 0 else ''
        print('{} Hz tone{} detected'.format(f, middle))
        
         
def _run_filter_length_test(detector_info):
    
    tone_frequency = detector_info.f1 - 500
    
    n = 10
    jd = detector_info.just_detectable_tone_amplitude
    inc = _round(.1 * jd)
    tone_amplitudes = list(jd + (1 + np.arange(n)) * inc)
    
#     tone_amplitudes = list(1200 + np.arange(10) * 200)
    
    min_length, max_length = detector_info.filter_length_bracket
    filter_lengths = range(min_length, max_length + 1)
    
    results = np.zeros(
        (len(filter_lengths), len(tone_amplitudes), 2), dtype='int')
    
    for j, tone_amplitude in enumerate(tone_amplitudes):
        
        print('processing test tone with amplitude {}, frequency {}'.format(
            tone_amplitude, tone_frequency))
        
        # print('creating constant tone test signal...')
        s = Bunch(
            sample_rate=_SAMPLE_RATE,
            duration=10,
            background_noise_amplitude=100,
            tone_start_times=[5],
            tone_durations=.2,
            tone_amplitudes=tone_amplitude,
            tone_frequencies=tone_frequency,
            tone_onset_duration=.01
        )
        
        samples = _create_background_noise(s)
        _add_tones(samples, s)

    
        # print('running old detector...')
        old_clips = old_detector.detect(samples, detector_info)
    
        # print('running new detector...')
        for i, filter_length in enumerate(filter_lengths):
            s = detector_info
            s.filter_length = filter_length
            new_clips = new_detector.detect(samples, s)
            # print('old:', old_clips)
            # print('new:', new_clips)
            results[i, j, 0] = new_clips[0][0] - old_clips[0][0]
            results[i, j, 1] = new_clips[0][1] - old_clips[0][1]
            
    
    for i, filter_length in enumerate(filter_lengths):
        strings = [
            _format_result(results[i, j]) for j in range(len(tone_amplitudes))]
        print('{}    {}'.format(filter_length, '    '.join(strings)))
        
        
def _format_result(r):
    return '{} {}'.format(r[0], r[1])

    
def _estimate_threshold(detector_info):
    print('estimating detection threshold...')
    n = 10
    estimates = np.zeros(n)
    for i in range(n):
        estimates[i] = _estimate_threshold_aux(detector_info)
        print('estimate {} of {}: {}'.format(i + 1, n, estimates[i]))
    estimate = np.mean(estimates)
    print('mean estimate: {}'.format(estimate))
    
    
def _estimate_threshold_aux(detector_info):
     
    s = Bunch(
        sample_rate=_SAMPLE_RATE,
        duration=10,
        background_noise_amplitude=100,
        impulse_time=5
    )

    # Create test signal.
    samples = _create_background_noise(s)
    amplitude = _find_just_detectable_impulse_amplitude_aux(
        detector_info, samples, s)
    impulse_index = _round(s.impulse_time * s.sample_rate)
    samples[impulse_index] = amplitude


    # Compute threshold estimate from signal, integration time, and delay.
    
    integration_time = detector_info.integration_time
    delay = _round(detector_info.delay * s.sample_rate)
                
    start_index = impulse_index - integration_time + 1
    end_index = impulse_index + 1
    x = samples[start_index:end_index]
    impulse_power = np.sum(x * x)
    
    start_index -= delay
    end_index -= delay
    x = samples[start_index:end_index]
    noise_power = np.sum(x * x)
    
    threshold = impulse_power / noise_power
    
    return threshold
    
    
def _estimate_threshold_2(detector_info):
    
    s = Bunch(
        sample_rate=_SAMPLE_RATE,
        duration=10,
        background_noise_amplitude=100,
        impulse_time=5
    )

    samples = _create_background_noise(s)
    amplitude = _find_just_detectable_impulse_amplitude_aux(
        detector_info, samples, s)
    
    impulse_index = _round(s.impulse_time * s.sample_rate)
    samples[impulse_index] = amplitude
    
    low = 1.01
    high = 5
    
    # Loop invariant: threshold is in (low, high].
    
    while high - low > .01:
        
        print('threshold is in ({}, {}]...'.format(low, high))
        
        mid = (low + high) / 2
        
        detector_info.threshold = mid
        
        clips = new_detector.detect(samples, detector_info)
        
        if len(clips) == 0:
            high = mid
            
        else:
            low = mid
            
    print('threshold estimate:', high)

    
def _compare_detectors_on_impulses(detector_versions, detector_info):
    
    num_impulses = 100
    indices = np.arange(num_impulses)
    lead_time = 5
    period = 2.5
    
    s = Bunch(
        sample_rate=_SAMPLE_RATE,
        duration=lead_time + period * num_impulses,
        background_noise_amplitude=100,
        impulse_times=lead_time + period * indices,
        impulse_amplitudes=10000 * (indices + 1.) / num_impulses
    )
    
    samples = _create_background_noise(s)
    _add_impulses(samples, s)
    
    _compare_detectors(detector_versions, detector_info, samples)
    
    
def _compare_detectors(detector_versions, detector_info, samples):
    
    name_a, name_b = detector_versions
    detector_a = _DETECTOR_MODULES[name_a]
    detector_b = _DETECTOR_MODULES[name_b]
    
    print()
    
    print('running detector version "{}"...'.format(name_a))
    clips_a = detector_a.detect(samples, detector_info)
    
    print('running detector version "{}"...'.format(name_b))
    clips_b = detector_b.detect(samples, detector_info)
    
    print('matching clips...')
    clip_matches = clip_utils.match_clips(clips_a, clips_b)
    
    for i, match in enumerate(clip_matches):
        stats = _compute_match_statistics(match)
        print(i, match, stats)


def _compute_match_statistics(match):
    old, new = match
    if old is None:
        return None
    else:
        return [_compute_match_statistics_aux(old, n) for n in new]
        
    
def _compute_match_statistics_aux(old, new):
    old_start, old_length = old
    old_end = old_start + old_length
    new_start, new_length = new
    new_end = new_start + new_length
    start_diff = new_start - old_start
    end_diff = new_end - old_end
    length_diff = new_length - old_length
    return (start_diff, end_diff, length_diff)


def _compare_detectors_on_noise_bursts(detector_versions, detector_info):
    
    num_amplitudes = 10
    amplitudes = 1000 + 2000 * (np.arange(num_amplitudes) + 1.) / num_amplitudes
    
    num_durations = 100
    durations = 1. * (np.arange(num_durations) + 1.) / num_durations
    
    lead_time = 5
    period = 2.5
    
    for amplitude in amplitudes:
        
        print()
        print('amplitude', amplitude)
        
        s = Bunch(
            sample_rate=_SAMPLE_RATE,
            duration=lead_time + period * num_durations,
            background_noise_amplitude=1000,
            noise_burst_times=lead_time + period * np.arange(num_durations),
            noise_burst_amplitudes=amplitude,
            noise_burst_durations=durations
        )
    
        samples = _create_background_noise(s)
        _add_noise_bursts(samples, s)
        
        _compare_detectors(detector_versions, detector_info, samples)


def _ramp(n):
    return (np.arange(n) + 1.) / n


def _compare_detectors_on_tones(detector_versions, detector_info):
    
    num_amplitudes = 10
    amplitudes = 3000 * _ramp(num_amplitudes)
    
    durations = [.01, .02, .05, .1, .2, .5, 1]
    num_durations = len(durations)
    
    num_frequencies = 10
    f0 = detector_info.f0 - 500
    f1 = detector_info.f1 + 500
    frequencies = f0 + (f1 - f0) * _ramp(num_frequencies)
    
    lead_time = 5
    period = 2.5
    
    for amplitude in amplitudes:
            
        for duration in durations:
        
            print()
            print('amplitude', amplitude, 'duration', duration)
            
            s = Bunch(
                sample_rate=_SAMPLE_RATE,
                duration=lead_time + period * num_durations,
                background_noise_amplitude=1000,
                tone_start_times=lead_time + period * np.arange(num_durations),
                tone_amplitudes=amplitude,
                tone_durations=duration,
                tone_frequencies=frequencies,
                tone_onset_duration=.005
            )
        
            samples = _create_background_noise(s)
            _add_tones(samples, s)
            
            _compare_detectors(detector_versions, detector_info, samples)


def _compare_detectors_on_chirps(detector_versions, detector_info):
    
    num_amplitudes = 10
    amplitudes = 3000 * _ramp(num_amplitudes)
    
    durations = [.01, .02, .05, .1, .2, .5, 1]
    
    num_chirps = 100
    f0 = detector_info.f0 - 500
    f1 = detector_info.f1 + 500
    df = 1000
    
    lead_time = 5
    period = 2.5
    
    for amplitude in amplitudes:
        
        for duration in durations:
            
            print()
            print('amplitude', amplitude, 'duration', duration)
            
            random = np.random.random
            start_freqs = f0 + (f1 - f0) * random(num_chirps)
            end_freqs = start_freqs + df * 2 * (random(num_chirps) - .5)
            
            s = Bunch(
                sample_rate=_SAMPLE_RATE,
                duration=lead_time + period * num_chirps,
                background_noise_amplitude=1000,
                chirp_start_times=lead_time + period * np.arange(num_chirps),
                chirp_amplitudes=amplitude,
                chirp_durations=duration,
                chirp_start_frequencies=start_freqs,
                chirp_end_frequencies=end_freqs,
                tone_onset_duration=.005
            )
        
            samples = _create_background_noise(s)
            _add_chirps(samples, s)
            
            _compare_detectors(detector_versions, detector_info, samples)


def _create_background_noise(s):
    length = _round(s.duration * s.sample_rate)
    return _create_noise(length, s.background_noise_amplitude)


def _create_noise(length, amplitude):
    a = 2 * amplitude
    b = -amplitude
    return a * np.random.random(length) + b


def _round(x):
    return int(round(x))


def _add_impulses(samples, s):
    
    times = _to_array(s.impulse_times)
    amplitudes = _to_array(s.impulse_amplitudes, len(times))
    
    for i, time in enumerate(s.impulse_times):
        index = _round(time * s.sample_rate)
        samples[index] = amplitudes[i]


def _to_array(x, length=None):
    if isinstance(x, (int, float)):
        return x * np.ones(length)
    else:
        return np.array(x)
    

def _add_noise_bursts(samples, s):
     
    times = _to_array(s.noise_burst_times)
    amplitudes = _to_array(s.noise_burst_amplitudes, len(times))
    durations = _to_array(s.noise_burst_durations, len(times))
     
    for i, time in enumerate(times):
        start_index = _round(time * s.sample_rate)
        length = _round(durations[i] * s.sample_rate)
        end_index = start_index + length
        samples[start_index:end_index] = _create_noise(length, amplitudes[i])
            
    
def _add_tones(samples, settings):
    
    s = settings
    
    start_times = _to_array(s.tone_start_times)
    n = len(start_times)
    amplitudes = _to_array(s.tone_amplitudes, n)
    durations = _to_array(s.tone_durations, n)
    frequencies = _to_array(s.tone_frequencies, n)
    onset_length = _round(s.tone_onset_duration * s.sample_rate)
    
    for i, start_time in enumerate(start_times):
        
        start_index = _round(start_time * s.sample_rate)
        length = _round(durations[i] * s.sample_rate)
    
        factor = 2 * np.pi * frequencies[i] / s.sample_rate
        phases = factor * np.arange(length)
        tone = amplitudes[i] * np.sin(phases)
        
        ramp = _ramp(onset_length)
        tone[:onset_length] *= ramp
        tone[-onset_length:] *= 1 - ramp
        
        samples[start_index:start_index + length] += tone
    
    return samples
    
    
def _add_chirps(samples, settings):
    
    s = settings
    
    start_times = _to_array(s.chirp_start_times)
    n = len(start_times)
    amplitudes = _to_array(s.chirp_amplitudes, n)
    durations = _to_array(s.chirp_durations, n)
    start_freqs = _to_array(s.chirp_start_frequencies, n)
    end_freqs = _to_array(s.chirp_end_frequencies, n)
    freq_deltas = end_freqs - start_freqs
    onset_length = _round(s.tone_onset_duration * s.sample_rate)
    
    for i, start_time in enumerate(start_times):
        
        start_index = _round(start_time * s.sample_rate)
        length = _round(durations[i] * s.sample_rate)
    
        # print('_add_chirps', i, start_time, len(samples), start_index, length)
        
        a = 2 * np.pi * start_freqs[i]
        b = np.pi * freq_deltas[i]
        t = np.arange(length) / s.sample_rate
        phases = (a + b * t) * t
        chirp = amplitudes[i] * np.sin(phases)
        
        ramp = _ramp(onset_length)
        chirp[:onset_length] *= ramp
        chirp[-onset_length:] *= 1 - ramp
        
        samples[start_index:start_index + length] += chirp
    
    return samples
    
    
if __name__ == '__main__':
    main()


'''
I've noticed that the clip samples saved by the Old Bird detectors can
differ from the samples that were input to the detector. The differences
always appear to have magnitude one. This might be because the normalization
that MATLAB performs when it reads the input file (it maps sample values
to the range [-1, 1]) is not quite the inverse of the denormalization
that Steve Mitchell's code (in particular, the `quantize` function of
the BufferedDSP file sclipnsave.c) performs. That function is:

short quantize (real_T x)
{
    return (short) floor (0.5 + 32767.0 * x);
}

MATLAB probably just divides the samples by 32768 to normalize them.
It might also map [-32768, 32767] linearly to [-1, 1], but that would
add a slight offset.

When I add scaling of input samples like MATLAB's (either kind) to the
new detector, it appears to make no difference.
'''

'''
Inferring parameter values from clips extracted from test signals.

1. The minimum duration plus padding can be found by running the detector
on an impulse train (with a spacing of, say, two seconds between consecutive
impulses) in low-level background noise and looking at the lengths of the
resulting clips. The maximum duration plus padding can be found by running
the detector on a long noise burst in low-level background noise and looking
at the length of the resulting clip.

The Tseep .mdl file I looked at carefully (which appears not to have the
final Tseep detector parameter values, so it is unfortunately not
authoritative) specified minimum and maximum durations in seconds and
padding (which is called "lag" in the file) in samples. Running the
above tests on the Old Bird Tseep-x detector yields:

    minimum duration + padding = 5205 samples
    maximum duration + padding = 11820 samples
    
These results strongly suggest:

    min_duration = .100 seconds
    max_duration = .400 seconds
    padding = 3000
    
Other combinations of durations and padding that yield the same sums
do not involve such nice round numbers.

2. To estimate the integration time, the delay, and the threshold, we
first run the detector on a series of test signals, each of which
comprises an impulse in constant amplitude noise. The noise amplitude
is the same in all of the test signals, but we vary the impulse amplitude
to determine the amplitude that just causes the impulse to trip the
detector.

Then, to estimate the integration time we run the detector on a second
series of test signals, each of which comprises an impulse train in
constant-amplitude noise. The noise amplitude is the same as in the
last step. The amplitude of the first impulse is a little larger than
the minimum amplitude needed to trip the detector, and the amplitude
of the other impulses is a little smaller than that amplitude. The
impulses are evenly spaced. The detector produces a single clip in
response to the initial impulse. If the spacing is smaller than the
integration time the clip has the maximum duration, while if the
spacing is larger than the integration time the clip has the minimum
duration.

To estimate the delay, we run the detector on a series of test signals,
each of which comprises two impulses in constant-amplitude noise. The
noise amplitude is the same as in the previous tests. The impulses
have the same amplitude, slightly less than the amplitude determined
in the first step. If the spacing between the impulses is less than
the delay the detector will produce a clip, but if the spacing is
less than the delay it will not.

Once we know the integration time and the delay, we can estimate the
threshold from the samples of the test signal of the first step that
just tripped the detector. Averaging estimates computed with different
noise reduces the variance of the estimate.

3. To estimate the filter passband edges, first establish the amplitude
of a short (200 ms long, say) inband tone (we assume some prior knowledge
of the passband, just not exact knowledge of its edge frequencies) in
background noise of amplitude 1000 that just suffices to trip the detector.
Then generate a series of tones with amplitude twice that minimum amplitude,
varying their frequencies. Tones whose frequencies are within the passband
should be detected, and tones whose frequencies are outside of it should not.
We can also get a good idea of the transition band width from such
experiments.

At this point only the filter length and the start offset should be unknown.
Measuring these parameters requires comparing detections output by the old
and new detectors. For a frequency within the passband, generate a series
of tones in background noise of amplitude 1000 whose amplitudes start at a
little over the detection threshold and increase by increments of 100 to
about twice the starting point. For a range of filter lengths, compare the
lengths of the clips output by the old and new detectors. The lengths should
all be equal for the correct filter length but not for other filter lengths.
The differences between the start indices of corresponding clips output by
the old and new detectors should also all be equal when the filter length
is correct, and the difference indicates the correct start offset.
'''
