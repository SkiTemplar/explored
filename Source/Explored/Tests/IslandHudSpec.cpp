#include "Misc/AutomationTest.h"

#include "UI/IslandHudModel.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FIslandHudSpec, "Explored.IslandHud",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FIslandHudSpec)

void FIslandHudSpec::Define()
{
	Describe(TEXT("textos"), [this]()
	{
		It(TEXT("reloj «Día N · HH:MM» truncado al minuto"), [this]()
		{
			TestEqual(TEXT("15:40"), FIslandHudModel::ClockText(2, 15.0f + 40.5f / 60.0f), FString(TEXT("Día 2 · 15:40")));
			TestEqual(TEXT("07:00"), FIslandHudModel::ClockText(1, 7.0f), FString(TEXT("Día 1 · 07:00")));
			TestEqual(TEXT("24 h vuelve a 00:00"), FIslandHudModel::ClockText(3, 24.0f), FString(TEXT("Día 3 · 00:00")));
			TestEqual(TEXT("NaN"), FIslandHudModel::ClockText(0, std::numeric_limits<float>::quiet_NaN()), FString(TEXT("Día 1 · 00:00")));
			TestEqual(TEXT("Cartel"), FIslandHudModel::DayCardText(3), FString(TEXT("Día 3")));
		});

		It(TEXT("barómetro con flecha de tendencia"), [this]()
		{
			TestEqual(TEXT("Baja"), FIslandHudModel::BarometerText(1007.6f, EBarometerTrend::Falling), FString(TEXT("1008 hPa ↓")));
			TestEqual(TEXT("Sube"), FIslandHudModel::BarometerText(995.0f, EBarometerTrend::Rising), FString(TEXT("995 hPa ↑")));
			TestEqual(TEXT("Estable"), FIslandHudModel::BarometerText(1014.0f, EBarometerTrend::Steady), FString(TEXT("1014 hPa →")));
		});

		It(TEXT("aviso «E · Verbo objeto», vacío si falta algo"), [this]()
		{
			TestEqual(TEXT("Aviso"), FIslandHudModel::PromptText(TEXT("E"), TEXT("Beber coco")), FString(TEXT("E · Beber coco")));
			TestTrue(TEXT("Sin verbo"), FIslandHudModel::PromptText(TEXT("E"), FString()).IsEmpty());
			TestTrue(TEXT("Sin tecla"), FIslandHudModel::PromptText(FString(), TEXT("Dormir")).IsEmpty());
		});
	});

	Describe(TEXT("barras, ranuras y progreso"), [this]()
	{
		It(TEXT("una barra parpadea solo bajo el umbral"), [this]()
		{
			TestTrue(TEXT("Sana siempre visible"), FIslandHudModel::IsBarVisible(30.0f, 25.0f, 0.3));
			TestTrue(TEXT("Baja, fase visible"), FIslandHudModel::IsBarVisible(10.0f, 25.0f, 0.1));
			TestFalse(TEXT("Baja, fase oculta"), FIslandHudModel::IsBarVisible(10.0f, 25.0f, 0.3));
		});

		It(TEXT("tres ranuras con las primeras pilas y la elegida resaltada"), [this]()
		{
			FBackpackState Pack;
			Pack.Stacks.Add({ FName(TEXT("cuchillo")), 1 });
			Pack.Stacks.Add({ FName(TEXT("coco")), 3 });
			const TArray<FHudQuickSlot> Slots = FIslandHudModel::QuickSlots(Pack, 1);
			TestEqual(TEXT("Tres"), Slots.Num(), 3);
			TestEqual(TEXT("Coco"), Slots[1].Count, 3);
			TestTrue(TEXT("Elegida"), Slots[1].bSelected && !Slots[0].bSelected);
			TestTrue(TEXT("Tercera vacía"), Slots[2].IsEmpty());
			const TArray<FHudQuickSlot> None = FIslandHudModel::QuickSlots(Pack, 7);
			TestFalse(TEXT("Fuera de rango, ninguna"), None[0].bSelected || None[1].bSelected || None[2].bSelected);
		});

		It(TEXT("en la mano va la carga pesada o la ranura elegida"), [this]()
		{
			FBackpackState Pack;
			Pack.Stacks.Add({ FName(TEXT("cuchillo")), 1 });
			Pack.Stacks.Add({ FName(TEXT("coco")), 3 });
			TestEqual(TEXT("Ranura 2"), FIslandHudModel::HeldItem(Pack, 1), FName(TEXT("coco")));
			TestTrue(TEXT("Ranura vacía, nada"), FIslandHudModel::HeldItem(Pack, 2).IsNone());
			TestTrue(TEXT("Fuera de rango, nada"), FIslandHudModel::HeldItem(Pack, 9).IsNone());
			Pack.HeldHeavy = FName(TEXT("lente"));
			TestEqual(TEXT("La lente manda"), FIslandHudModel::HeldItem(Pack, 0), FName(TEXT("lente")));
		});

		It(TEXT("encoge lo que no cabe en la mano y no agranda lo pequeño"), [this]()
		{
			TestEqual(TEXT("Tronco de 250 cm a 50"), FIslandHudModel::HeldItemScale(250.0f, 50.0f), 0.2f);
			TestEqual(TEXT("Cuchillo intacto"), FIslandHudModel::HeldItemScale(28.0f, 50.0f), 1.0f);
			TestEqual(TEXT("Sin tamaño, 1"), FIslandHudModel::HeldItemScale(0.0f, 50.0f), 1.0f);
		});

		It(TEXT("progreso de 0 a 1 y 0 si el total no vale"), [this]()
		{
			TestEqual(TEXT("Mitad"), FIslandHudModel::Progress01(10.0f, 20.0f), 0.5f);
			TestEqual(TEXT("Tope"), FIslandHudModel::Progress01(30.0f, 20.0f), 1.0f);
			TestEqual(TEXT("Total 0"), FIslandHudModel::Progress01(1.0f, 0.0f), 0.0f);
		});
	});
}

#endif
