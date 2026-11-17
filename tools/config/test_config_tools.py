"""Tests for the config packer and UF2 merger.

Run: python3 -m unittest discover tools/config
"""
import copy
import json
import pathlib
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))

import make_config_uf2 as mk  # noqa: E402

LAYOUT = "<IHH6b2x36f6f6f6f6f36f36ffB3x"


def header(name):
    return (ROOT / "firmware" / "include" / name).read_text()


def load(unit):
    return json.loads((ROOT / "configs" / f"{unit}.json").read_text())


def payload(uf2):
    blocks = [uf2[i:i + 512] for i in range(0, len(uf2), 512)]
    return b"".join(b[32:32 + 256] for b in blocks)[:556]


class FirmwareParity(unittest.TestCase):
    """The packer's constants are hand copies of the firmware's."""

    def test_constants_match_tuning_config_h(self):
        h = header("TuningConfig.h")

        def const(name):
            return int(re.search(rf"{name} = (0x[0-9A-Fa-f]+|\d+)", h)
                       .group(1), 0)
        self.assertEqual(const("TUNING_MAGIC"), mk.TUNING_MAGIC)
        self.assertEqual(const("TUNING_VERSION"), mk.TUNING_VERSION)
        self.assertEqual(const("TUNING_FLASH_ADDR"), mk.TUNING_FLASH_ADDR)
        self.assertIn(f"sizeof(TuningConfig) == {struct.calcsize(LAYOUT) + 4}",
                      h)
        coeff = float(re.search(r"TUNING_MAX_COEFF = ([\d.]+)f", h).group(1))
        self.assertEqual(coeff, mk.MAX_COEFF)

    def test_palette_count_matches_config_h(self):
        h = header("Config.h")
        palette = re.search(r"LED_IDLE_PALETTE\[\] = \{(.*?)\};", h, re.S)
        self.assertEqual(len(re.findall(r"0x[0-9A-Fa-f]{6}", palette.group(1))),
                         mk.LED_PALETTE_COUNT)


class BuildBlob(unittest.TestCase):
    def test_round_trip_unit_configs(self):
        for unit in ("unit1", "unit2"):
            cfg = load(unit)
            blob = mk.build_blob(cfg)
            self.assertEqual(len(blob), 556)
            body, (crc,) = blob[:552], struct.unpack("<I", blob[552:])
            self.assertEqual(crc, zlib.crc32(body))
            f = iter(struct.unpack(LAYOUT, body))

            def take(n):
                return [next(f) for _ in range(n)]
            self.assertEqual(take(3), [mk.TUNING_MAGIC, mk.TUNING_VERSION, 0])
            self.assertEqual(take(6), cfg["sign_axis"])
            for key, n in (("decouple", 36), ("trim_pos", 6),
                           ("trim_neg", 6), ("dead_axis", 6),
                           ("curve_expo", 6), ("cross_pos", 36),
                           ("cross_neg", 36), ("smooth_tau_s", 1)):
                v = cfg[key]
                want = ([x for row in v for x in row] if n == 36
                        else v if n == 6 else [v])
                for got, w in zip(take(n), want, strict=True):
                    self.assertAlmostEqual(got, w, places=4, msg=key)
            self.assertEqual(take(1), [cfg["led_default_color_index"]])

    def test_rejects_bad_configs(self):
        nan = float("nan")
        cases = {
            "nan coeff": lambda c: c["decouple"][0].__setitem__(0, nan),
            "huge coeff": lambda c: c["cross_pos"][0].__setitem__(0, 1e39),
            "coeff over bound": lambda c: c["decouple"][1].__setitem__(1, 1001),
            "float sign": lambda c: c["sign_axis"].__setitem__(0, 1.0),
            "zero sign": lambda c: c["sign_axis"].__setitem__(0, 0),
            "short vector": lambda c: c.__setitem__("trim_pos", [1] * 5),
            "unknown key": lambda c: c.__setitem__("dead_zone", 1),
            "missing key": lambda c: c.pop("smooth_tau_s"),
            "led out of palette": lambda c: c.__setitem__(
                "led_default_color_index", mk.LED_PALETTE_COUNT),
            "bool led": lambda c: c.__setitem__("led_default_color_index",
                                                True),
            # Pass in float64 but round onto the bound in float32.
            "dead rounds to limit": lambda c: c["dead_axis"].__setitem__(
                0, 349.99999999),
            "trim rounds to 10": lambda c: c["trim_pos"].__setitem__(
                0, 9.9999999999),
            "trim underflows to 0": lambda c: c["trim_neg"].__setitem__(
                0, 1e-50),
            "int too large for float": lambda c: c["trim_pos"].__setitem__(
                0, 10**400),
        }
        for name, mutate in cases.items():
            with self.subTest(name):
                cfg = copy.deepcopy(load("unit1"))
                mutate(cfg)
                with self.assertRaises((mk.ConfigError, KeyError)):
                    mk.build_blob(cfg)

    def test_cli_reports_bad_json_without_traceback(self):
        with tempfile.TemporaryDirectory() as d:
            bad = pathlib.Path(d, "bad.json")
            bad.write_text("{not json")
            r = subprocess.run(
                [sys.executable, str(HERE / "make_config_uf2.py"), str(bad),
                 str(pathlib.Path(d, "out.uf2"))],
                capture_output=True, text=True)
            self.assertEqual(r.returncode, 1)
            self.assertIn("invalid config", r.stderr)
            self.assertNotIn("Traceback", r.stderr)


