#include "Misc/AutomationTest.h"

#include "WorldGen/IslandDecorModel.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace IslandDecorTest
{
	FIslandDecor Sample()
	{
		FIslandDecor Decor;
		Decor.Meshes = { TEXT("/Game/Art/K1_Naturaleza/SM_GrassClump_01"), TEXT("/Game/Art/K1_Naturaleza/SM_Shrub_01") };
		for (int32 I = 0; I < 5; ++I)
		{
			FIslandDecorInstance Inst;
			Inst.Mesh = I % 2;
			Inst.Location = FVector(I * 3.0, -I * 2.0, 1.5);
			Inst.YawDeg = 30.0f * I;
			Inst.Scale = 1.0f;
			Decor.Instances.Add(Inst);
		}
		return Decor;
	}
}

BEGIN_DEFINE_SPEC(FIslandDecorSpec, "Explored.IslandDecor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FIslandDecorSpec)

void FIslandDecorSpec::Define()
{
	using namespace IslandDecorTest;

	It(TEXT("acepta datos válidos y cuenta las instancias por malla"), [this]()
	{
		const FIslandDecor Decor = Sample();
		FString Error;
		TestTrue(TEXT("válido"), Decor.Validate(Error));
		const TArray<int32> Counts = Decor.CountPerMesh();
		TestEqual(TEXT("mallas"), Counts.Num(), 2);
		TestEqual(TEXT("hierba"), Counts[0], 3);
		TestEqual(TEXT("arbusto"), Counts[1], 2);
	});

	It(TEXT("rechaza índices, escalas, rutas y coordenadas no válidas"), [this]()
	{
		FString Error;
		FIslandDecor Bad = Sample();
		Bad.Instances[2].Mesh = 5;
		TestFalse(TEXT("índice"), Bad.Validate(Error));
		Bad = Sample();
		Bad.Instances[0].Scale = 0.0f;
		TestFalse(TEXT("escala"), Bad.Validate(Error));
		Bad = Sample();
		Bad.Instances[1].Location.Z = std::numeric_limits<double>::quiet_NaN();
		TestFalse(TEXT("NaN"), Bad.Validate(Error));
		Bad = Sample();
		Bad.Meshes[1] = TEXT("/Engine/BasicShapes/Cube");
		TestFalse(TEXT("ruta"), Bad.Validate(Error));
		Bad = Sample();
		Bad.Meshes.Reset();
		TestFalse(TEXT("sin mallas"), Bad.Validate(Error));
		Bad = Sample();
		Bad.Instances.SetNum(FIslandDecor::MaxInstances + 1);
		TestFalse(TEXT("demasiadas"), Bad.Validate(Error));
	});
}

#endif
