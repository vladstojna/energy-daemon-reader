import ctypes
import os
import sys
import enum
import weakref
from typing import Optional, Tuple

shared_lib_path = os.environ.get("ERD_SHARED_LIB", None)
if not shared_lib_path:
    print("Environment variable ERD_SHARED_LIB not set", file=sys.stderr)
    print("Falling back to ./liberd.so", file=sys.stderr)
    shared_lib_path = "./liberd.so"
print("Shared library: {}".format(shared_lib_path))
_erdlib = ctypes.CDLL(shared_lib_path)


class _erd_status_t(ctypes.c_int):
    pass


class _erd_energy_unit_t(ctypes.c_int):
    pass


class _erd_time_unit_t(ctypes.c_int):
    pass


class _erd_domain_t(ctypes.c_int):
    pass


class _erd_error_descriptor_st(ctypes.Structure):
    _fields_ = [
        ("what", ctypes.c_char_p),
        ("size", ctypes.c_uint64),
    ]


class _erd_attr_st(ctypes.Structure):
    pass


class _erd_handle_st(ctypes.Structure):
    pass


class _erd_readings_st(ctypes.Structure):
    _fields_ = [
        ("time", ctypes.c_int64),
        ("energy", ctypes.c_uint64),
        ("tunit", _erd_time_unit_t),
        ("eunit", _erd_energy_unit_t),
    ]


_erd_attr_t = ctypes.POINTER(_erd_attr_st)
_erd_handle_t = ctypes.POINTER(_erd_handle_st)


_erdlib.erd_attr_create.argtypes = (
    ctypes.POINTER(_erd_attr_t),
    ctypes.POINTER(_erd_error_descriptor_st),
)
_erdlib.erd_attr_create.restype = _erd_status_t

_erdlib.erd_attr_destroy.argtypes = (_erd_attr_t,)
_erdlib.erd_attr_destroy.restype = _erd_status_t

_erdlib.erd_attr_set_domain.argtypes = (_erd_attr_t, _erd_domain_t)
_erdlib.erd_attr_set_domain.restype = _erd_status_t

_erdlib.erd_attr_set_socket.argtypes = (_erd_attr_t, ctypes.c_uint32)
_erdlib.erd_attr_set_socket.restype = _erd_status_t

_erdlib.erd_handle_create.argtypes = (
    ctypes.POINTER(_erd_handle_t),
    _erd_attr_t,
    ctypes.POINTER(_erd_error_descriptor_st),
)
_erdlib.erd_handle_create.restype = _erd_status_t

_erdlib.erd_handle_destroy.argtypes = (_erd_handle_t,)
_erdlib.erd_handle_destroy.restype = _erd_status_t

_erdlib.erd_obtain_readings.argtypes = (
    _erd_handle_t,
    ctypes.POINTER(_erd_readings_st),
    ctypes.POINTER(_erd_error_descriptor_st),
)
_erdlib.erd_obtain_readings.restype = _erd_status_t

_erdlib.erd_subtract_readings.argtypes = (
    _erd_handle_t,
    ctypes.POINTER(_erd_readings_st),
    ctypes.POINTER(_erd_readings_st),
    ctypes.POINTER(_erd_readings_st),
)
_erdlib.erd_subtract_readings.restype = _erd_status_t


class StatusCode(enum.Enum):
    success = 0
    unknown = 1
    invalid_status = 2
    invalid_argument = 3
    invalid_unit = 4
    system_error = 5
    alloc_error = 6
    generic_error = 7


class _ErrorDescriptor:
    def __init__(self, size: int = 128) -> None:
        self._buffer = (ctypes.c_char * size)()
        self._ed = _erd_error_descriptor_st(
            ctypes.cast(self._buffer, ctypes.c_char_p), len(self._buffer)
        )

    def clear(self) -> None:
        self._ed.size = len(self._buffer)

    def __str__(self) -> str:
        return str(ctypes.cast(self._buffer, ctypes.c_char_p).value)


class Error(Exception):
    def __init__(
        self, message: str, status: StatusCode, ed: Optional[_ErrorDescriptor] = None
    ) -> None:
        super().__init__(message)
        self.status = status
        self.descriptor = ed

    def __str__(self) -> str:
        if self.descriptor is None:
            return "{}: {}".format(super().__str__(), self.status)
        return "{}: {} (desc: {})".format(
            super().__str__(), self.status, self.descriptor
        )


class Domain(enum.Enum):
    package = 0
    uncore = 1
    cores = 2
    dram = 3


