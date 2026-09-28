"""Sonoridad LUFS aproximada dentro de un rango razonable por familia de
sonido. Los rangos son deliberadamente amplios (varios dB de margen sobre lo
medido durante el diseño): el objetivo es atrapar errores groseros -un
fichero mudo, o uno que se dispara muy por encima del resto de su familia-
no exigir una sonoridad exacta."""

from __future__ import annotations

from explored_audio.levels import integrated_lufs, lufs_approx


def _expected_range(name: str) -> tuple[float, float]:
    if name.startswith("amb_"):
        # Los colchones de ambiente se normalizan a una sonoridad de
        # referencia (ver AMBIENCE_TARGET_LUFS en build.py): el rango es
        # estrecho a proposito, es una comprobacion real de esa normalizacion.
        return (-27.0, -20.0)
    if name in ("sfx_fire_loop", "sfx_cooking_sizzle_loop"):
        return (-28.0, -12.0)
    if name == "sfx_heartbeat_low_loop":
        return (-34.0, -16.0)
    if name.startswith("sfx_thunder"):
        return (-40.0, -24.0)
    if name.startswith("sfx_bird"):
        return (-18.0, -2.0)
    if name.startswith("sfx_ui"):
        return (-20.0, -6.0)
    if name.startswith("sfx_footstep"):
        return (-26.0, -8.0)
    # Impactos, chapoteos, recogida, fauna corta, herramientas, cuerpo:
    # transitorios con cresta alta, LUFS bajo el pico.
    return (-36.0, -12.0)


def test_lufs_dentro_de_rango_por_familia(catalog, rendered):
    for spec in catalog:
        if spec.category == "Musica":
            continue  # se mide con sonoridad integrada, ver el test siguiente
        audio = rendered[spec.name]
        lufs = lufs_approx(audio)
        low, high = _expected_range(spec.name)
        assert low <= lufs <= high, f"{spec.name}: {lufs:.1f} LUFS fuera de [{low}, {high}]"


def test_musica_a_menos_16_lufs_integrados(catalog, rendered):
    """La musica se masteriza a MUSIC_TARGET_LUFS (-16) integrados con puertas
    BS.1770: tolerancia de medio LU, es la comprobacion real del master."""
    for spec in catalog:
        if spec.category != "Musica":
            continue
        lufs = integrated_lufs(rendered[spec.name])
        assert abs(lufs - (-16.0)) <= 0.5, f"{spec.name}: {lufs:.2f} LUFS integrados, se esperaban -16"
