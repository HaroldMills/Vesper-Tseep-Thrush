"""
A detector operates on a single audio channel. It has a `detect` method
that takes a NumPy array of samples. The method can be called repeatedly
with consecutive sample arrays. The `complete_detection` method should be
called after the final call to the `detect` method. During detection,
the detector notifies its listener each time it detects a clip. 
"""


import math

import numpy as np
import scipy.signal as signal

import dsp_utils


_CHUNK_SIZE = 1


def detect(samples, settings):
    
    """Runs the Old Bird transient detector on the specified samples."""
    
    sample_rate = settings.sample_rate
    
    clips = []
    padding = settings.initial_padding
    listener = lambda i, n: clips.append((i - padding + 1, n + padding))
    detector = OldBirdDetectorRedux(settings, sample_rate, listener)
    
    chunk_size = int(round(_CHUNK_SIZE * sample_rate))
      
    i = 0
    while i < len(samples):
        s = samples[i:i + chunk_size]
        detector.detect(s)
        i += chunk_size
        
    # detector.detect(samples)
    detector.complete_detection()
    
    return clips


class OldBirdDetectorRedux:
    
    """Reimplementation of Old Bird transient detector."""
    
    
    def __init__(self, settings, sample_rate, listener):
        
        self._settings = settings
        self._sample_rate = sample_rate
        
        self._signal_processor = self._create_signal_processor()
        self._transient_finder = _TransientFinder(settings, listener)
        
        self._num_samples_processed = 0
        self._recent_samples = np.array([], dtype='float')
        self._initial_samples_repeated = False
        
    
    def _create_signal_processor(self):
        
        coefficients = self._design_filter()
        
        s = self.settings
        
        # We use `math.floor` here rather than `round` since the Simulink
        # .mdl files we have access to suggest that the original Old Bird
        # detector used MATLAB's `fix`  function, which rounds towards zero.
        delay = math.floor(s.delay * self.sample_rate)
        
        processors = [
            _FirFilter(coefficients),
            _Squarer(),
            _Integrator(s.integration_time),
            _DelayAndDivideProcessor(delay),
            _ThresholdCrossingMarker(s.threshold)
        ]
        
        return _SignalProcessorChain(processors)
        
        
    def _design_filter(self):
        s = self._settings
        f0 = s.f0
        f1 = s.f1
        bw = s.bw
        fs2 = self.sample_rate / 2
        bands = np.array([0, f0 - bw, f0, f1, f1 + bw, fs2]) / fs2
        desired = np.array([0, 0, 1, 1, 0, 0])
        return dsp_utils.firls(s.filter_length, bands, desired)
    
    
    @property
    def settings(self):
        return self._settings
    
    
    @property
    def sample_rate(self):
        return self._sample_rate
    
    
    @property
    def listener(self):
        return self._transient_finder.listener
    
    
    def detect(self, samples):
        
        augmented_samples = np.concatenate((self._recent_samples, samples))
        
        if len(augmented_samples) <= self._signal_processor.latency:
            # don't yet have enough samples to fill processing pipeline
            
            self._recent_samples = augmented_samples
            
        else:
            # have enough samples to fill processing pipeline
            
            # Run signal processors on samples.
            crossing_samples = self._signal_processor.execute(augmented_samples)
            
            # Get transient index offset.
            offset = self._num_samples_processed
            if not self._initial_samples_repeated:
                offset += self._signal_processor.latency
                self._initial_samples_repeated = True
                
            crossings = self._get_crossings(crossing_samples, offset)
            
            self._transient_finder.find_transients(crossings)
            
            # Save trailing samples for next call to this method.
            self._recent_samples = \
                augmented_samples[-self._signal_processor.latency:]
            
        self._num_samples_processed += len(samples)
            
            
    def _get_crossings(self, crossing_samples, offset):
    
        # Find indices of outward-going threshold crossing events.
        rise_indices = np.where(crossing_samples == 1)[0] + offset
        fall_indices = np.where(crossing_samples == -2)[0] + offset
        
        return sorted(
            [(i, True) for i in rise_indices] +
            [(i, False) for i in fall_indices])        
    
    
    def complete_detection(self):
        
        """
        Completes detection after the `detect` method has been called
        for all input.
        """
        
        # Send a final falling crossing to the transient finder to
        # terminate a transient that may have started more than the
        # minimum clip duration before the end of the input but for
        # which for whatever reason there has not yet been a fall.
        fall = (self._num_samples_processed, False)
        self._transient_finder.find_transients([fall])


class _SignalProcessor:
    
    
    def __init__(self, latency):
        self._latency = latency
        
        
    @property
    def latency(self):
        return self._latency
    
    
    def execute(self, x):
        raise NotImplementedError()
    
    
class _FirFilter(_SignalProcessor):
    
    
    def __init__(self, coefficients):
        super().__init__(len(coefficients) - 1)
        self._coefficients = coefficients
        
        
    def execute(self, x):
        return signal.fftconvolve(x, self._coefficients, mode='valid')
    
    
class _Squarer(_SignalProcessor):
    
    
    def __init__(self):
        super().__init__(0)
    
    
    def execute(self, x):
        return x * x;
    
    
