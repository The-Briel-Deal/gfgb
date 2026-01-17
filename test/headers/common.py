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



c_int128 = ctypes.c_ubyte*16
c_uint128 = c_int128
void = None
if ctypes.sizeof(ctypes.c_longdouble) == 16:
    c_long_double_t = ctypes.c_longdouble
else:
    c_long_double_t = ctypes.c_ubyte*16

def string_cast(char_pointer, encoding='utf-8', errors='strict'):
    value = ctypes.cast(char_pointer, ctypes.c_char_p).value
    if value is not None and encoding is not None:
        value = value.decode(encoding, errors=errors)
    return value


def char_pointer_cast(string, encoding='utf-8'):
    if encoding is not None:
        try:
            string = string.encode(encoding)
        except AttributeError:
            # In Python3, bytes has no encode attribute
            pass
    string = ctypes.c_char_p(string)
    return ctypes.cast(string, ctypes.POINTER(ctypes.c_char))



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


class struct_gb_state(Structure):
    pass

class struct_SDL_Window(Structure):
    pass

class struct_SDL_Renderer(Structure):
    pass

class struct_SDL_Palette(Structure):
    pass

class struct__IO_FILE(Structure):
    pass

class struct_regs(Structure):
    pass

class struct_io_regs(Structure):
    pass

struct_io_regs._pack_ = 1 # source:False
struct_io_regs._fields_ = [
    ('sc', ctypes.c_ubyte),
    ('tima', ctypes.c_ubyte),
    ('tma', ctypes.c_ubyte),
    ('tac', ctypes.c_ubyte),
    ('nr10', ctypes.c_ubyte),
    ('nr11', ctypes.c_ubyte),
    ('nr12', ctypes.c_ubyte),
    ('nr13', ctypes.c_ubyte),
    ('nr14', ctypes.c_ubyte),
    ('nr21', ctypes.c_ubyte),
    ('nr22', ctypes.c_ubyte),
    ('nr23', ctypes.c_ubyte),
    ('nr24', ctypes.c_ubyte),
    ('nr30', ctypes.c_ubyte),
    ('nr31', ctypes.c_ubyte),
    ('nr32', ctypes.c_ubyte),
    ('nr33', ctypes.c_ubyte),
    ('nr34', ctypes.c_ubyte),
    ('nr41', ctypes.c_ubyte),
    ('nr42', ctypes.c_ubyte),
    ('nr43', ctypes.c_ubyte),
    ('nr44', ctypes.c_ubyte),
    ('nr50', ctypes.c_ubyte),
    ('nr51', ctypes.c_ubyte),
    ('nr52', ctypes.c_ubyte),
    ('lcdc', ctypes.c_ubyte),
    ('scy', ctypes.c_ubyte),
    ('scx', ctypes.c_ubyte),
    ('bg_pallete', ctypes.c_ubyte),
    ('ie', ctypes.c_ubyte),
    ('if_', ctypes.c_ubyte),
    ('ime', ctypes.c_bool),
]

struct_regs._pack_ = 1 # source:False
struct_regs._fields_ = [
    ('a', ctypes.c_ubyte),
    ('b', ctypes.c_ubyte),
    ('c', ctypes.c_ubyte),
    ('d', ctypes.c_ubyte),
    ('e', ctypes.c_ubyte),
    ('f', ctypes.c_ubyte),
    ('h', ctypes.c_ubyte),
    ('l', ctypes.c_ubyte),
    ('sp', ctypes.c_uint16),
    ('pc', ctypes.c_uint16),
    ('io', struct_io_regs),
]

class struct_debug_symbol_list(Structure):
    pass

class struct_debug_symbol(Structure):
    pass

struct_debug_symbol_list._pack_ = 1 # source:False
struct_debug_symbol_list._fields_ = [
    ('syms', ctypes.POINTER(struct_debug_symbol)),
    ('len', ctypes.c_uint16),
    ('capacity', ctypes.c_uint16),
    ('PADDING_0', ctypes.c_ubyte * 4),
]

class struct_SDL_Texture(Structure):
    pass

