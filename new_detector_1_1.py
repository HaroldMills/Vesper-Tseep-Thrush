"""Wraps redux detector version 1.1 for use by `test_detector` script."""


from old_bird_detector_redux_1_1 import ThrushDetector, TseepDetector


def detect(samples, settings):
    
    """Runs the Old Bird detector reimplementation on the specified samples."""
    
    listener = _Listener()
    
    if settings.detector_name == 'Thrush':
        cls = ThrushDetector
    else:
        cls = TseepDetector
        
    detector = cls(settings.sample_rate, listener)
    detector.detect(samples)
    detector.complete_detection()
    
    return listener.clips


class _Listener:
    
    def __init__(self):
        self.clips = []
        
    def process_clip(self, start_index, length):
        self.clips.append((start_index, length))
