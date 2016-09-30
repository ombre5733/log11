from collections import namedtuple
import struct


class Deserializer:
    _pointer_fmt = '<I'
    _pointer_fmt = '<Q'

    def __init__(self):
        pass

    def decode(self, data):
        self._data = bytearray(data)
        return self._doDecode()

    def _doDecode(self):
        r = []
        while len(self._data):
            tag = self._readVarUint()
            if tag == 25:
                return r
            if tag > 128:
                s = self._doDecode()
                t = namedtuple('anon_{}'.format(tag),
                               ['{}'.format(x) for x in xrange(len(s))],
                               rename=True)
                r.append(t(*s))
            else:
                r.append(self._map[tag](self))
        return r

    def _readVarInt(self):
        r = 0
        base = 1
        while True:
            b = self._data[0]
            self._data = self._data[1:]
            r += (b & 0x7F) * base
            base <<= 7
            if (b & 0x80) == 0:
                break
        if r % 2 == 0:
            return r / 2
        else:
            return -(r + 1) / 2

    def _readVarUint(self):
        r = 0
        base = 1
        while True:
            b = self._data[0]
            self._data = self._data[1:]
            r += (b & 0x7F) * base
            base <<= 7
            if (b & 0x80) == 0:
                break
        return r

    def _read(self, fmt):
        r = struct.unpack_from(fmt, self._data)
        self._data = self._data[struct.calcsize(fmt):]
        return r[0]

    def _readString(self):
        l = self._readVarUint()
        s = self._data[:l]
        self._data = self._data[l:]
        return str(s)

    def _readStringPointer(self):
        r = self._readVarUint()
        return r


    _map = dict([( 1, lambda s: False),
                 ( 2, lambda s: True),
                 ( 3, lambda s: Deserializer._read(s, '<c')),
                 ( 4, lambda s: Deserializer._read(s, '<b')),
                 ( 5, lambda s: Deserializer._read(s, '<h')),
                 ( 6, lambda s: Deserializer._read(s, '<i')),
                 ( 7, lambda s: Deserializer._read(s, '<q')),
                 ( 8, lambda s: Deserializer._read(s, '<B')),
                 ( 9, lambda s: Deserializer._read(s, '<H')),
                 (10, lambda s: Deserializer._read(s, '<I')),
                 (11, lambda s: Deserializer._read(s, '<Q')),
                 (12, _readVarInt),
                 (13, _readVarInt),
                 (14, _readVarInt),
                 (15, _readVarUint),
                 (16, _readVarUint),
                 (17, _readVarUint),
                 (18, lambda s: Deserializer._read(s, '<f')),
                 (19, lambda s: Deserializer._read(s, '<d')),
                 (21, lambda s: Deserializer._read(s, Deserializer._pointer_fmt)),
                 (22, _readString),
                 (23, _readStringPointer),
                ])