struct_gb_state._pack_ = 1 # source:False
struct_gb_state._fields_ = [
    ('sdl_window', ctypes.POINTER(struct_SDL_Window)),
    ('sdl_renderer', ctypes.POINTER(struct_SDL_Renderer)),
    ('sdl_palette', ctypes.POINTER(struct_SDL_Palette)),
    ('regs', struct_regs),
    ('bootrom_mapped', ctypes.c_bool),
    ('bootrom_has_syms', ctypes.c_bool),
    ('rom_loaded', ctypes.c_bool),
    ('bootrom', ctypes.c_ubyte * 256),
    ('rom0', ctypes.c_ubyte * 16384),
    ('rom1', ctypes.c_ubyte * 16384),
    ('wram', ctypes.c_ubyte * 8192),
    ('vram', ctypes.c_ubyte * 8192),
    ('hram', ctypes.c_ubyte * 128),
    ('PADDING_0', ctypes.c_ubyte),
    ('syms', struct_debug_symbol_list),
    ('textures', ctypes.POINTER(struct_SDL_Texture) * 384),
    ('serial_port_output', ctypes.POINTER(struct__IO_FILE)),
    ('last_frame_ticks_ns', ctypes.c_uint64),
]

class struct_SDL_Color(Structure):
    pass

struct_SDL_Palette._pack_ = 1 # source:False
struct_SDL_Palette._fields_ = [
    ('ncolors', ctypes.c_int32),
    ('PADDING_0', ctypes.c_ubyte * 4),
    ('colors', ctypes.POINTER(struct_SDL_Color)),
    ('version', ctypes.c_uint32),
    ('refcount', ctypes.c_int32),
]

struct_SDL_Color._pack_ = 1 # source:False
struct_SDL_Color._fields_ = [
    ('r', ctypes.c_ubyte),
    ('g', ctypes.c_ubyte),
    ('b', ctypes.c_ubyte),
    ('a', ctypes.c_ubyte),
]

struct_debug_symbol._pack_ = 1 # source:False
struct_debug_symbol._fields_ = [
    ('name', ctypes.c_char * 32),
    ('bank', ctypes.c_int32),
    ('start_offset', ctypes.c_uint16),
    ('len', ctypes.c_uint16),
]


