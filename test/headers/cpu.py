# -*- coding: utf-8 -*-
#
# TARGET arch is: ['-I/usr/lib/clang/21/include/']
# WORD_SIZE is: 8
# POINTER_SIZE is: 8
# LONGDOUBLE_SIZE is: 16
#
import ctypes


class AsDictMixin:
    @classmethod
    def as_dict(cls, self):
        result = {}
        if not isinstance(self, AsDictMixin):
            # not a structure, assume it's already a python object
            return self
        if not hasattr(cls, "_fields_"):
            return result
        # sys.version_info >= (3, 5)
        # for (field, *_) in cls._fields_:  # noqa
        for field_tuple in cls._fields_:  # noqa
            field = field_tuple[0]
            if field.startswith('PADDING_'):
                continue
            value = getattr(self, field)
            type_ = type(value)
            if hasattr(value, "_length_") and hasattr(value, "_type_"):
                # array
                type_ = type_._type_
                if hasattr(type_, 'as_dict'):
                    value = [type_.as_dict(v) for v in value]
                else:
                    value = [i for i in value]
            elif hasattr(value, "contents") and hasattr(value, "_type_"):
                # pointer
                try:
                    if not hasattr(type_, "as_dict"):
                        value = value.contents
                    else:
                        type_ = type_._type_
                        value = type_.as_dict(value.contents)
                except ValueError:
                    # nullptr
                    value = None
            elif isinstance(value, AsDictMixin):
                # other structure
                value = type_.as_dict(value)
            result[field] = value
        return result


class Structure(ctypes.Structure, AsDictMixin):

    def __init__(self, *args, **kwds):
        # We don't want to use positional arguments fill PADDING_* fields

        args = dict(zip(self.__class__._field_names_(), args))
        args.update(kwds)
        super(Structure, self).__init__(**args)

    @classmethod
    def _field_names_(cls):
        if hasattr(cls, '_fields_'):
            return (f[0] for f in cls._fields_ if not f[0].startswith('PADDING'))
        else:
            return ()

    @classmethod
    def get_type(cls, field):
        for f in cls._fields_:
            if f[0] == field:
                return f[1]
        return None

    @classmethod
    def bind(cls, bound_fields):
        fields = {}
        for name, type_ in cls._fields_:
            if hasattr(type_, "restype"):
                if name in bound_fields:
                    if bound_fields[name] is None:
                        fields[name] = type_()
                    else:
                        # use a closure to capture the callback from the loop scope
                        fields[name] = (
                            type_((lambda callback: lambda *args: callback(*args))(
                                bound_fields[name]))
                        )
                    del bound_fields[name]
                else:
                    # default callback implementation (does nothing)
                    try:
                        default_ = type_(0).restype().value
                    except TypeError:
                        default_ = None
                    fields[name] = type_((
                        lambda default_: lambda *args: default_)(default_))
            else:
                # not a callback function, use default initialization
                if name in bound_fields:
                    fields[name] = bound_fields[name]
                    del bound_fields[name]
                else:
                    fields[name] = type_()
        if len(bound_fields) != 0:
            raise ValueError(
                "Cannot bind the following unknown callback(s) {}.{}".format(
                    cls.__name__, bound_fields.keys()
            ))
        return cls(**fields)


class Union(ctypes.Union, AsDictMixin):
    pass



class FunctionFactoryStub:
    def __getattr__(self, _):
      return ctypes.CFUNCTYPE(lambda y:y)

# libraries['FIXME_STUB'] explanation
# As you did not list (-l libraryname.so) a library that exports this function
# This is a non-working stub instead. 
# You can either re-run clan2py with -l /path/to/library.so
# Or manually fix this by comment the ctypes.CDLL loading
_libraries = {}
_libraries['FIXME_STUB'] = FunctionFactoryStub() #  ctypes.CDLL('FIXME_STUB')
c_int128 = ctypes.c_ubyte*16
c_uint128 = c_int128
void = None
if ctypes.sizeof(ctypes.c_longdouble) == 16:
    c_long_double_t = ctypes.c_longdouble
else:
    c_long_double_t = ctypes.c_ubyte*16




# values for enumeration 'r8'
r8__enumvalues = {
    0: 'R8_B',
    1: 'R8_C',
    2: 'R8_D',
    3: 'R8_E',
    4: 'R8_H',
    5: 'R8_L',
    6: 'R8_HL_DREF',
    7: 'R8_A',
}
R8_B = 0
R8_C = 1
R8_D = 2
R8_E = 3
R8_H = 4
R8_L = 5
R8_HL_DREF = 6
R8_A = 7
r8 = ctypes.c_uint32 # enum

# values for enumeration 'r16'
r16__enumvalues = {
    0: 'R16_BC',
    1: 'R16_DE',
    2: 'R16_HL',
    3: 'R16_SP',
}
R16_BC = 0
R16_DE = 1
R16_HL = 2
R16_SP = 3
r16 = ctypes.c_uint32 # enum

