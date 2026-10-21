// Generado por Tools/Style (`uv run style-ui`). No editar a mano: la fuente es
// Tools/Style/paleta.json (colores) y Tools/Style/explored_style/ui_icons.py (iconos).
#pragma once

#include "CoreMinimal.h"

/** Colores de la paleta única (sRGB, GDD v3 §11). La interfaz los convierte con ExploredPalette::Linear. */
namespace ExploredPalette
{
	inline const FColor ArenaClara(232, 216, 168); // 0 arena_clara
	inline const FColor ArenaOscura(201, 178, 124); // 1 arena_oscura
	inline const FColor HierbaClara(141, 187, 90); // 2 hierba_clara
	inline const FColor HierbaOscura(79, 138, 60); // 3 hierba_oscura
	inline const FColor VerdePalma(62, 125, 58); // 4 verde_palma
	inline const FColor MaderaClara(185, 138, 90); // 5 madera_clara
	inline const FColor MaderaOscura(122, 82, 51); // 6 madera_oscura
	inline const FColor RocaMedia(124, 124, 120); // 7 roca_media
	inline const FColor Oxido(181, 83, 47); // 8 oxido
	inline const FColor RojoFaro(200, 64, 47); // 9 rojo_faro
	inline const FColor Laton(217, 164, 65); // 10 laton
	inline const FColor Cristal(159, 214, 224); // 11 cristal
	inline const FColor ArenaHumeda(168, 148, 106); // 12 arena_humeda
	inline const FColor VerdeSelva(44, 90, 49); // 13 verde_selva
	inline const FColor VerdeLima(191, 215, 126); // 14 verde_lima
	inline const FColor MaderaMedia(154, 107, 64); // 15 madera_media
	inline const FColor MaderaDeriva(165, 156, 138); // 16 madera_deriva
	inline const FColor Fibra(210, 181, 126); // 17 fibra
	inline const FColor RocaClara(171, 168, 158); // 18 roca_clara
	inline const FColor RocaOscura(84, 86, 90); // 19 roca_oscura
	inline const FColor AguaSomera(95, 196, 194); // 20 agua_somera
	inline const FColor AguaMedia(46, 143, 166); // 21 agua_media
	inline const FColor AguaProfunda(30, 87, 118); // 22 agua_profunda
	inline const FColor CieloDia(156, 203, 232); // 23 cielo_dia
	inline const FColor CieloOcaso(242, 180, 126); // 24 cielo_ocaso
	inline const FColor CieloNoche(42, 56, 88); // 25 cielo_noche
	inline const FColor Fuego(240, 140, 46); // 26 fuego
	inline const FColor Acero(183, 193, 200); // 27 acero
	inline const FColor Hierro(69, 71, 77); // 28 hierro
	inline const FColor Piel(214, 156, 120); // 29 piel
	inline const FColor Papel(246, 238, 219); // 30 papel
	inline const FColor Tinta(43, 39, 34); // 31 tinta
	inline const FColor BlancoNube(250, 247, 242); // 32 blanco_nube
	inline const FColor GrisLila(199, 195, 214); // 33 gris_lila
	inline const FColor CieloCenit(82, 136, 194); // 34 cielo_cenit
	inline const FColor HierbaSalvia(127, 160, 92); // 35 hierba_salvia
	inline const FColor HierbaSeca(169, 168, 106); // 36 hierba_seca
	inline const FColor Tierra(140, 113, 85); // 37 tierra
	inline const FColor MalvaOcaso(180, 138, 162); // 38 malva_ocaso
	inline const FColor GrisTormenta(90, 97, 108); // 39 gris_tormenta
	inline const FColor NocheCenit(24, 32, 58); // 40 noche_cenit

	/** Color lineal para Slate y Canvas, con la opacidad dada. */
	inline FLinearColor Linear(const FColor& Srgb, float Alpha = 1.0f)
	{
		FLinearColor Out = FLinearColor::FromSRGBColor(Srgb);
		Out.A = Alpha;
		return Out;
	}
}