# values for enumeration 'SDL_PixelFormat'
SDL_PixelFormat__enumvalues = {
    0: 'SDL_PIXELFORMAT_UNKNOWN',
    286261504: 'SDL_PIXELFORMAT_INDEX1LSB',
    287310080: 'SDL_PIXELFORMAT_INDEX1MSB',
    470811136: 'SDL_PIXELFORMAT_INDEX2LSB',
    471859712: 'SDL_PIXELFORMAT_INDEX2MSB',
    303039488: 'SDL_PIXELFORMAT_INDEX4LSB',
    304088064: 'SDL_PIXELFORMAT_INDEX4MSB',
    318769153: 'SDL_PIXELFORMAT_INDEX8',
    336660481: 'SDL_PIXELFORMAT_RGB332',
    353504258: 'SDL_PIXELFORMAT_XRGB4444',
    357698562: 'SDL_PIXELFORMAT_XBGR4444',
    353570562: 'SDL_PIXELFORMAT_XRGB1555',
    357764866: 'SDL_PIXELFORMAT_XBGR1555',
    355602434: 'SDL_PIXELFORMAT_ARGB4444',
    356651010: 'SDL_PIXELFORMAT_RGBA4444',
    359796738: 'SDL_PIXELFORMAT_ABGR4444',
    360845314: 'SDL_PIXELFORMAT_BGRA4444',
    355667970: 'SDL_PIXELFORMAT_ARGB1555',
    356782082: 'SDL_PIXELFORMAT_RGBA5551',
    359862274: 'SDL_PIXELFORMAT_ABGR1555',
    360976386: 'SDL_PIXELFORMAT_BGRA5551',
    353701890: 'SDL_PIXELFORMAT_RGB565',
    357896194: 'SDL_PIXELFORMAT_BGR565',
    386930691: 'SDL_PIXELFORMAT_RGB24',
    390076419: 'SDL_PIXELFORMAT_BGR24',
    370546692: 'SDL_PIXELFORMAT_XRGB8888',
    371595268: 'SDL_PIXELFORMAT_RGBX8888',
    374740996: 'SDL_PIXELFORMAT_XBGR8888',
    375789572: 'SDL_PIXELFORMAT_BGRX8888',
    372645892: 'SDL_PIXELFORMAT_ARGB8888',
    373694468: 'SDL_PIXELFORMAT_RGBA8888',
    376840196: 'SDL_PIXELFORMAT_ABGR8888',
    377888772: 'SDL_PIXELFORMAT_BGRA8888',
    370614276: 'SDL_PIXELFORMAT_XRGB2101010',
    374808580: 'SDL_PIXELFORMAT_XBGR2101010',
    372711428: 'SDL_PIXELFORMAT_ARGB2101010',
    376905732: 'SDL_PIXELFORMAT_ABGR2101010',
    403714054: 'SDL_PIXELFORMAT_RGB48',
    406859782: 'SDL_PIXELFORMAT_BGR48',
    404766728: 'SDL_PIXELFORMAT_RGBA64',
    405815304: 'SDL_PIXELFORMAT_ARGB64',
    407912456: 'SDL_PIXELFORMAT_BGRA64',
    408961032: 'SDL_PIXELFORMAT_ABGR64',
    437268486: 'SDL_PIXELFORMAT_RGB48_FLOAT',
    440414214: 'SDL_PIXELFORMAT_BGR48_FLOAT',
    438321160: 'SDL_PIXELFORMAT_RGBA64_FLOAT',
    439369736: 'SDL_PIXELFORMAT_ARGB64_FLOAT',
    441466888: 'SDL_PIXELFORMAT_BGRA64_FLOAT',
    442515464: 'SDL_PIXELFORMAT_ABGR64_FLOAT',
    454057996: 'SDL_PIXELFORMAT_RGB96_FLOAT',
    457203724: 'SDL_PIXELFORMAT_BGR96_FLOAT',
    455114768: 'SDL_PIXELFORMAT_RGBA128_FLOAT',
    456163344: 'SDL_PIXELFORMAT_ARGB128_FLOAT',
    458260496: 'SDL_PIXELFORMAT_BGRA128_FLOAT',
    459309072: 'SDL_PIXELFORMAT_ABGR128_FLOAT',
    842094169: 'SDL_PIXELFORMAT_YV12',
    1448433993: 'SDL_PIXELFORMAT_IYUV',
    844715353: 'SDL_PIXELFORMAT_YUY2',
    1498831189: 'SDL_PIXELFORMAT_UYVY',
    1431918169: 'SDL_PIXELFORMAT_YVYU',
    842094158: 'SDL_PIXELFORMAT_NV12',
    825382478: 'SDL_PIXELFORMAT_NV21',
    808530000: 'SDL_PIXELFORMAT_P010',
    542328143: 'SDL_PIXELFORMAT_EXTERNAL_OES',
    1196444237: 'SDL_PIXELFORMAT_MJPG',
    376840196: 'SDL_PIXELFORMAT_RGBA32',
    377888772: 'SDL_PIXELFORMAT_ARGB32',
    372645892: 'SDL_PIXELFORMAT_BGRA32',
    373694468: 'SDL_PIXELFORMAT_ABGR32',
    374740996: 'SDL_PIXELFORMAT_RGBX32',
    375789572: 'SDL_PIXELFORMAT_XRGB32',
    370546692: 'SDL_PIXELFORMAT_BGRX32',
    371595268: 'SDL_PIXELFORMAT_XBGR32',
}
SDL_PIXELFORMAT_UNKNOWN = 0
SDL_PIXELFORMAT_INDEX1LSB = 286261504
SDL_PIXELFORMAT_INDEX1MSB = 287310080
SDL_PIXELFORMAT_INDEX2LSB = 470811136
SDL_PIXELFORMAT_INDEX2MSB = 471859712
SDL_PIXELFORMAT_INDEX4LSB = 303039488
SDL_PIXELFORMAT_INDEX4MSB = 304088064
SDL_PIXELFORMAT_INDEX8 = 318769153
SDL_PIXELFORMAT_RGB332 = 336660481
SDL_PIXELFORMAT_XRGB4444 = 353504258
SDL_PIXELFORMAT_XBGR4444 = 357698562
SDL_PIXELFORMAT_XRGB1555 = 353570562
SDL_PIXELFORMAT_XBGR1555 = 357764866
SDL_PIXELFORMAT_ARGB4444 = 355602434
SDL_PIXELFORMAT_RGBA4444 = 356651010
SDL_PIXELFORMAT_ABGR4444 = 359796738
SDL_PIXELFORMAT_BGRA4444 = 360845314
SDL_PIXELFORMAT_ARGB1555 = 355667970
SDL_PIXELFORMAT_RGBA5551 = 356782082
SDL_PIXELFORMAT_ABGR1555 = 359862274
SDL_PIXELFORMAT_BGRA5551 = 360976386
SDL_PIXELFORMAT_RGB565 = 353701890
SDL_PIXELFORMAT_BGR565 = 357896194
SDL_PIXELFORMAT_RGB24 = 386930691
SDL_PIXELFORMAT_BGR24 = 390076419
SDL_PIXELFORMAT_XRGB8888 = 370546692
SDL_PIXELFORMAT_RGBX8888 = 371595268
SDL_PIXELFORMAT_XBGR8888 = 374740996
SDL_PIXELFORMAT_BGRX8888 = 375789572
SDL_PIXELFORMAT_ARGB8888 = 372645892
SDL_PIXELFORMAT_RGBA8888 = 373694468
SDL_PIXELFORMAT_ABGR8888 = 376840196
SDL_PIXELFORMAT_BGRA8888 = 377888772
SDL_PIXELFORMAT_XRGB2101010 = 370614276
SDL_PIXELFORMAT_XBGR2101010 = 374808580
SDL_PIXELFORMAT_ARGB2101010 = 372711428
SDL_PIXELFORMAT_ABGR2101010 = 376905732
SDL_PIXELFORMAT_RGB48 = 403714054
SDL_PIXELFORMAT_BGR48 = 406859782
SDL_PIXELFORMAT_RGBA64 = 404766728
SDL_PIXELFORMAT_ARGB64 = 405815304
SDL_PIXELFORMAT_BGRA64 = 407912456
SDL_PIXELFORMAT_ABGR64 = 408961032
SDL_PIXELFORMAT_RGB48_FLOAT = 437268486
SDL_PIXELFORMAT_BGR48_FLOAT = 440414214
SDL_PIXELFORMAT_RGBA64_FLOAT = 438321160
SDL_PIXELFORMAT_ARGB64_FLOAT = 439369736
SDL_PIXELFORMAT_BGRA64_FLOAT = 441466888
SDL_PIXELFORMAT_ABGR64_FLOAT = 442515464
SDL_PIXELFORMAT_RGB96_FLOAT = 454057996
SDL_PIXELFORMAT_BGR96_FLOAT = 457203724
SDL_PIXELFORMAT_RGBA128_FLOAT = 455114768
SDL_PIXELFORMAT_ARGB128_FLOAT = 456163344
SDL_PIXELFORMAT_BGRA128_FLOAT = 458260496
SDL_PIXELFORMAT_ABGR128_FLOAT = 459309072
SDL_PIXELFORMAT_YV12 = 842094169
SDL_PIXELFORMAT_IYUV = 1448433993
SDL_PIXELFORMAT_YUY2 = 844715353
SDL_PIXELFORMAT_UYVY = 1498831189
SDL_PIXELFORMAT_YVYU = 1431918169
SDL_PIXELFORMAT_NV12 = 842094158
SDL_PIXELFORMAT_NV21 = 825382478
SDL_PIXELFORMAT_P010 = 808530000
SDL_PIXELFORMAT_EXTERNAL_OES = 542328143
SDL_PIXELFORMAT_MJPG = 1196444237
SDL_PIXELFORMAT_RGBA32 = 376840196
SDL_PIXELFORMAT_ARGB32 = 377888772
SDL_PIXELFORMAT_BGRA32 = 372645892
SDL_PIXELFORMAT_ABGR32 = 373694468
SDL_PIXELFORMAT_RGBX32 = 374740996
SDL_PIXELFORMAT_XRGB32 = 375789572
SDL_PIXELFORMAT_BGRX32 = 370546692
SDL_PIXELFORMAT_XBGR32 = 371595268
SDL_PixelFormat = ctypes.c_uint32 # enum
struct_SDL_Texture._pack_ = 1 # source:False
struct_SDL_Texture._fields_ = [
    ('format', SDL_PixelFormat),
    ('w', ctypes.c_int32),
    ('h', ctypes.c_int32),
    ('refcount', ctypes.c_int32),
]

