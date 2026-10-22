#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"

#include "ExploredStyleCommandlet.generated.h"

/**
 * Validador de estilo en Unreal (GDD v3 §11, entregable de F2): la otra mitad de `check-style`
 * (Tools/Style, en Blender). Recorre los manifiestos de Art/Export/K* y comprueba cada malla
 * importada en /Game/Art/<kit>:
 *
 * - existe;
 * - triángulos de la LOD 0 <= `tope_tris` del manifiesto (presupuesto del GDD por tipo);
 * - un solo hueco de material, con un material de la familia M_Toon (M_Toon, M_ToonCesped…);
 * - una sola LOD en el césped (K6), que adelgaza en el material y no por LOD.
 *
 * Además avisa de las mallas de /Game/Art/K* que no están en ningún manifiesto.
 *
 * Uso: UnrealEditor-Cmd Explored.uproject -run=ExploredStyle -unattended -nopause
 * Devuelve 1 si hay algún fallo; el detalle va al log con el prefijo [Estilo].
 */
UCLASS()
class UExploredStyleCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UExploredStyleCommandlet();

	virtual int32 Main(const FString& Params) override;
};