# values for enumeration 'r16_mem'
r16_mem__enumvalues = {
    0: 'R16_MEM_BC',
    1: 'R16_MEM_DE',
    2: 'R16_MEM_HLI',
    3: 'R16_MEM_HLD',
}
R16_MEM_BC = 0
R16_MEM_DE = 1
R16_MEM_HLI = 2
R16_MEM_HLD = 3
r16_mem = ctypes.c_uint32 # enum

# values for enumeration 'r16_stk'
r16_stk__enumvalues = {
    0: 'R16_STK_BC',
    1: 'R16_STK_DE',
    2: 'R16_STK_HL',
    3: 'R16_STK_AF',
}
R16_STK_BC = 0
R16_STK_DE = 1
R16_STK_HL = 2
R16_STK_AF = 3
r16_stk = ctypes.c_uint32 # enum

# values for enumeration 'cond'
cond__enumvalues = {
    0: 'COND_NZ',
    1: 'COND_Z',
    2: 'COND_NC',
    3: 'COND_C',
}
COND_NZ = 0
COND_Z = 1
COND_NC = 2
COND_C = 3
cond = ctypes.c_uint32 # enum
class struct_inst_param(Structure):
    pass


# values for enumeration 'inst_param_type'
inst_param_type__enumvalues = {
    0: 'R8',
    1: 'R16',
    2: 'R16_MEM',
    3: 'R16_STK',
    4: 'IMM16',
    5: 'IMM8',
    6: 'IMM8_HMEM',
    7: 'SP_IMM8',
    8: 'IMM16_MEM',
    9: 'B3',
    10: 'COND',
    11: 'UNKNOWN_INST_BYTE',
    12: 'VOID_PARAM_TYPE',
}
R8 = 0
R16 = 1
R16_MEM = 2
R16_STK = 3
IMM16 = 4
IMM8 = 5
IMM8_HMEM = 6
SP_IMM8 = 7
IMM16_MEM = 8
B3 = 9
COND = 10
UNKNOWN_INST_BYTE = 11
VOID_PARAM_TYPE = 12
inst_param_type = ctypes.c_uint32 # enum
class union_inst_param_0(Union):
    pass

union_inst_param_0._pack_ = 1 # source:False
union_inst_param_0._fields_ = [
    ('r8', r8),
    ('r16', r16),
    ('r16_mem', r16_mem),
    ('r16_stk', r16_stk),
    ('cond', cond),
    ('imm8', ctypes.c_ubyte),
    ('imm16', ctypes.c_uint16),
    ('b3', ctypes.c_ubyte),
    ('unknown_inst_byte', ctypes.c_ubyte),
    ('PADDING_0', ctypes.c_ubyte * 3),
]

struct_inst_param._pack_ = 1 # source:False
struct_inst_param._anonymous_ = ('_0',)
struct_inst_param._fields_ = [
    ('type', inst_param_type),
    ('_0', union_inst_param_0),
]

class struct_inst(Structure):
    pass


# values for enumeration 'inst_type'
inst_type__enumvalues = {
    0: 'ADC',
    1: 'ADD',
    2: 'AND',
    3: 'BIT',
    4: 'CALL',
    5: 'CCF',
    6: 'CP',
    7: 'CPL',
    8: 'DAA',
    9: 'DEC',
    10: 'INC',
    11: 'JP',
    12: 'JR',
    13: 'LD',
    14: 'LDH',
    15: 'NOP',
    16: 'OR',
    17: 'POP',
    18: 'PUSH',
    19: 'RES',
    20: 'RET',
    21: 'RL',
    22: 'RLA',
    23: 'RLC',
    24: 'RLCA',
    25: 'RR',
    26: 'RRA',
    27: 'RRC',
    28: 'RRCA',
    29: 'SBC',
    30: 'SCF',
    31: 'SET',
    32: 'SLA',
    33: 'SRA',
    34: 'SRL',
    35: 'SUB',
    36: 'SWAP',
    37: 'XOR',
    38: 'DI',
    39: 'EI',
    40: 'UNKNOWN_INST',
}
ADC = 0
ADD = 1
AND = 2
BIT = 3
CALL = 4
CCF = 5
CP = 6
CPL = 7
DAA = 8
DEC = 9
INC = 10
JP = 11
JR = 12
LD = 13
LDH = 14
NOP = 15
OR = 16
POP = 17
PUSH = 18
RES = 19
RET = 20
RL = 21
RLA = 22
RLC = 23
RLCA = 24
RR = 25
RRA = 26
RRC = 27
RRCA = 28
SBC = 29
SCF = 30
SET = 31
SLA = 32
SRA = 33
SRL = 34
SUB = 35
SWAP = 36
XOR = 37
DI = 38
EI = 39
UNKNOWN_INST = 40
inst_type = ctypes.c_uint32 # enum
struct_inst._pack_ = 1 # source:False
struct_inst._fields_ = [
    ('type', inst_type),
    ('p1', struct_inst_param),
    ('p2', struct_inst_param),
]

