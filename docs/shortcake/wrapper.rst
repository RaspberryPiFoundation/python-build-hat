Wrapper
=======

.. automodule:: buildhat
   :members:
   :inherited-members:

Example
#######

The following code depicts a simple example using the new wrapper classes.

.. code-block:: python

   from buildhat import Motor, MotorPair, ForceSensor, DistanceSensor

   m1 = Motor('C')
   m1.run_for_rotations(1)
   print(m1.get_position())

   d1 = DistanceSensor('B')
   print(d1.get_distance_cm())

