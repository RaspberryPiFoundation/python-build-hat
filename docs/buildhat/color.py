"""Example getting color values from color sensor"""

from buildhat import ColorSensor

color = ColorSensor('C')

print("HSV", color.get_color_hsv())
print("RGBI", color.get_color_rgbi())
print("Ambient", color.get_ambient_light())
print("Reflected", color.get_reflected_light())
print("Color", color.get_color())

print("Waiting for color black")
color.wait_until_color("black")
print("Found color black")

print("Waiting for color white")
color.wait_until_color("white")
print("Found color white")

while True:
    c = color.wait_for_new_color()
    print("Found new color", c)
