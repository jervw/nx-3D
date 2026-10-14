"""Generate an original, low-poly C4 prop with a keypad and visible wiring."""

import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "weapons"))
from generate import MATERIALS, Mesh  # Reuse the project's OBJ and swatch writer.


MATERIALS.update({
    "c4_case": (.075, .084, .082),
    "c4_edge": (.19, .21, .20),
    "c4_strap": (.025, .03, .03),
    "c4_charge": (.28, .30, .27),
    "c4_screen": (.25, .69, .42),
    "c4_key": (.43, .46, .43),
    "c4_red": (.88, .07, .035),
    "c4_yellow": (.84, .55, .06),
    "c4_wire": (.65, .08, .045),
})


def wire(mesh, points, radius, material):
    """Join short eight-sided cylinders along a cable route."""
    for a, b in zip(points, points[1:]):
        direction = [b[i] - a[i] for i in range(3)]
        length = math.sqrt(sum(v * v for v in direction))
        direction = [v / length for v in direction]
        cross = (-direction[2], 0, direction[0])
        cross_length = math.hypot(cross[0], cross[2])
        cross = [v / cross_length for v in cross]
        up = [direction[1] * cross[2],
              direction[2] * cross[0] - direction[0] * cross[2],
              -direction[1] * cross[0]]
        rings = []
        for center in (a, b):
            rings.append([tuple(center[j] + radius * (math.cos(t) * cross[j] + math.sin(t) * up[j])
                                for j in range(3))
                          for t in (2 * math.pi * i / 8 for i in range(8))])
        mesh.face(list(reversed(rings[0])), material)
        mesh.face(rings[1], material)
        for i in range(8):
            j = (i + 1) % 8
            mesh.face((rings[0][i], rings[0][j], rings[1][j], rings[1][i]), material)


def main():
    c4 = Mesh()
    # A squat armored base and the small control unit mounted over two charges.
    c4.box(-15, 15, 0, 5, -20, 20, "c4_case")
    c4.box(-14, 14, 5, 9, -19, 19, "c4_charge")
    for x0, x1 in ((-13, -2), (2, 13)):
        c4.box(x0, x1, 9, 13, -17, 2, "c4_charge")
    for z0, z1 in ((-15, -11), (-3, 1)):
        c4.box(-15.5, 15.5, 12.8, 14.5, z0, z1, "c4_strap")
    c4.box(-11, 11, 9, 16, 3, 18, "c4_case")
    c4.box(-9.5, 9.5, 15.8, 16.6, 5, 16, "c4_edge")
    # Display, bezel, and four rows of keypad buttons are visible from above.
    c4.box(-8, 8, 16.6, 17.1, 10.6, 15, "c4_screen")
    for row in range(4):
        for col in range(3):
            x = -6 + col * 6
            z = 6 + row * .9
            c4.box(x - 1.6, x + 1.6, 16.6, 17.2, z, z + .55, "c4_key")
    c4.box(8.3, 9.3, 16.6, 17.3, 12.6, 13.7, "c4_red")
    c4.box(-9.3, -8.3, 16.6, 17.3, 12.6, 13.7, "c4_yellow")
    wire(c4, [(-11, 11, -16), (-17, 14, -9), (-18, 12, 4), (-10, 15, 9)], .9, "c4_wire")
    wire(c4, [(11, 11, -16), (18, 15, -8), (17, 12, 4), (10, 15, 9)], .9, "c4_yellow")
    c4.write("c4", Path(__file__).resolve().parent)


if __name__ == "__main__":
    main()
