"""Utility functions pertaining to clips."""


import numpy as np


def match_clips(a, b):
    
    """
    Matches clips from two clip sequences.
    
    Each of the arguments `a` and `b` of this function is a sequence
    of pairs of nonnegative integers. Each pair comprises the start
    index and length in samples of a clip. This function assumes that
    the start and end indices of the clips (where the end index of
    a clip is the start index plus the length) of each sequence
    strictly increase, and that the clips are sorted by their start
    indices.
    
    The function returns a sequence of pairs. Each pair is of one
    of the forms `(<a clip>, <tuple of b clips>)` or `(None, <b clip>)`,
    where `<a clip>` is a clip of `a` and `<b clip>` is a clip of `b`.
    Each clip of `a` is paired with the tuple of clips of `b` that
    overlap it by one or more samples. Each clip of `b` that does
    not overlap any clip of `b` appears in a `(None, <b clip>)`
    tuple. The pairs are ordered by the start index of the `a` clip
    if present, or the start index of the `b` clip otherwise.
    """
    
    # This function assumes that both the start times and the end times
    # of clip arrays `a` and `b` strictly increase, and that the arrays
    # are sorted in the order of these times.
    
    a_starts = np.array([clip[0] for clip in a])
    a_ends = np.array([clip[0] + clip[1] for clip in a])

    b_starts = np.array([clip[0] for clip in b])
    b_ends = np.array([clip[0] + clip[1] for clip in b])
    
    # Find `starts` such that clips `b[:starts[i]]` precede clip `a[i]`.
    starts = np.searchsorted(b_ends, a_starts, 'right')
    
    # Find `ends` such that clips `b[ends[i]:]` follow clip `a[i]`.
    ends = np.searchsorted(b_starts, a_ends, 'left')
    
    # At this point clips b[starts[i]:ends[i]] overlap clip a[i].

    matches = []
    j = 0
    
    for i in range(len(a)):
        
        # Include (None, b[j]) for each b[j] that overlaps no clip of a.
        while j < starts[i]:
            matches.append((None, (b[j],)))
            j += 1
            
        matches.append((a[i], tuple(b[starts[i]:ends[i]])))
        j = ends[i]
        
    # Include (None, b[j]) for trailing b clips that overlap no clip of a.
    while j < len(b):
        matches.append((None, (b[j],)))
        j += 1
        
    return matches