/** Celdas de Content/Slate/T_IconosUI.png, en orden de lectura. */
enum class EExploredIcon : uint8
{
	Sed = 0,
	Hambre = 1,
	Energia = 2,
	Calor = 3,
	Cuchillo = 4,
	Cantimplora = 5,
	Barometro = 6,
	Coco = 7,
	CocoVacio = 8,
	Lena = 9,
	Fibra = 10,
	Cuerda = 11,
	CanaBambu = 12,
	CanaPescar = 13,
	Tronco = 14,
	Remo = 15,
	PezCrudo = 16,
	PezAsado = 17,
	TarroAceite = 18,
	Mecha = 19,
	Lente = 20,
	FlechaSube = 21,
	FlechaBaja = 22,
	Estable = 23,
	Sol = 24,
	Luna = 25,
	Cuaderno = 26,
	Faro = 27,
	Candado = 28,
	Pagina = 29,
	Anillo = 30,
	Count
};

namespace ExploredIconSheet
{
	constexpr int32 Columns = 8;
	constexpr int32 Rows = 4;
	constexpr int32 CellPixels = 128;

	/** Id de items.json (o de necesidad) a icono; Count si no tiene. */
	inline EExploredIcon FromId(FName Id)
	{
		if (Id == FName(TEXT("sed"))) { return EExploredIcon::Sed; }
		if (Id == FName(TEXT("hambre"))) { return EExploredIcon::Hambre; }
		if (Id == FName(TEXT("energia"))) { return EExploredIcon::Energia; }
		if (Id == FName(TEXT("calor"))) { return EExploredIcon::Calor; }
		if (Id == FName(TEXT("cuchillo"))) { return EExploredIcon::Cuchillo; }
		if (Id == FName(TEXT("cantimplora"))) { return EExploredIcon::Cantimplora; }
		if (Id == FName(TEXT("barometro"))) { return EExploredIcon::Barometro; }
		if (Id == FName(TEXT("coco"))) { return EExploredIcon::Coco; }
		if (Id == FName(TEXT("coco_vacio"))) { return EExploredIcon::CocoVacio; }
		if (Id == FName(TEXT("lena"))) { return EExploredIcon::Lena; }
		if (Id == FName(TEXT("fibra"))) { return EExploredIcon::Fibra; }
		if (Id == FName(TEXT("cuerda"))) { return EExploredIcon::Cuerda; }
		if (Id == FName(TEXT("cana_bambu"))) { return EExploredIcon::CanaBambu; }
		if (Id == FName(TEXT("cana_pescar"))) { return EExploredIcon::CanaPescar; }
		if (Id == FName(TEXT("tronco"))) { return EExploredIcon::Tronco; }
		if (Id == FName(TEXT("remo"))) { return EExploredIcon::Remo; }
		if (Id == FName(TEXT("pez_crudo"))) { return EExploredIcon::PezCrudo; }
		if (Id == FName(TEXT("pez_asado"))) { return EExploredIcon::PezAsado; }
		if (Id == FName(TEXT("tarro_aceite"))) { return EExploredIcon::TarroAceite; }
		if (Id == FName(TEXT("mecha"))) { return EExploredIcon::Mecha; }
		if (Id == FName(TEXT("lente"))) { return EExploredIcon::Lente; }
		if (Id == FName(TEXT("flecha_sube"))) { return EExploredIcon::FlechaSube; }
		if (Id == FName(TEXT("flecha_baja"))) { return EExploredIcon::FlechaBaja; }
		if (Id == FName(TEXT("estable"))) { return EExploredIcon::Estable; }
		if (Id == FName(TEXT("sol"))) { return EExploredIcon::Sol; }
		if (Id == FName(TEXT("luna"))) { return EExploredIcon::Luna; }
		if (Id == FName(TEXT("cuaderno"))) { return EExploredIcon::Cuaderno; }
		if (Id == FName(TEXT("faro"))) { return EExploredIcon::Faro; }
		if (Id == FName(TEXT("candado"))) { return EExploredIcon::Candado; }
		if (Id == FName(TEXT("pagina"))) { return EExploredIcon::Pagina; }
		if (Id == FName(TEXT("anillo"))) { return EExploredIcon::Anillo; }
		return EExploredIcon::Count;
	}

	/** Región UV (u0, v0, u1, v1) de la celda del icono. */
	inline FBox2f UVRegion(EExploredIcon Icon)
	{
		const int32 Index = FMath::Clamp(static_cast<int32>(Icon), 0, Columns * Rows - 1);
		const float U = static_cast<float>(Index % Columns) / Columns;
		const float V = static_cast<float>(Index / Columns) / Rows;
		return FBox2f(FVector2f(U, V), FVector2f(U + 1.0f / Columns, V + 1.0f / Rows));
	}
}