@unittest.skipUnless(shutil.which("g++"), "needs a host g++")
class FirmwareValidation(unittest.TestCase):
    """Compiles firmware/src/TuningStore.cpp for the host and feeds it
    packer output, so the packer and the boot check can't drift apart."""

    LOADED, ABSENT, BAD_VERSION, BAD_CRC, BAD_VALUES = range(5)

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = pathlib.Path(cls.tmp.name, "validate")
        subprocess.run(
            ["g++", "-std=gnu++17", "-I", str(HERE / "host"),
             "-I", str(ROOT / "firmware" / "include"),
             str(HERE / "host" / "validate_main.cpp"), "-o", str(cls.exe)],
            check=True)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_exe(self, *args, blob=b""):
        return subprocess.run([str(self.exe), *args], input=blob,
                              capture_output=True, check=True).stdout.decode()

    def test_load_applies_only_valid_blocks(self):
        good = mk.build_blob(load("unit1"))
        self.assertEqual(self.run_exe("--load", blob=good).split(),
                         [str(self.LOADED), "blob"])
        bad = bytearray(good)
        bad[100] ^= 1
        self.assertEqual(self.run_exe("--load", blob=bytes(bad)).split(),
                         [str(self.BAD_CRC), "defaults"])

    def test_compiled_defaults_pass_value_checks(self):
        self.assertEqual(self.run_exe("--defaults-sane").strip(), "1")

    def status(self, blob):
        r = subprocess.run([str(self.exe)], input=blob, capture_output=True,
                           check=True)
        return int(r.stdout)

    @staticmethod
    def resign(body):
        return body + struct.pack("<I", zlib.crc32(body))

    def test_field_offsets_match_packer_order(self):
        # Packer field order, with each field's struct format; pad bytes
        # (2x, 3x) precede the next field.
        order = [("magic", "I"), ("version", "H"), ("reserved", "H"),
                 ("sign_axis", "6b2x"), ("decouple", "36f"),
                 ("trim_pos", "6f"), ("trim_neg", "6f"),
                 ("dead_axis", "6f"), ("curve_expo", "6f"),
                 ("cross_pos", "36f"), ("cross_neg", "36f"),
                 ("smooth_tau_s", "f"), ("led_default_color_index", "B3x"),
                 ("crc32", "I")]
        self.assertEqual("<" + "".join(f for _, f in order[:-1]), LAYOUT)
        expected, off = {}, 0
        for name, fmt in order:
            expected[name] = off
            off += struct.calcsize("<" + fmt)
        r = subprocess.run([str(self.exe), "--offsets"], capture_output=True,
                           text=True, check=True)
        actual = {k: int(v) for k, v in
                  (line.split() for line in r.stdout.splitlines())}
        self.assertEqual(actual, expected)

    def test_packed_unit_configs_load(self):
        for unit in ("unit1", "unit2"):
            with self.subTest(unit):
                self.assertEqual(self.status(mk.build_blob(load(unit))),
                                 self.LOADED)

    def test_rejections(self):
        good = bytearray(mk.build_blob(load("unit1")))
        erased = b"\xff" * 556
        bad_version = bytearray(good[:552])
        struct.pack_into("<H", bad_version, 4, mk.TUNING_VERSION + 1)
        bad_crc = bytearray(good)
        bad_crc[100] ^= 1
        cases = {
            "erased flash": (erased, self.ABSENT),
            "zeroed flash": (b"\x00" * 556, self.ABSENT),
            "other version": (self.resign(bytes(bad_version)),
                              self.BAD_VERSION),
            "bit flip": (bytes(bad_crc), self.BAD_CRC),
        }
        for name, (blob, want) in cases.items():
            with self.subTest(name):
                self.assertEqual(self.status(blob), want)

    # (field, flat index, value). Rejects break exactly one check; accepts
    # sit on the inclusive side of each bound. Last-axis/last-cell indices
    # pin the loop bounds; each case runs through BOTH the packer and the
    # host-compiled firmware, so their ranges can't drift apart.
    REJECT = [
        ("sign_axis", 5, 0),
        ("decouple", 0, 1e30), ("decouple", 35, -1001.0),
        ("decouple", 11, float("nan")),
        ("cross_pos", 35, 1001.0), ("cross_pos", 1, float("nan")),
        ("cross_neg", 35, -1001.0),
        ("trim_pos", 5, 0.0), ("trim_pos", 0, 10.0),
        ("trim_neg", 5, 0.0), ("trim_neg", 0, 10.0),
        ("dead_axis", 5, -1.0), ("dead_axis", 0, 350.0),
        ("curve_expo", 5, 0.49), ("curve_expo", 0, 5.01),
        ("smooth_tau_s", 0, -0.01), ("smooth_tau_s", 0, 1.01),
        ("led_default_color_index", 0, mk.LED_PALETTE_COUNT),
    ]
    ACCEPT = [
        ("decouple", 35, -1000.0), ("cross_pos", 35, 1000.0),
        ("cross_neg", 0, -1000.0),
        ("trim_pos", 5, 9.99), ("trim_neg", 5, 0.01),
        ("dead_axis", 5, 0.0), ("dead_axis", 0, 349.9),
        ("curve_expo", 5, 0.5), ("curve_expo", 0, 5.0),
        ("smooth_tau_s", 0, 0.0), ("smooth_tau_s", 0, 1.0),
        ("led_default_color_index", 0, mk.LED_PALETTE_COUNT - 1),
    ]
    # Start of each field in struct.unpack(LAYOUT, ...) output.
    BASE = {"sign_axis": 3, "decouple": 9, "trim_pos": 45, "trim_neg": 51,
            "dead_axis": 57, "curve_expo": 63, "cross_pos": 69,
            "cross_neg": 105, "smooth_tau_s": 141,
            "led_default_color_index": 142}

    def mutated(self, field, idx, value):
        cfg = copy.deepcopy(load("unit1"))
        v = cfg[field]
        if not isinstance(v, list):
            cfg[field] = value
        elif isinstance(v[0], list):
            v[idx // 6][idx % 6] = value
        else:
            v[idx] = value
        f = list(struct.unpack(LAYOUT, mk.build_blob(load("unit1"))[:552]))
        f[self.BASE[field] + idx] = value
        return cfg, self.resign(struct.pack(LAYOUT, *f))

    def test_packer_and_firmware_reject_the_same_values(self):
        for field, idx, value in self.REJECT:
            with self.subTest(field=field, idx=idx, value=value):
                cfg, blob = self.mutated(field, idx, value)
                with self.assertRaises(mk.ConfigError):
                    mk.build_blob(cfg)
                self.assertEqual(self.status(blob), self.BAD_VALUES)

    def test_packer_and_firmware_accept_bound_values(self):
        for field, idx, value in self.ACCEPT:
            with self.subTest(field=field, idx=idx, value=value):
                cfg, _ = self.mutated(field, idx, value)
                self.assertEqual(self.status(mk.build_blob(cfg)),
                                 self.LOADED)


class MergeUf2(unittest.TestCase):
    def run_merge(self, d, *inputs):
        out = pathlib.Path(d, "merged.uf2")
        r = subprocess.run(
            [sys.executable, str(HERE / "merge_uf2.py"), str(out),
             *map(str, inputs)], capture_output=True, text=True)
        return r, out

    def write(self, d, name, data):
        p = pathlib.Path(d, name)
        p.write_bytes(data)
        return p

    def test_renumbers_blocks(self):
        with tempfile.TemporaryDirectory() as d:
            fw = self.write(d, "fw.uf2", mk.to_uf2(bytes(range(256)) * 3,
                                                   0x10000000))
            cfg = self.write(d, "cfg.uf2",
                             mk.to_uf2(mk.build_blob(load("unit1")),
                                       mk.TUNING_FLASH_ADDR))
            r, out = self.run_merge(d, fw, cfg)
            self.assertEqual(r.returncode, 0, r.stderr)
            data = out.read_bytes()
            blocks = [data[i:i + 512] for i in range(0, len(data), 512)]
            self.assertEqual(len(blocks), 6)
            for i, b in enumerate(blocks):
                self.assertEqual(struct.unpack_from("<II", b, 20), (i, 6))
            self.assertEqual(payload(b"".join(blocks[3:])),
                             mk.build_blob(load("unit1")))

    def test_rejects_overlap_and_bad_magic(self):
        with tempfile.TemporaryDirectory() as d:
            a = self.write(d, "a.uf2", mk.to_uf2(b"\0" * 10, 0x10000000))
            b = self.write(d, "b.uf2", mk.to_uf2(b"\1" * 10, 0x10000000))
            r, _ = self.run_merge(d, a, b)
            self.assertNotEqual(r.returncode, 0)
            self.assertIn("also written by", r.stderr)

            bad = bytearray(a.read_bytes())
            bad[0] ^= 0xFF
            c = self.write(d, "c.uf2", bytes(bad))
            r, _ = self.run_merge(d, c, b)
            self.assertNotEqual(r.returncode, 0)
            self.assertIn("bad UF2 magic", r.stderr)

            t = self.write(d, "t.uf2", a.read_bytes()[:500])
            r, _ = self.run_merge(d, t, b)
            self.assertNotEqual(r.returncode, 0)
            self.assertIn("whole number", r.stderr)


if __name__ == "__main__":
    unittest.main()
