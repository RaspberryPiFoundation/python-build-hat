"""Exceptions for all build HAT classes"""


class DistanceSensorError(Exception):
    """Error raised when invalid arguments passed to distance sensor functions"""


class MatrixError(Exception):
    """Error raised when invalid arguments passed to matrix functions"""


class LightError(Exception):
    """Error raised when invalid arguments passed to light functions"""


class MotorError(Exception):
    """Error raised when invalid arguments passed to motor functions"""


class BuildHATError(Exception):
    """Error raised when HAT not found"""


class DeviceError(Exception):
    """Error raised when there is a Device issue"""
