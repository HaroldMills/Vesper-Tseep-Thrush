import wave

import numpy as np


_WAVE_SAMPLE_DTYPE = np.dtype('<i2')


def write_sound_file(file_path, samples, sample_rate):
    
    samples = np.array(samples, dtype=_WAVE_SAMPLE_DTYPE)
    
    with wave.open(file_path, 'wb') as writer:
        
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


def read_sound_file(file_path):
    
    with wave.open(file_path, 'rb') as reader:
        
        length = reader.getparams().nframes
        string = reader.readframes(length)
        samples = np.frombuffer(string, dtype=_WAVE_SAMPLE_DTYPE)
        return samples
