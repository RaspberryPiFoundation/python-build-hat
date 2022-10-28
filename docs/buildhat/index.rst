================
 Build HAT
================

.. _buildhat_lib:

Library
=======

The Build HAT library has been created to support the `Raspberry Pi Build HAT`_, 
an add-on board for the Raspberry Pi computer, which allows control of up to four LEGO® TECHNIC™ motors and sensors included in the SPIKE™ Portfolio.

.. _Raspberry Pi Build HAT: http://raspberrypi.com/products/build-hat

.. image:: images/BuildHAT_closeup.jpg
   :width: 300
   :alt: The Raspberry Pi Build HAT


Other LEGO® devices may be supported if they use the PoweredUp connector:

.. image:: images/lpf2.jpg
   :width: 100
   :alt: The LEGO PoweredUp connector

In order to drive motors, your Raspberry Pi and Build HAT will need an external 7.5V
power supply. For best results, use the `official Raspberry Pi Build HAT power supply`_. 

.. _official Raspberry Pi Build HAT power supply: http://raspberrypi.com/products/build-hat-power-supply

.. warning::

   The API for the Build HAT is undergoing active development and is subject
   to change. 

.. toctree::
   :maxdepth: 2
  
   colorsensor.rst
   colordistancesensor.rst
   distancesensor.rst
   forcesensor.rst
   light.rst
   matrix.rst
   motionsensor.rst
   motor.rst
   motorpair.rst
   passivemotor.rst
   tiltsensor.rst
   hat.rst
