"""Capas de ambiente y efectos sincronizadas con los planos (WAV de `Art/Export/Audio`).

Las ganancias son dB relativos a la capa normalizada a -16 LUFS (ver `mix.normalize`): una
capa a -14 dB queda unos 14 LU por debajo de la música, que va a 0 dB.
"""

from __future__ import annotations

from dataclasses import dataclass

from .timeline import Timeline

AMB = "Ambiente"
SFX = "Efectos"


@dataclass(frozen=True)
class Layer:
    folder: str
    name: str
    start: float
    end: float
    gain_points: tuple[tuple[float, float], ...]
    fade_in: float = 0.5
    fade_out: float = 0.5
    loop: bool = True
    offset: float = 0.0

    @property
    def relpath(self) -> str:
        return f"{self.folder}/{self.name}.wav"


def one_shot(name: str, at: float, gain_db: float) -> Layer:
    return Layer(SFX, name, at, at + 60.0, ((at, gain_db),), fade_in=0.005, fade_out=0.02, loop=False)


def _footsteps(start: float, end: float, step: float = 0.56) -> list[Layer]:
    steps = []
    t, k = start, 0
    while t < end:
        steps.append(one_shot(f"sfx_footstep_sand_0{k % 4 + 1}", t, -27.0 + (k % 2) * 1.5))
        t += step
        k += 1
    return steps


def sea_and_land(tl: Timeline) -> list[Layer]:
    s1, s2, s3, s4, s5 = (tl.shot(i) for i in range(1, 6))
    return [
        Layer(AMB, "amb_ocean_calm", 0.0, s5.start + 1.0,
              ((0.0, -8.0), (s2.end - 1.0, -9.0), (s3.start + 1.0, -17.0), (s4.start, -18.0), (s5.start, -18.0)),
              fade_in=0.3, fade_out=1.6),
        Layer(AMB, "amb_wind_light", 0.0, s5.start + 1.0,
              ((0.0, -16.0), (s3.start, -18.0), (s4.start + 1.0, -13.0), (s5.start, -12.0)),
              fade_in=0.3, fade_out=1.6, offset=7.0),
        one_shot("sfx_bird_gull_01", s1.start + 1.3, -19.0),
        one_shot("sfx_bird_gull_03", s2.start + 2.4, -22.0),
        Layer(AMB, "amb_wind_palms", s3.start - 0.6, s3.end + 0.8, ((s3.start, -14.0),), fade_in=0.8, fade_out=1.2),
        Layer(AMB, "amb_jungle_day", s3.start - 0.3, s3.end + 0.5, ((s3.start, -19.0),), fade_in=0.6, fade_out=1.0,
              offset=11.0),
        *_footsteps(s2.start + 1.2, s2.end - 0.6),
    ]


def storm(tl: Timeline) -> list[Layer]:
    s5, s6 = tl.shot(5), tl.shot(6)
    return [
        Layer(AMB, "amb_rain_heavy", s5.start - 0.6, s6.start + 2.5,
              ((s5.start, -9.0), (s6.start, -12.0), (s6.start + 2.5, -20.0)), fade_in=0.6, fade_out=2.0),
        Layer(AMB, "amb_ocean_rough", s5.start - 0.5, s6.end + 0.6,
              ((s5.start, -9.0), (s6.start, -11.0), (s6.end, -15.0)), fade_in=0.5, fade_out=1.4),
        Layer(AMB, "amb_wind_strong", s5.start - 0.5, s6.end,
              ((s5.start, -12.0), (s6.start, -15.0), (s6.end, -20.0)), fade_in=0.5, fade_out=1.5, offset=5.0),
        one_shot("sfx_thunder_02", s5.start + 3.8, -11.0),
        one_shot("sfx_wind_gust_01", s6.start + 1.5, -17.0),
    ]


def finale(tl: Timeline) -> list[Layer]:
    s7, s8 = tl.shot(7), tl.shot(8)
    ignite, lit = tl.ignite_time(), tl.lit_time()
    return [
        Layer(AMB, "amb_ocean_calm", s7.start - 0.8, tl.duration, ((s7.start, -15.0), (s8.start, -16.0)),
              fade_in=0.8, fade_out=0.1, offset=19.0),
        Layer(AMB, "amb_wind_light", s7.start - 0.8, tl.duration, ((s7.start, -20.0),), fade_in=0.8, fade_out=0.1,
              offset=23.0),
        one_shot("sfx_fire_ignite", ignite, -9.0),
        Layer(SFX, "sfx_fire_loop", ignite + 0.4, s7.start + 9.0, ((ignite, -18.0), (lit + 2.0, -21.0)),
              fade_in=0.6, fade_out=2.0),
        Layer(SFX, "sfx_lens_rotation_loop", lit, tl.duration, ((lit, -17.0), (s8.start, -14.0)),
              fade_in=1.2, fade_out=0.1),
    ]


def build_layers(tl: Timeline) -> tuple[Layer, ...]:
    return (*sea_and_land(tl), *storm(tl), *finale(tl))


HORN = Layer(SFX, "sfx_ship_horn", 0.0, 60.0, ((0.0, 0.0),), fade_in=0.005, fade_out=0.05, loop=False)
