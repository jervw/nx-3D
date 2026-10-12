"""Generate original low-poly AK-47 and M16 OBJ assets for nx-3D.

The rifles point along +Z, with +Y up. All geometry and materials are authored
here; no downloaded models or textures are used.
"""

from pathlib import Path
import math
import struct
import zlib


MATERIALS = {
    "ak_polymer": (0.075, 0.079, 0.083),
    "ak_metal": (0.105, 0.110, 0.115),
    "ak_edge": (0.180, 0.185, 0.190),
    "ak_shadow": (0.025, 0.027, 0.030),
    "m16_polymer": (0.195, 0.200, 0.205),
    "m16_metal": (0.260, 0.270, 0.275),
    "m16_edge": (0.350, 0.360, 0.360),
    "m16_shadow": (0.080, 0.085, 0.090),
}


def write_swatch(path, color):
    def chunk(kind, data):
        return (struct.pack(">I", len(data)) + kind + data
                + struct.pack(">I", zlib.crc32(kind + data)))

    rgb = bytes(round(channel * 255) for channel in color)
    header = b"\x89PNG\r\n\x1a\n"
    header += chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0))
    header += chunk(b"IDAT", zlib.compress(b"\x00" + rgb, 9))
    header += chunk(b"IEND", b"")
    path.write_bytes(header)


class Mesh:
    def __init__(self):
        self.vertices = []
        self.faces = []

    def face(self, points, material):
        start = len(self.vertices) + 1
        self.vertices.extend(points)
        for index in range(1, len(points) - 1):
            self.faces.append((material, (start, start + index, start + index + 1)))

    def box(self, x0, x1, y0, y1, z0, z1, material):
        a = (x0, y0, z0)
        b = (x1, y0, z0)
        c = (x1, y1, z0)
        d = (x0, y1, z0)
        e = (x0, y0, z1)
        f = (x1, y0, z1)
        g = (x1, y1, z1)
        h = (x0, y1, z1)
        for quad in ((a, d, c, b), (e, f, g, h), (a, b, f, e),
                     (d, h, g, c), (a, e, h, d), (b, c, g, f)):
            self.face(quad, material)

    def silhouette(self, points, half_width, material):
        """Extrude a polygon of (z, y) points across the rifle's width."""
        left = [(-half_width, y, z) for z, y in points]
        right = [(half_width, y, z) for z, y in points]
        self.face(list(reversed(left)), material)
        self.face(right, material)
        for index in range(len(points)):
            next_index = (index + 1) % len(points)
            self.face((left[index], left[next_index],
                       right[next_index], right[index]), material)

    def cylinder_z(self, z0, z1, radius, y=0, material="ak_metal", sides=10):
        near = [(radius * math.cos(2 * math.pi * i / sides),
                 y + radius * math.sin(2 * math.pi * i / sides), z0)
                for i in range(sides)]
        far = [(x, yy, z1) for x, yy, _ in near]
        self.face(list(reversed(near)), material)
        self.face(far, material)
        for i in range(sides):
            j = (i + 1) % sides
            self.face((near[i], near[j], far[j], far[i]), material)

    def write(self, name, directory):
        obj = [f"# Original procedural low-poly {name} model", f"mtllib {name}.mtl"]
        obj.extend(f"v {x:.5f} {y:.5f} {z:.5f}" for x, y, z in self.vertices)
        material = None
        for face_material, indices in self.faces:
            if face_material != material:
                material = face_material
                obj.append(f"usemtl {material}")
            obj.append("f " + " ".join(map(str, indices)))
        (directory / f"{name}.obj").write_text("\n".join(obj) + "\n")
        mtl = ["# Original material palette with 1-pixel diffuse swatches"]
        for material_name in sorted({face[0] for face in self.faces}):
            r, g, b = MATERIALS[material_name]
            mtl.extend((f"newmtl {material_name}",
                        f"Ka {r * 0.45:.3f} {g * 0.45:.3f} {b * 0.45:.3f}",
                        f"Kd {r:.3f} {g:.3f} {b:.3f}",
                        f"map_Kd {material_name}.png",
                        "Ks 0.18 0.18 0.18", "Ns 32", "illum 2", ""))
            write_swatch(directory / f"{material_name}.png", (r, g, b))
        (directory / f"{name}.mtl").write_text("\n".join(mtl))