class struct__IO_marker(Structure):
    pass

class struct__IO_codecvt(Structure):
    pass

class struct__IO_wide_data(Structure):
    pass

struct__IO_FILE._pack_ = 1 # source:False
struct__IO_FILE._fields_ = [
    ('_flags', ctypes.c_int32),
    ('PADDING_0', ctypes.c_ubyte * 4),
    ('_IO_read_ptr', ctypes.POINTER(ctypes.c_char)),
    ('_IO_read_end', ctypes.POINTER(ctypes.c_char)),
    ('_IO_read_base', ctypes.POINTER(ctypes.c_char)),
    ('_IO_write_base', ctypes.POINTER(ctypes.c_char)),
    ('_IO_write_ptr', ctypes.POINTER(ctypes.c_char)),
    ('_IO_write_end', ctypes.POINTER(ctypes.c_char)),
    ('_IO_buf_base', ctypes.POINTER(ctypes.c_char)),
    ('_IO_buf_end', ctypes.POINTER(ctypes.c_char)),
    ('_IO_save_base', ctypes.POINTER(ctypes.c_char)),
    ('_IO_backup_base', ctypes.POINTER(ctypes.c_char)),
    ('_IO_save_end', ctypes.POINTER(ctypes.c_char)),
    ('_markers', ctypes.POINTER(struct__IO_marker)),
    ('_chain', ctypes.POINTER(struct__IO_FILE)),
    ('_fileno', ctypes.c_int32),
    ('_flags2', ctypes.c_int32, 24),
    ('_short_backupbuf', ctypes.c_int32, 8),
    ('_old_offset', ctypes.c_int64),
    ('_cur_column', ctypes.c_uint16),
    ('_vtable_offset', ctypes.c_byte),
    ('_shortbuf', ctypes.c_char * 1),
    ('PADDING_1', ctypes.c_ubyte * 4),
    ('_lock', ctypes.POINTER(None)),
    ('_offset', ctypes.c_int64),
    ('_codecvt', ctypes.POINTER(struct__IO_codecvt)),
    ('_wide_data', ctypes.POINTER(struct__IO_wide_data)),
    ('_freeres_list', ctypes.POINTER(struct__IO_FILE)),
    ('_freeres_buf', ctypes.POINTER(None)),
    ('_prevchain', ctypes.POINTER(ctypes.POINTER(struct__IO_FILE))),
    ('_mode', ctypes.c_int32),
    ('_unused3', ctypes.c_int32),
    ('_total_written', ctypes.c_uint64),
    ('_unused2', ctypes.c_char * 8),
]


