"""Unit tests for the `clip_utils` module."""


import unittest

import clip_utils


class AmplitudeAxisTests(unittest.TestCase):


    def test_match_clips(self):
        
        c01 = (0, 1)
        c02 = (0, 2)
        c03 = (0, 3)
        c11 = (1, 1)
        c12 = (1, 2)
        c13 = (1, 3)
        
        n = lambda c: (None, (c,))
        n01 = n(c01)
        # n02 = n(c02)
        n03 = n(c03)
        n11 = n(c11)
        # n12 = n(c12)
        n13 = n(c13)
        
        b0 = []
        b1 = [c01]
        b2 = [c01, c11]
        b3 = [c02, c12]
        b4 = [c03, c13]
        
        cases = [
            
            ([], b0, []),
            ([], b1, [n01]),
            ([], b2, [n01, n11]),
            
            ([(-1, 1)], b1, [((-1, 1), ()), n01]),
            ([(1, 1)], b1, [n01, ((1, 1), ())]),
            ([(-1, 2)], b1, [((-1, 2), (c01,))]),
            
            ([(-1, 2), (1, 2)], b2, [
                ((-1, 2), (c01,)),
                ((1, 2), (c11,))]),
                 
            ([(1, 1)], b3, [((1, 1), (c02, c12))]),
            ([(0, 2)], b4, [((0, 2), (c03, c13))]),
            ([(2, 1)], b4, [((2, 1), (c03, c13))]),
            ([(2, 2)], b4, [((2, 2), (c03, c13))]),
            
            ([(-2, 1), (-1, 2), (4, 2)], b4, [
                ((-2, 1), ()),
                ((-1, 2), (c03,)),
                n13,
                ((4, 2), ())]),
                 
            ([(-1, 1), (4, 1)], b4, [
                ((-1, 1), ()),
                n03,
                n13,
                ((4, 1), ())])

        ]
        
        for a, b, expected in cases:
            actual = clip_utils.match_clips(a, b)
            self.assertEqual(actual, expected)
