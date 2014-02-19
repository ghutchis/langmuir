# -*- coding: utf-8 -*-
"""
@author: adam
"""
import langmuir as lm
import collections

class CheckPoint(object):
    """
    A class to open lm checkpoint files.

    ========================= =======================================
    **Attribute**             **Description**
    ========================= =======================================
    :py:attr:`electrons`      :py:obj:`list` of :py:obj:`int`
    :py:attr:`holes`          :py:obj:`list` of :py:obj:`int`
    :py:attr:`traps`          :py:obj:`list` of :py:obj:`int`
    :py:attr:`defects`        :py:obj:`list` of :py:obj:`int`
    :py:attr:`trapPotentials` :py:obj:`list` of :py:obj:`float`
    :py:attr:`fluxState`      :py:obj:`list` of :py:obj:`int`
    :py:attr:`randomState`    :py:obj:`list` of :py:obj:`int`
    :py:attr:`parameters`     :class:`Parameters`
    ========================= =======================================

    :param handle: filename or file object; Can be :py:obj:`None`.
    :type handle: str
    """

    def __init__(self, handle=None):
        self._electrons      = []
        self._holes          = []
        self._traps          = []
        self._defects        = []
        self._potentials     = []
        self._flux_state     = []
        self._random_state   = []
        self._parameters     = lm.parameters.Parameters()
        if handle:
            self.load(handle)

    @property
    def electrons(self):
        return self._electrons
    @electrons.setter
    def electrons(self, value):
        self._electrons = list(value)

    @property
    def holes(self):
        return self._holes
    @holes.setter
    def holes(self, value):
        self._holes = list(value)

    @property
    def traps(self):
        return self._traps
    @traps.setter
    def traps(self, value):
        self._traps = list(value)

    @property
    def defects(self):
        return self._defects
    @defects.setter
    def defects(self, value):
        self._defects = list(value)

    @property
    def potentials(self):
        return self._potentials
    @potentials.setter
    def potentials(self, value):
        self._potentials = list(value)

    @property
    def flux_state(self):
        return self._flux_state
    @flux_state.setter
    def flux_state(self, value):
        self._flux_state = list(value)

    @property
    def random_state(self):
        return self._random_state
    @random_state.setter
    def random_state(self, value):
        self._random_state = list(value)

    @property
    def parameters(self):
        return self._parameters

    def load(self, handle):
        """
        Load checkpoint from a file.

        :param handle: filename or file object
        :type handle: str

        >>> chk = lm.checkpoint.CheckPoint()
        >>> chk.load('out.chk')
        """
        self.clear()
        handle = lm.common.zhandle(handle, 'rb')
        line = handle.readline()
        while line:
            line = line.strip()
            if line == '[Electrons]':
                self._electrons = self._load_values(handle, int)
            elif line == '[Holes]':
                self._holes = self._load_values(handle, int)
            elif line == '[Defects]':
                self._defects = self._load_values(handle, int)
            elif line == '[Traps]':
                self._traps = self._load_values(handle, int)
            elif line == '[TrapPotentials]':
                self._potentials = self._load_values(handle, float)
            elif line == '[FluxState]':
                self._flux_state = self._load_values(handle, int)
            elif line == '[RandomState]':
                self._random_state = [int(i.strip()) for i in
                    handle.readline().strip().split()]
            elif line == '[Parameters]':
                self._parameters.load(handle)
            else:
                raise RuntimeError('invalid section:\n\t%s' % line)
            line = handle.readline()
        handle.close()

    def save(self, handle):
        """
        Save checkpoint to a file.

        :param handle: filename or file object
        :type handle: str

        >>> chk = lm.checkpoint.CheckPoint()
        >>> chk.save('sim.inp')
        """
        handle = lm.common.zhandle(handle, 'wb')
        if self._electrons:
            self._save_label(handle, 'Electrons')
            self._save_values(handle, self._electrons)
        if self._holes:
            self._save_label(handle, 'Holes')
            self._save_values(handle, self._holes)
        if self._defects:
            self._save_label(handle, 'Defects')
            self._save_values(handle, self._defects)
        if self._traps:
            self._save_label(handle, 'Traps')
            self._save_values(handle, self._traps)
        if self._potentials:
            self._save_label(handle, 'TrapPotentials')
            self._save_values(handle, self._potentials)
        if self._flux_state:
            self._save_label(handle, 'FluxState')
            self._save_values(handle, self._flux_state)
        if self._random_state:
            self._save_label(handle, 'RandomState')
            print >> handle, ' '.join(('%d' % i for i in self._random_state))
        if self._parameters:
            self._parameters.save(handle)
        handle.close()

    def clear(self):
        """
        Forget all stored information.

        >>> chk = lm.checkpoint.CheckPoint()
        >>> chk.clear()
        """
        self._electrons = []
        self._holes = []
        self._traps = []
        self._defects = []
        self._potentials = []
        self._flux_state = []
        self._random_state = []
        self._parameters.clear()

    def reset(self, keep_elecs=False, keep_holes=False, traps=True, defects=True):
        """
        Reset a simulation.

        :param keep_elecs: do not delete electrons
        :param keep_holes: do not delete holes
        """
        if not keep_elecs:
            self._electrons = []
        if not keep_holes:
            self._holes = []
        self['current.step'] = 0
        self['random.seed'] = 0
        self._random_state = []
        self._flux_state = []

    def _load_values(self, handle, type_=int):
        """
        Read lines from handle and convert them to type_.
        """
        count = int(handle.readline().strip())
        values = [type_(handle.readline().strip()) for i in range(count)]
        return values

    def _save_values(self, handle, values):
        """
        Write values to handle.
        """
        print >> handle, len(values)
        for v in values:
            print >> handle, v

    def _save_label(self, handle, label):
        """
        Write section header to handle.
        """
        print >> handle, '[%s]' % label

    def __str__(self):
        s  = ''
        s += '[Electrons]     : %d\n' % len(self._electrons)
        s += '[Holes]         : %d\n' % len(self._holes)
        s += '[Traps]         : %d\n' % len(self._traps)
        s += '[Defects]       : %d\n' % len(self._defects)
        s += '[TrapPotenials] : %d\n' % len(self._potentials)
        s += '[FluxState]     : %d\n' % len(self._flux_state)
        s += '[RandomState]   : %d\n' % len(self._random_state)
        s += '[Parameters]    : %d\n' % len(self._parameters)
        s += str(self._parameters)
        return s

    def has_key(self, key, *args, **kwargs):
        """
        Check if key exists in parameters.
        """
        return self._parameters.has_key(key, *args, **kwargs)

    def fix_traps(self):
        """
        Check trap parameter validity.

        >>> chk = lm.checkpoint.CheckPoint()
        >>> chk.fix_traps()
        """
        if self._traps:

            size = len(self._traps)

            if self._potentials:
                if not len(self._potentials) == size:
                    raise RuntimeError('number of traps and potentials ' +
                                       'do not agree')

                value = self._potentials[0]

                count = self._potentials.count(value)
                if not count == len(self._potentials):
                    value = None
            else:
                try:
                    value = self['trap.potential']
                except KeyError:
                    value = 0.5

            if not self._parameters.has_key('grid.x'):
                self['grid.x'] = 1

            if not self._parameters.has_key('grid.y'):
                self['grid.y'] = 1

            if not self._parameters.has_key('grid.z'):
                self['grid.z'] = 1

            volume = self['grid.x'] * self['grid.y'] * self['grid.z']

            if not size <= volume:
                raise RuntimeError('number of traps exceeds volume: %d > %d'
                    % (size, volume))

            percentage = (size + 0.25) / volume

            self['trap.percentage'] = percentage
            self['seed.percentage'] = 0

            if value is None:
                self['trap.potential'] = 0.0
            else:
                self._potentials = []
                self['trap.potential'] = value

    def __getitem__(self, key):
        """
        Access checkpoint attribute or parameters by key
        """
        return self._parameters[key]

    def __setitem__(self, key, value):
        """
        Access checkpoint attribute or parameters by key
        """
        self._parameters[key] = value