# values for enumeration 'io_reg_addr'
io_reg_addr__enumvalues = {
    65281: 'IO_SB',
    65282: 'IO_SC',
    65285: 'IO_TIMA',
    65286: 'IO_TMA',
    65287: 'IO_TAC',
    65296: 'IO_NR10',
    65297: 'IO_NR11',
    65298: 'IO_NR12',
    65299: 'IO_NR13',
    65300: 'IO_NR14',
    65302: 'IO_NR21',
    65303: 'IO_NR22',
    65304: 'IO_NR23',
    65305: 'IO_NR24',
    65306: 'IO_NR30',
    65307: 'IO_NR31',
    65308: 'IO_NR32',
    65309: 'IO_NR33',
    65310: 'IO_NR34',
    65312: 'IO_NR41',
    65313: 'IO_NR42',
    65314: 'IO_NR43',
    65315: 'IO_NR44',
    65316: 'IO_NR50',
    65317: 'IO_NR51',
    65295: 'IO_IF',
    65535: 'IO_IE',
    65318: 'IO_SND_ON',
    65344: 'IO_LCDC',
    65346: 'IO_SCY',
    65347: 'IO_SCX',
    65348: 'IO_LY',
    65351: 'IO_BGP',
}
IO_SB = 65281
IO_SC = 65282
IO_TIMA = 65285
IO_TMA = 65286
IO_TAC = 65287
IO_NR10 = 65296
IO_NR11 = 65297
IO_NR12 = 65298
IO_NR13 = 65299
IO_NR14 = 65300
IO_NR21 = 65302
IO_NR22 = 65303
IO_NR23 = 65304
IO_NR24 = 65305
IO_NR30 = 65306
IO_NR31 = 65307
IO_NR32 = 65308
IO_NR33 = 65309
IO_NR34 = 65310
IO_NR41 = 65312
IO_NR42 = 65313
IO_NR43 = 65314
IO_NR44 = 65315
IO_NR50 = 65316
IO_NR51 = 65317
IO_IF = 65295
IO_IE = 65535
IO_SND_ON = 65318
IO_LCDC = 65344
IO_SCY = 65346
IO_SCX = 65347
IO_LY = 65348
IO_BGP = 65351
io_reg_addr = ctypes.c_uint32 # enum
uint16_t = ctypes.c_uint16
try:
    get_io_reg = _libraries['FIXME_STUB'].get_io_reg
    get_io_reg.restype = ctypes.POINTER(ctypes.c_ubyte)
    get_io_reg.argtypes = [ctypes.POINTER(struct_gb_state), uint16_t]