class _Integrator(_FirFilter):
    
    # An alternative to making this class an `_FirFilter` subclass would
    # be to use the `np.cumsum` function to compute the cumulative sum
    # of the input and then the difference between the result and a
    # delayed version of the result. That approach is more efficient
    # but it has numerical problems for sufficiently long inputs
    # (the cumulative sum of the squared samples grows ever larger, but
    # the samples do not, so you'll eventually start throwing away sample
    # bits), so I have chosen not to use it. An alternative would be to use
    # Cython or Numba or something like that to implement the integration
    # in a way that is both faster and accurate for arbitrarily long inputs.
    
    def __init__(self, integration_size):
        coefficients = np.ones(integration_size) / integration_size
        super().__init__(coefficients)


class _DelayAndDivideProcessor(_SignalProcessor):
    
    
    def __init__(self, delay):
        super().__init__(delay - 1)
        self._delay = delay
        
        
    def execute(self, x):
        return x[self._delay:] / x[:-self._delay]
             
    
class _ThresholdCrossingMarker(_SignalProcessor):
    
    
    def __init__(self, threshold):
        super().__init__(1)
        self._threshold = threshold
        
        
    def execute(self, x):
        
        # Compare input to threshold and its inverse.
        y = np.zeros(len(x))
        y[x > self._threshold] = 1
        y[x < 1. / self._threshold] = -2
        
        # Take differences to mark where the input crosses thr threshold
        # and its inverse. The four types of crossing will be marked as
        # follows:
        #
        #     1 - upward crossing of threshold
        #    -1 - downward crossing of threshold
        #     2 - upward crossing of threshold inverse
        #    -2 - downward crossing of threshold inverse
        return np.diff(y)
        

class _SignalProcessorChain(_SignalProcessor):
    
    
    def __init__(self, processors):
        latency = sum([p.latency for p in processors])
        super().__init__(latency)
        self._processors = processors
        
        
    def execute(self, x):
        for processor in self._processors:
            x = processor.execute(x)
        return x
    
    
_STATE_DOWN = 0
_STATE_UP = 1
_STATE_HOLDING = 2


class _TransientFinder:
    
    """
    Finds transients from a sequence of detection threshold crossings,
    notifying a listener for each transient.
    """
    
    
    def __init__(self, settings, listener):
        
        self._settings = settings
        self._listener = listener
        
        s = settings
        
        # We use `math.floor` here rather than `round` since the Simulink
        # .mdl files we have access to suggest that the Simulink detector
        # used MATLAB's `fix`  function, which rounds toward zero.
        self._min_length = int(math.floor(s.min_duration * s.sample_rate))
        self._max_length = int(math.floor(s.max_duration * s.sample_rate))
    
        self._state = _STATE_DOWN
        
        self._start_index = 0
        """
        index of start of current transient.
        
        The value of this attribute only has meaning for the up and holding
        states. It does not mean anything for the down state.
        """
        
        
    @property
    def settings(self):
        return self._settings
    
    
    @property
    def listener(self):
        return self._listener
    
    
    def find_transients(self, crossings):
        
        for index, rise in crossings:
    
            if self._state == _STATE_DOWN:
    
                if rise:
                    # rise while down
    
                    # Start new transient.
                    self._start_index = index
                    self._state = _STATE_UP
    
                # Do nothing for fall while down.
    
            elif self._state == _STATE_UP:
    
                if rise:
                    # rise while up
    
                    if index == self._start_index + self._max_length:
                        # rise just past end of maximal transient
                        
                        # Emit maximal transient
                        self._listener(self._start_index, self._max_length)
                        
                        # Return to down state. It seems a little odd that
                        # a rise would return us to the down state, but
                        # that is what happens in the original Old Bird
                        # detector (see line 252 of the original detector
                        # source code file splimflipflop.c), and our goal
                        # here is to reimplement that detector. This code
                        # should seldom execute on real inputs, since it
                        # should be rare for two consecutive rises to occur
                        # precisely `self._max_length` samples apart.
                        self._state = _STATE_DOWN
                        
                    elif index > self._start_index + self._max_length:
                        # rise more than one sample past end of maximal
                        # transient
    
                        # Emit maximal transient
                        self._listener(self._start_index, self._max_length)
    
                        # Start new transient.
                        self._start_index = index
    
                    # Do nothing for rise before end of maximal transient.
    
                else:
                    # fall while up
    
                    if index < self._start_index + self._min_length:
                        # fall before end of minimal transient
    
                        self._state = _STATE_HOLDING
    
                    else:
                        # fall at or after end of minimal transient
    
                        length = index - self._start_index
    
                        # Truncate transient if after end of maximal transient.
                        if length > self._max_length:
                            length = self._max_length
    
                        # Emit transient.
                        self._listener(self._start_index, length)
                        
                        self._state = _STATE_DOWN
    
            else:
                # holding after short transient
    
                if rise:
                    # rise while holding after short transient
    
                    if index > self._start_index + self._min_length:
                        # rise follows end of minimal transient by at least
                        # one non-transient sample
                        
                        # Emit minimal transient.
                        self._listener(self._start_index, self._min_length)
    
                        # Start new transient.
                        self._start_index = index
                        
                    self._state = _STATE_UP
    
                else:
                    # fall while holding after short transient
    
                    if index >= self._start_index + self._min_length:
                        # fall at or after end of minimal transient
    
                        # Emit minimal transient.
                        self._listener(self._start_index, self._min_length)
    
                        self._state = _STATE_DOWN
    
                    # Do nothing for fall before end of minimal transient.
