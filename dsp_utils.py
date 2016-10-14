"""Utility functions pertaining to DSP."""


import numpy as np
import scipy.linalg as linalg
import scipy.signal as signal


def firls(numtaps, bands, desired):
    
    """
    Designs an FIR filter that is optimum in a least squares sense.
    
    This function is like `scipy.signal.firls` except that `numtaps`
    can be even as well as odd and band weighting is not supported.
    """
    
    # TODO: Add support for band weighting and then submit a pull
    # request to improve `scipy.signal.firls`.
    
    numtaps = int(numtaps)
    if numtaps % 2 == 1:
        return signal.firls(numtaps, bands, desired)
    else:
        return _firls_even(numtaps, bands, desired)
    
    
def _firls_even(numtaps, bands, desired):
    
    # This function implements an algorithm similar to the one of the
    # SciPy `firls` function, but for even-length filters rather than
    # odd-length ones. See paper notes entitled "Least squares FIR
    # filter design for even N" for derivation. The derivation is
    # similar to that of Ivan Selesnick's "Linear-Phase FIR Filter
    # Design By Least Squares" (available online at
    # http://cnx.org/contents/eb1ecb35-03a9-4610-ba87-41cd771c95f2@7),
    # with due alteration of detail for even filter lengths.
    
    bands.shape = (-1, 2)
    desired.shape = (-1, 2)
    weights = np.ones(len(desired))
    M = int(numtaps / 2)
    
    # Compute M x M matrix Q (actually twice Q).
    n = np.arange(numtaps)[:, np.newaxis, np.newaxis]
    q = np.dot(np.diff(np.sinc(bands * n) * bands, axis=2)[:, :, 0], weights)
    Q1 = linalg.toeplitz(q[:M])
    Q2 = linalg.hankel(q[1:M + 1], q[M:])
    Q = Q1 + Q2
    
    # Compute M-vector b.
    k = np.arange(M) + .5
    e = bands[1]
    b = np.diff(e * np.sinc(e * k[:, np.newaxis])).reshape(-1)
    
    # Compute a (actually half a).
    a = np.dot(linalg.pinv(Q), b)
    
    # Compute h.
    h = np.concatenate((np.flipud(a), a))
    
    return h