except AttributeError:
    pass
uint32_t = ctypes.c_uint32
try:
    gb_dots = _libraries['FIXME_STUB'].gb_dots
    gb_dots.restype = uint32_t
    gb_dots.argtypes = []
except AttributeError:
    pass
uint8_t = ctypes.c_uint8
try:
    get_ro_io_reg = _libraries['FIXME_STUB'].get_ro_io_reg
    get_ro_io_reg.restype = uint8_t
    get_ro_io_reg.argtypes = [ctypes.POINTER(struct_gb_state), uint16_t]
except AttributeError:
    pass
try:
    unmap_address = _libraries['FIXME_STUB'].unmap_address
    unmap_address.restype = ctypes.POINTER(None)
    unmap_address.argtypes = [ctypes.POINTER(struct_gb_state), uint16_t]
except AttributeError:
    pass
try:
    read_mem8 = _libraries['FIXME_STUB'].read_mem8
    read_mem8.restype = uint8_t
    read_mem8.argtypes = [ctypes.POINTER(struct_gb_state), uint16_t]
except AttributeError:
    pass
try:
    read_mem16 = _libraries['FIXME_STUB'].read_mem16
    read_mem16.restype = uint16_t
    read_mem16.argtypes = [ctypes.POINTER(struct_gb_state), uint16_t]
except AttributeError:
    pass
try:
    write_mem8 = _libraries['FIXME_STUB'].write_mem8
    write_mem8.restype = None
    write_mem8.argtypes = [ctypes.POINTER(struct_gb_state), uint16_t, uint8_t]
except AttributeError:
    pass
try:
    write_mem16 = _libraries['FIXME_STUB'].write_mem16
    write_mem16.restype = None
    write_mem16.argtypes = [ctypes.POINTER(struct_gb_state), uint16_t, uint16_t]
except AttributeError:
    pass
try:
    gb_state_init = _libraries['FIXME_STUB'].gb_state_init
    gb_state_init.restype = None
    gb_state_init.argtypes = [ctypes.POINTER(struct_gb_state)]
except AttributeError:
    pass
try:
    gb_state_alloc = _libraries['FIXME_STUB'].gb_state_alloc
    gb_state_alloc.restype = ctypes.POINTER(struct_gb_state)
    gb_state_alloc.argtypes = []
except AttributeError:
    pass
