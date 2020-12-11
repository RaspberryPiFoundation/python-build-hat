from buildhat import ColorSensor

color = ColorSensor('B')

print("HSV", color.get_color_hsv())
print("RGBI", color.get_color_rgbi())
print("Ambient", color.get_ambient_light())
print("Reflected", color.get_reflected_light())
print("Color", color.get_color())

print("Waiting for color red")
color.wait_until_color("red")
print("Found color red!")

while True:
    c = color.wait_for_new_color()
    print("Found new color", c)
