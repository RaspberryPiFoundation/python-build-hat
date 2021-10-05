.. _buildhat_lib:

Library
=======

The Build HAT library has been created to support the Raspberry Pi Build HAT, 
an add-on board for the Raspberry Pi computer which allows control of up to 4 LEGO® Technic™ motors and sensors included in the SPIKE™ Portfolio.

.. image:: images/BuildHAT_closeup.jpg
   :width: 300
   :alt: The Raspberry Pi Build HAT


Other LEGO® devices may be supported if they use the LPF2 connector:

.. image:: images/lpf2.jpg
   :width: 100
   :alt: The LEGO LPF2 connector

In order to drive motors, your Raspberry Pi and Build HAT will need an external 7.5v
power supply. For best results, use the official Raspberry Pi Build HAT power supply. 

.. warning::

   The API for the Build HAT is undergoing active development and is subject
   to change. 

.. toctree::
   :maxdepth: 2
  
   colorsensor.rst
   colordistancesensor.rst
   distancesensor.rst
   forcesensor.rst
   matrix.rst
   motor.rst
   motorpair.rst