def load(handle):
    """
    Load checkpoint file.

    :param handle: filename or file object
    :type handle: str

    >>> chk = lm.checkpoint.load('out.chk')
    """
    if isinstance(handle, CheckPoint):
        return handle
    return CheckPoint(handle)

def load_last(work, **kwargs):
    """
    Load the last checkpoint file found in the working directory.

    :param work: directory to look in
    :type work: str

    >>> chk = lm.checkpoint.load_last('/home/adam/simulations')
    """
    _kwargs = dict(at_least_one=True, r=True)
    _kwargs.update(**kwargs)

    last = lm.find.chks(work, **_kwargs)[-1]
    return load(last)

def compare(chk1, chk2):
    """
    Rigorously compare to checkpoint files.

    :param chk_i: checkpoint object 1
    :param chk_j: checkpoint object 1

    :type chk_i: lm.checkpoint.CheckPoint
    :type chk_j: lm.checkpoint.CheckPoint

    :returns: dict, bool
    """
    results = collections.OrderedDict()

    results['parameters'] = lm.common.compare_dicts(
        chk1.parameters, chk2.parameters)

    results['elecs'] = lm.common.compare_lists(
        chk1.electrons, chk2.electrons)

    results['holes'] = lm.common.compare_lists(
        chk1.holes, chk2.holes)

    results['defects'] = lm.common.compare_lists(
        chk1.defects, chk2.defects)

    results['traps'] = lm.common.compare_lists(
        chk1.traps, chk2.traps)

    results['potentials'] = lm.common.compare_lists(
        chk1.potentials, chk2.potentials)

    results['flux'] = lm.common.compare_lists(
        chk1.flux_state, chk2.flux_state)

    results['random'] = lm.common.compare_lists(
        chk1.random_state, chk2.random_state)

    valid = True
    for key, sub_results in results.iteritems():
        if not sub_results['valid']:
            valid = False
    results['valid'] = valid
    results['invalid'] = not valid

    return results