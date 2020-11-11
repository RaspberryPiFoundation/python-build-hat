from build_hat import BuildHAT

"""
    Assign attributes from actual device object, to our wrapper.

    This functions better than simply overriding __getattr__ 
    method, as it allows autocompletion via the Python REPL.
"""

def patchattr(toobj, fromobj):
    for attribute in dir(fromobj):
        if not attribute.startswith("__") and getattr(fromobj, attribute):
            setattr(toobj, attribute, getattr(fromobj, attribute))


class Device:

    instance = None

    def __init__(self, port):
        if not Device.instance:
            Device.instance = BuildHAT()
        self.port = getattr(self.instance.port, port)


class Motor(Device):
    
    def __init__(self, port):
        super().__init__(port)
        patchattr(self, self.port.motor)    
    
    """
    def __getattr__(self, name):
        return getattr(self.port.motor, name)
    """