__all__ = \
    ['IO_BGP', 'IO_IE', 'IO_IF', 'IO_LCDC', 'IO_LY', 'IO_NR10',
    'IO_NR11', 'IO_NR12', 'IO_NR13', 'IO_NR14', 'IO_NR21', 'IO_NR22',
    'IO_NR23', 'IO_NR24', 'IO_NR30', 'IO_NR31', 'IO_NR32', 'IO_NR33',
    'IO_NR34', 'IO_NR41', 'IO_NR42', 'IO_NR43', 'IO_NR44', 'IO_NR50',
    'IO_NR51', 'IO_SB', 'IO_SC', 'IO_SCX', 'IO_SCY', 'IO_SND_ON',
    'IO_TAC', 'IO_TIMA', 'IO_TMA', 'SDL_PIXELFORMAT_ABGR128_FLOAT',
    'SDL_PIXELFORMAT_ABGR1555', 'SDL_PIXELFORMAT_ABGR2101010',
    'SDL_PIXELFORMAT_ABGR32', 'SDL_PIXELFORMAT_ABGR4444',
    'SDL_PIXELFORMAT_ABGR64', 'SDL_PIXELFORMAT_ABGR64_FLOAT',
    'SDL_PIXELFORMAT_ABGR8888', 'SDL_PIXELFORMAT_ARGB128_FLOAT',
    'SDL_PIXELFORMAT_ARGB1555', 'SDL_PIXELFORMAT_ARGB2101010',
    'SDL_PIXELFORMAT_ARGB32', 'SDL_PIXELFORMAT_ARGB4444',
    'SDL_PIXELFORMAT_ARGB64', 'SDL_PIXELFORMAT_ARGB64_FLOAT',
    'SDL_PIXELFORMAT_ARGB8888', 'SDL_PIXELFORMAT_BGR24',
    'SDL_PIXELFORMAT_BGR48', 'SDL_PIXELFORMAT_BGR48_FLOAT',
    'SDL_PIXELFORMAT_BGR565', 'SDL_PIXELFORMAT_BGR96_FLOAT',
    'SDL_PIXELFORMAT_BGRA128_FLOAT', 'SDL_PIXELFORMAT_BGRA32',
    'SDL_PIXELFORMAT_BGRA4444', 'SDL_PIXELFORMAT_BGRA5551',
    'SDL_PIXELFORMAT_BGRA64', 'SDL_PIXELFORMAT_BGRA64_FLOAT',
    'SDL_PIXELFORMAT_BGRA8888', 'SDL_PIXELFORMAT_BGRX32',
    'SDL_PIXELFORMAT_BGRX8888', 'SDL_PIXELFORMAT_EXTERNAL_OES',
    'SDL_PIXELFORMAT_INDEX1LSB', 'SDL_PIXELFORMAT_INDEX1MSB',
    'SDL_PIXELFORMAT_INDEX2LSB', 'SDL_PIXELFORMAT_INDEX2MSB',
    'SDL_PIXELFORMAT_INDEX4LSB', 'SDL_PIXELFORMAT_INDEX4MSB',
    'SDL_PIXELFORMAT_INDEX8', 'SDL_PIXELFORMAT_IYUV',
    'SDL_PIXELFORMAT_MJPG', 'SDL_PIXELFORMAT_NV12',
    'SDL_PIXELFORMAT_NV21', 'SDL_PIXELFORMAT_P010',
    'SDL_PIXELFORMAT_RGB24', 'SDL_PIXELFORMAT_RGB332',
    'SDL_PIXELFORMAT_RGB48', 'SDL_PIXELFORMAT_RGB48_FLOAT',
    'SDL_PIXELFORMAT_RGB565', 'SDL_PIXELFORMAT_RGB96_FLOAT',
    'SDL_PIXELFORMAT_RGBA128_FLOAT', 'SDL_PIXELFORMAT_RGBA32',
    'SDL_PIXELFORMAT_RGBA4444', 'SDL_PIXELFORMAT_RGBA5551',
    'SDL_PIXELFORMAT_RGBA64', 'SDL_PIXELFORMAT_RGBA64_FLOAT',
    'SDL_PIXELFORMAT_RGBA8888', 'SDL_PIXELFORMAT_RGBX32',
    'SDL_PIXELFORMAT_RGBX8888', 'SDL_PIXELFORMAT_UNKNOWN',
    'SDL_PIXELFORMAT_UYVY', 'SDL_PIXELFORMAT_XBGR1555',
    'SDL_PIXELFORMAT_XBGR2101010', 'SDL_PIXELFORMAT_XBGR32',
    'SDL_PIXELFORMAT_XBGR4444', 'SDL_PIXELFORMAT_XBGR8888',
    'SDL_PIXELFORMAT_XRGB1555', 'SDL_PIXELFORMAT_XRGB2101010',
    'SDL_PIXELFORMAT_XRGB32', 'SDL_PIXELFORMAT_XRGB4444',
    'SDL_PIXELFORMAT_XRGB8888', 'SDL_PIXELFORMAT_YUY2',
    'SDL_PIXELFORMAT_YV12', 'SDL_PIXELFORMAT_YVYU', 'SDL_PixelFormat',
    'gb_dots', 'gb_state_alloc', 'gb_state_init', 'get_io_reg',
    'get_ro_io_reg', 'io_reg_addr', 'read_mem16', 'read_mem8',
    'struct_SDL_Color', 'struct_SDL_Palette', 'struct_SDL_Renderer',
    'struct_SDL_Texture', 'struct_SDL_Window', 'struct__IO_FILE',
    'struct__IO_codecvt', 'struct__IO_marker', 'struct__IO_wide_data',
    'struct_debug_symbol', 'struct_debug_symbol_list',
    'struct_gb_state', 'struct_io_regs', 'struct_regs', 'uint16_t',
    'uint32_t', 'uint8_t', 'unmap_address', 'write_mem16',
    'write_mem8']
