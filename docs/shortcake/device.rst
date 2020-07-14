Device
======

.. include-comment:: ../../src/device.c


Modes
-----

``Modes`` is the term used to define how a device operates for basic
input and output.  A device may have up to sixteen modes, numbered
sequentially from zero, each of which may provide different numbers of
input values.  For example a colour sensor may have an index colour
mode that returns a single value, the colour index, and an RGB mode
that returns three values at once.

The modes that a device has can be obtained by calling its
:py:meth:`Port.info()` method.  For example, a force sensor has the
following modes::

    >>> for i, mode in enumerate(hub.port.B.info()['modes']):
    ...     print("{0}: {1}: {2}".format(i, mode["name"], mode["format"]))
    ... 
    0: FORCE: {'datasets': 1, 'type': 0, 'figures': 4, 'decimals': 1}
    1: TOUCH: {'datasets': 1, 'type': 0, 'figures': 1, 'decimals': 0}
    2: TAP: {'datasets': 1, 'type': 0, 'figures': 1, 'decimals': 0}
    3: FPEAK: {'datasets': 1, 'type': 0, 'figures': 4, 'decimals': 1}
    4: FRAW: {'datasets': 1, 'type': 1, 'figures': 4, 'decimals': 0}
    5: FPRAW: {'datasets': 1, 'type': 1, 'figures': 4, 'decimals': 0}
    6: CALIB: {'datasets': 8, 'type': 1, 'figures': 4, 'decimals': 0}

So we can see that the sensor has seven modes, and all bar mode 6
return a single value.  Mode 6 itself will return a list of eight
values when :py:meth:`Device.get()` is called, which presumably have
something to do with calibrating the device.  The other different
modes have different purposes, hinted at by their names and formats.
We can set the mode that we want by just passing its mode number to
:py:meth:`Device.mode()`.

Sometimes we will want information from more than one device mode at a
time.  For example, we might need to know not just that the force
sensor has been touched, but what the peak force was.  To that end, it
is possible to set the device to have a combination of modes active at
the same time, all returning data when :py:meth:`Device.get()` is
called.  Not all possible combinations of modes work together.  Which
combinations are allowed can again be found from the dictionary
returned by :py:meth:`Port.info()`::

    >>> [hex(c) for c in hub.port.B.info()["combi_modes"]]
    ['0x3f']

The ``combi_modes`` entry is a tuple of bitmaps.  In each bitmap, set
bits indicate that the corresponding modes can be combined.  In the
simple case of our force sensor we have just one bitmap, which has
bits 0 to 5 all set.  That means that any of modes 0, 1, 2, 3, 4 or 5
can be combined together, but mode 6 cannot be combined with
anything.

A combination mode does not have to contain all the modes in the
corresponding bitmap.  We can just combine modes 1 and 3 of our force
sensor without having to add modes 0, 2, 4 and 5 as well.

To set a device to a combination mode, we must specify which dataset
(value) we want to see for each mode.  We do this by passing a list of
``(mode_number, dataset_number)`` tuples to :py:meth:`Device.mode()`
instead of the single number we passed for a simple mode.  If we want
to pass more than one dataset for a mode, each dataset must go in a
separate tuple.

For our example we have it easy; every mode we can combine only has
one dataset.  We can therefore set up our touch-and-peak combination
like this::

    >>> hub.port.B.device.mode([(1, 0), (3, 0)])
    >>> hub.port.B.device.get()
    [1, 10]