class EnergyUnit(enum.Enum):
    joule = 0
    microjoule = 1


class TimeUnit(enum.Enum):
    second = 0
    nanosecond = 1


class Attributes:
    def __init__(self, domain: Domain, socket: int) -> None:
        self.domain = domain
        self.socket = socket
        self._pointer = None
        self._finalizer = weakref.finalize(self, Attributes._destroy, self._pointer)
        self._create()
        self._setdomain()
        self._setsocket()

    def _create(self) -> None:
        ed = _ErrorDescriptor()
        pointer = ctypes.pointer(_erd_attr_st())
        status: _erd_status_t = _erdlib.erd_attr_create(
            ctypes.byref(pointer), ctypes.byref(ed._ed)
        )
        if status.value != StatusCode.success.value:
            raise Error("Error creating attributes", StatusCode(status.value), ed)
        self._pointer = pointer

    @staticmethod
    def _destroy(pointer: _erd_attr_t) -> None:
        status: _erd_status_t = _erdlib.erd_attr_destroy(pointer)
        if status.value != StatusCode.success.value:
            print(
                "Error destroying attributes: {}".format(StatusCode(status.value)),
                file=sys.stderr,
            )

    def _setdomain(self) -> None:
        status: _erd_status_t = _erdlib.erd_attr_set_domain(
            self._pointer, _erd_domain_t(self.domain.value)
        )
        if status.value != StatusCode.success.value:
            raise Error("Error setting domain", StatusCode(status.value))

    def _setsocket(self) -> None:
        status: _erd_status_t = _erdlib.erd_attr_set_socket(
            self._pointer, ctypes.c_uint32(self.socket)
        )
        if status.value != StatusCode.success.value:
            raise Error("Error setting socket", StatusCode(status.value))

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        self._finalizer()


class _ReadingsBase:
    def __init__(self, native: _erd_readings_st) -> None:
        self._native = native

    def energy(self) -> Tuple[int, EnergyUnit]:
        return (self._native.energy, EnergyUnit(self._native.eunit.value))


class Readings(_ReadingsBase):
    def timestamp(self) -> Tuple[int, TimeUnit]:
        return (self._native.time, TimeUnit(self._native.tunit.value))


class Difference(_ReadingsBase):
    def duration(self) -> Tuple[int, TimeUnit]:
        return (self._native.time, TimeUnit(self._native.tunit.value))


class Handle:
    def __init__(self, attr: Attributes) -> None:
        self.attributes = attr
        self._pointer = None
        self._finalizer = weakref.finalize(self, Handle._destroy, self._pointer)
        self._create()

    def _create(self) -> None:
        ed = _ErrorDescriptor()
        pointer = ctypes.pointer(_erd_handle_st())
        status: _erd_status_t = _erdlib.erd_handle_create(
            ctypes.byref(pointer), self.attributes._pointer, ctypes.byref(ed._ed)
        )
        if status.value != StatusCode.success.value:
            raise Error("Error creating handle", StatusCode(status.value), ed)
        self._pointer = pointer

    @staticmethod
    def _destroy(pointer: _erd_handle_t) -> None:
        status: _erd_status_t = _erdlib.erd_handle_destroy(pointer)
        if status.value != StatusCode.success.value:
            print(
                "Error destroying handle: {}".format(StatusCode(status.value)),
                file=sys.stderr,
            )

    def obtain_readings(self) -> Readings:
        readings_native = _erd_readings_st()
        status: _erd_status_t = _erdlib.erd_obtain_readings(
            self._pointer, ctypes.byref(readings_native), None
        )
        if status.value != StatusCode.success.value:
            raise Error("Error obtaining readings", StatusCode(status.value))
        return Readings(readings_native)

    def subtract(self, lhs: Readings, rhs: Readings) -> Difference:
        readings_native = _erd_readings_st()
        status: _erd_status_t = _erdlib.erd_subtract_readings(
            self._pointer,
            ctypes.byref(lhs._native),
            ctypes.byref(rhs._native),
            ctypes.byref(readings_native),
        )
        if status.value != StatusCode.success.value:
            raise Error("Error obtaining readings", StatusCode(status.value))
        return Difference(readings_native)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        self._finalizer()


if __name__ == "__main__":
    import time

    handle = Handle(Attributes(Domain.package, 0))
    before = handle.obtain_readings()
    time.sleep(1)
    after = handle.obtain_readings()
    diff = handle.subtract(after, before)
    print(diff.energy(), diff.timestamp())