def ak47():
    gun = Mesh()
    gun.box(-.125, .125, -.115, .135, -.37, .48, "ak_metal")
    gun.box(-.117, .117, .136, .172, -.31, .40, "ak_edge")
    gun.box(-.131, -.119, -.05, .07, -.25, .32, "ak_shadow")
    gun.box(.119, .132, -.05, .06, -.25, .32, "ak_edge")
    gun.silhouette([(-.36, .08), (-.55, .035), (-1.17, -.12),
                    (-1.17, -.32), (-.99, -.30), (-.40, -.17)],
                   .098, "ak_polymer")
    gun.box(-.11, .11, -.34, -.12, -1.23, -1.15, "ak_shadow")
    gun.silhouette([(-.30, -.11), (-.48, -.12), (-.62, -.58),
                    (-.45, -.63), (-.24, -.30)], .071, "ak_polymer")
    gun.silhouette([(-.02, -.11), (.15, -.13), (.20, -.43),
                    (.28, -.72), (.48, -.75), (.50, -.91),
                    (.26, -.88), (.03, -.58)], .087, "ak_metal")
    gun.box(-.092, .092, -.54, -.52, .16, .31, "ak_edge")
    gun.silhouette([(.42, .13), (.48, .17), (1.12, .12),
                    (1.17, -.04), (.51, -.06)], .109, "ak_polymer")
    gun.box(-.085, .085, .14, .18, .50, 1.10, "ak_edge")
    gun.cylinder_z(.45, 1.16, .043, .19, "ak_metal", 8)
    gun.cylinder_z(1.08, 1.83, .037, .015, "ak_metal", 10)
    gun.cylinder_z(1.77, 1.93, .056, .015, "ak_shadow", 8)
    gun.box(-.069, .069, -.045, .079, 1.12, 1.23, "ak_metal")
    gun.box(-.047, .047, .075, .175, 1.21, 1.26, "ak_shadow")
    gun.box(-.075, .075, .17, .225, -.31, -.23, "ak_shadow")
    gun.box(-.018, .018, .22, .29, -.29, -.27, "ak_edge")
    for z in (.58, .72, .86, 1.00):
        gun.box(-.111, -.103, -.005, .083, z, z + .025, "ak_shadow")
        gun.box(.103, .111, -.005, .083, z, z + .025, "ak_shadow")
    return gun


def m16():
    gun = Mesh()
    gun.box(-.117, .117, -.12, .13, -.35, .43, "m16_metal")
    gun.silhouette([(-.37, .08), (-.59, .07), (-.76, -.01),
                    (-1.11, -.02), (-1.15, -.21), (-.74, -.22),
                    (-.54, -.14), (-.37, -.14)], .080, "m16_polymer")
    gun.box(-.105, .105, -.25, -.04, -1.19, -1.09, "m16_shadow")
    gun.cylinder_z(-.78, -.36, .048, .025, "m16_metal", 10)
    gun.silhouette([(-.31, -.12), (-.46, -.14), (-.61, -.57),
                    (-.44, -.60), (-.22, -.28)], .071, "m16_polymer")
    gun.silhouette([(.04, -.12), (.19, -.13), (.20, -.73),
                    (.36, -.74), (.34, -.12)], .078, "m16_metal")
    gun.box(-.090, .090, -.75, -.70, .17, .37, "m16_shadow")
    gun.silhouette([(.39, .14), (.48, .15), (1.20, .085),
                    (1.20, -.10), (.46, -.10)], .093, "m16_polymer")
    gun.cylinder_z(.44, 1.33, .045, .015, "m16_metal", 8)
    gun.cylinder_z(1.27, 1.91, .033, .015, "m16_metal", 10)
    gun.cylinder_z(1.87, 2.00, .058, .015, "m16_shadow", 8)
    gun.box(-.088, .088, .135, .169, -.34, .39, "m16_edge")
    gun.box(-.073, .073, .26, .304, -.17, .29, "m16_metal")
    gun.box(-.073, -.052, .17, .303, -.17, -.08, "m16_metal")
    gun.box(.052, .073, .17, .303, -.17, -.08, "m16_metal")
    gun.box(-.073, -.052, .17, .303, .20, .29, "m16_metal")
    gun.box(.052, .073, .17, .303, .20, .29, "m16_metal")
    gun.box(-.080, .080, -.06, .09, 1.31, 1.41, "m16_metal")
    gun.box(-.018, .018, .09, .33, 1.34, 1.38, "m16_shadow")
    gun.box(-.084, .084, .30, .33, 1.34, 1.40, "m16_edge")
    for z in (.59, .75, .91, 1.07):
        gun.box(-.096, -.088, -.055, .085, z, z + .018, "m16_shadow")
        gun.box(.088, .096, -.055, .085, z, z + .018, "m16_shadow")
    return gun


def main():
    output = Path(__file__).resolve().parent
    ak47().write("ak47", output)
    m16().write("m16", output)


if __name__ == "__main__":
    main()
