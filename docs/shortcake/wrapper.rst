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

   f1 = ForceSensor('A')
   print(f1.is_pressed())
   print(f1.get_force_newton())
   print(f1.get_force_percentage())

   p1 = MotorPair('C','D')
   p1.run_for_rotations(0.1)
   p1.run_for_degrees(360)
