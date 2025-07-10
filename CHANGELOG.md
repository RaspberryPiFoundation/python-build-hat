# Change Log

## 0.9.0

Fixes:

* Resend 'version' if empty data received from the Build HAT

## 0.8.0

Adds:

* Support for custom firmware (see: https://www.raspberrypi.com/news/build-hat-firmware-now-fully-open-source/)
* Add W503 to Flake8 ignore
* Increase interval for sensor while testing - avoids error that occurs when handling large amount of sensor data. In future, might be interesting to see if we can support faster UART baud rate.

## 0.7.0

Adds:

* Mode 7 IR transmission to ColorDistanceSensor https://github.com/RaspberryPiFoundation/python-build-hat/pull/205
* Debug log filename access https://github.com/RaspberryPiFoundation/python-build-hat/pull/204
* Movement counter to WeDo 2.0 Motion Sensor https://github.com/RaspberryPiFoundation/python-build-hat/pull/201

## 0.6.0

### Added

* Support for Raspberry Pi 5 (https://github.com/RaspberryPiFoundation/python-build-hat/pull/203)

## 0.5.12

### Added

* Ability to set custom device interval for callbacks

### Changed

* New firmware with mitigation for motor disconnecting and selrate command

### Removed

n/a

## 0.5.11

### Added

* Expose release property to allow motor to hold position
* Updated documentation

### Changed

n/a

### Removed

n/a

## 0.5.10

### Added

* Support for 88008 motor
* get_distance support for ColourDistanceSensor

### Changed

* Use debug log level for logging

### Removed

n/a

## 0.5.9

### Added

* Allow the BuildHAT LEDs to be turned on and off
* Allow for White in set\_pixels
* Prevent using same port multiple times
* Add support for checking maximum force sensed

### Changed

* Linting of code
* Renamed exceptions to conform to linter's style

### Removed

n/a

## 0.5.8

### Added

* LED Matrix transitions
* Expose feature to measure voltage of hat
* Function to set brightness of LEDs

### Changed

* New firmware to fix passive devices in hardware revision

### Removed

n/a

## 0.5.7

### Added

* Support for light
* Allow alternative serial device to be used
* Passive motor support
* WeDo sensor support

### Changed

n/a

### Removed

n/a

## 0.5.6

### Added


### Changed

n/a

### Removed

n/a

## 0.5.5

### Added


### Changed

n/a

### Removed

n/a

## 0.5.4

### Added

Documentation copy updates

### Changed

n/a

### Removed

n/a

## 0.5.3

### Added

* Force sensor now supports force threshold
* Simplify list of colours supported by colour sensor
* Fix distance sensor firing bug
* Simplify coast call

### Changed

n/a

### Removed

n/a

## 0.5.2

### Added

* Fixes force sensor callback only firing one
* Pass distance to distance sensor callback
* Make sure motors move simultaneously for a motor pair

### Changed

n/a

### Removed

n/a

## 0.5.1

### Added

Further documentation better describing the Build HAT library components along with supported hardware.

### Changed

n/a

### Removed

n/a
