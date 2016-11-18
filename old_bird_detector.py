'''
Basic plan for signal processing infrastructure:

Simple, fast, easy to create new processors.

Features:
* Acyclic directed processor graphs.
* Multirate processing.
* Regularly sampled signals as well as irregularly spaced event sequences.
* Real-time as well as non-real-time processing with the same processors.
* Declarative description of processor graphs.
* Support for multiprocessing.
* Description of processor graph orthogonal to mapping onto computational units.
* Units of processor I/O are signal fragments and events. Units carry temporal
info like signal indices, signal start datetime, event start index and length,
etc. Processors propagate this info from inputs to outputs.
* Processor progress monitoring.

Initial version limited but sufficient for the near-term.

Read about Ptolemy, other simulation projects for ideas.
Sriram and Bhattacharyya (2009)
'''


'''
Implementation notes:
* Input signal fragments are read-only, so fragments can be shared by
  multiple processors.
* Processor input wraps input signal fragments as a single *available
  fragment* to simplify processor implementation.
* Processor releases initial portions of available fragment, triggering
  release of wrapped input fragments.
  
Classes:
* SignalInfo: signal metadata but no samples.
* Signal: signal metadata and samples.
* SignalFragment: a portion of a signal, including contiguous samples.
* SignalFragmentConcatenation: a SignalFragment
* Processor
* ProcessorInput
* ProcessorOutput
'''


class Named:
    pass


class Processor(Named):


    @staticmethod
    def connect(output, input):
        output.connect(input)
        
        
    def __init__(self, name, parent):
        super().__init__(name)
        self._parent = parent
        self._children = set()
        
        
    @property
    def parent(self):
        return self._parent
    
    
    @property
    def children(self):
        return self._children
    
    
    @property
    def input(self):
        return self._get_singleton_port(self.inputs, 'input')
        
        
    def _get_singleton_port(self, ports, name):
        if len(ports) == 0:
            raise ValueError('Processor has no {}s.'.format(name))
        elif len(ports) > 1:
            raise ValueError('Processor has more than one {}.'.format(name))
        return ports[0]
    
    
    @property
    def output(self):
        return self._get_singleton_port(self.outputs, 'output')        
    
    
class ProcessorInput(Named):
    
    
    def __init__(self, name, processor):
        super().__init__(name)
        self._processor = processor
        
        
    @property
    def processor(self):
        return self._processor
    
    
    def connect(self, output):
        output.connect(self)
        
        
    def receive(self, message):
        
        # `message` is typically a signal fragment, but it can be something
        # else, such as a detection, depending on the particular type of
        # processor.
        
        raise NotImplementedError()


class ProcessorOutput(Named):
    
    
    def __init__(self, name, processor):
        super().__init__(name)
        self._processor = processor
        self._connected_inputs = set()
        self._listeners = set()
        
        
    @property
    def processor(self):
        return self._processor
    
    
    @property
    def connected_inputs(self):
        return set(self._connected_inputs)
    
    
    def connect(self, input):
        self._connected_inputs.add(input)
        self._listeners.add(input.receive)
        
        
    def add_listener(self, listener):
        self._listeners.add(listener)
        
        
    def remove_listener(self, listener):
        self._listeners.remove(listener)
        
        
    def send(self, message):
        for listener in self._listeners:
            listener(message)


class FirFilter(Processor):
    pass


class Operator(Processor):
    
    
    def __init__(self, name, parent, operator):
        super().__init__(name, parent)
        self._operator = operator
        
        
    @property
    def operator(self):
        return self._operator


class FiniteIntegrator(Processor):
    pass


class DelayedOperator(Processor):
    pass


class ThresholdCrossingFinder(Processor):
    pass


class ThresholdCrossingParser(Processor):
    pass


class OldBirdDetector(Processor):
    
    
    def __init__(self, name, parent, settings):
        
        super().__init__(name, parent)
        
        self._children = self._create_children(settings)
        
        # RESUME: Need input and output pads. An input pad looks like
        # an input at the top level of a processor, but like a processor
        # with an output at the next level down. Similarly, an output
        # pad looks like an output at top level, but like a processor
        # with an input at the next level down. Does this work if there
        # are multiple levels of nesting?
        self._inputs = self.processors['Filter'].inputs
        self._outputs = self.processors['Threshold Crossing Parser'].outputs


    def _create_children(self, settings):
        
        s = settings
        
        filter = FirFilter('Filter', self, s.filter_coefficients)
        squarer = Operator('Squarer', self, lambda x: x * x)
        integrator = FiniteIntegrator('Integrator', self, s.integration_time)
        
        ratioer = DelayedOperator(
            'Ratioer', self, self.ratio_delay, lambda x0, x1: x1 / x0)
        
        crossing_finder = ThresholdCrossingFinder(
            'Threshold Crossing Finder', self, s.detection_threshold)
        
        parser = ThresholdCrossingParser(
            'Threshold Crossing Parser', self, s.min_duration, s.max_duration)
            
        processors = (
            filter, squarer, integrator, ratioer, crossing_finder, parser)
        
        chain_processors(processors)
        
        return dict((p.name, p) for p in processors)
    
    
def chain_processors(processors):
    connect = Processor.connect
    for i in range(len(processors) - 1):
        output = processors[i].output
        input = processors[i + 1].input
        connect(output, input)