class struct_gb_state(Structure):
    pass

uint8_t = ctypes.c_uint8
try:
    get_r8 = _libraries['FIXME_STUB'].get_r8
    get_r8.restype = uint8_t
    get_r8.argtypes = [ctypes.POINTER(struct_gb_state), r8]
except AttributeError:
    pass
try:
    set_r8 = _libraries['FIXME_STUB'].set_r8
    set_r8.restype = None
    set_r8.argtypes = [ctypes.POINTER(struct_gb_state), r8, uint8_t]
except AttributeError:
    pass
uint16_t = ctypes.c_uint16
try:
    get_r16 = _libraries['FIXME_STUB'].get_r16
    get_r16.restype = uint16_t
    get_r16.argtypes = [ctypes.POINTER(struct_gb_state), r16]
except AttributeError:
    pass
try:
    set_r16 = _libraries['FIXME_STUB'].set_r16
    set_r16.restype = None
    set_r16.argtypes = [ctypes.POINTER(struct_gb_state), r16, uint16_t]
except AttributeError:
    pass
try:
    get_r16_mem = _libraries['FIXME_STUB'].get_r16_mem
    get_r16_mem.restype = uint16_t
    get_r16_mem.argtypes = [ctypes.POINTER(struct_gb_state), r16_mem]
except AttributeError:
    pass
try:
    set_r16_mem = _libraries['FIXME_STUB'].set_r16_mem
    set_r16_mem.restype = None
    set_r16_mem.argtypes = [ctypes.POINTER(struct_gb_state), r16, uint8_t]
except AttributeError:
    pass
try:
    get_r16_stk = _libraries['FIXME_STUB'].get_r16_stk
    get_r16_stk.restype = uint16_t
    get_r16_stk.argtypes = [ctypes.POINTER(struct_gb_state), r16_stk]
except AttributeError:
    pass
try:
    set_r16_stk = _libraries['FIXME_STUB'].set_r16_stk
    set_r16_stk.restype = None
    set_r16_stk.argtypes = [ctypes.POINTER(struct_gb_state), r16_stk, uint16_t]
except AttributeError:
    pass
try:
    fetch = _libraries['FIXME_STUB'].fetch
    fetch.restype = struct_inst
    fetch.argtypes = [ctypes.POINTER(struct_gb_state)]
except AttributeError:
    pass
try:
    execute = _libraries['FIXME_STUB'].execute
    execute.restype = None
    execute.argtypes = [ctypes.POINTER(struct_gb_state), struct_inst]
except AttributeError:
    pass
__all__ = \
    ['ADC', 'ADD', 'AND', 'B3', 'BIT', 'CALL', 'CCF', 'COND',
    'COND_C', 'COND_NC', 'COND_NZ', 'COND_Z', 'CP', 'CPL', 'DAA',
    'DEC', 'DI', 'EI', 'IMM16', 'IMM16_MEM', 'IMM8', 'IMM8_HMEM',
    'INC', 'JP', 'JR', 'LD', 'LDH', 'NOP', 'OR', 'POP', 'PUSH', 'R16',
    'R16_BC', 'R16_DE', 'R16_HL', 'R16_MEM', 'R16_MEM_BC',
    'R16_MEM_DE', 'R16_MEM_HLD', 'R16_MEM_HLI', 'R16_SP', 'R16_STK',
    'R16_STK_AF', 'R16_STK_BC', 'R16_STK_DE', 'R16_STK_HL', 'R8',
    'R8_A', 'R8_B', 'R8_C', 'R8_D', 'R8_E', 'R8_H', 'R8_HL_DREF',
    'R8_L', 'RES', 'RET', 'RL', 'RLA', 'RLC', 'RLCA', 'RR', 'RRA',
    'RRC', 'RRCA', 'SBC', 'SCF', 'SET', 'SLA', 'SP_IMM8', 'SRA',
    'SRL', 'SUB', 'SWAP', 'UNKNOWN_INST', 'UNKNOWN_INST_BYTE',
    'VOID_PARAM_TYPE', 'XOR', 'cond', 'execute', 'fetch', 'get_r16',
    'get_r16_mem', 'get_r16_stk', 'get_r8', 'inst_param_type',
    'inst_type', 'r16', 'r16_mem', 'r16_stk', 'r8', 'set_r16',
    'set_r16_mem', 'set_r16_stk', 'set_r8', 'struct_gb_state',
    'struct_inst', 'struct_inst_param', 'uint16_t', 'uint8_t',
    'union_inst_param_0']
