#include "Misc/AutomationTest.h"

#include "Core/ExploredRandom.h"
#include "Core/NetQuantize.h"
#include "Fauna/FaunaAnchorNetModel.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FFaunaAnchorNetModelSpec, "Explored.Net.FaunaAnchor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FFaunaAnchorNetModelSpec)

void FFaunaAnchorNetModelSpec::Define()
{
	const double NaN = std::numeric_limits<double>::quiet_NaN();

	Describe("Formato de 10 bytes", [this]()
	{
		It("mide 10 bytes por grupo y va y vuelve", [this]()
		{
			FFaunaGroupAnchor A;
			A.GroupId = 0xBEEF;
			A.CentroidCm = FVector(123456.7, -98765.4, -1234.5);
			A.BrainState = 7;
			TArray<uint8> Bytes;
			FFaunaAnchorNetModel::Append(A, Bytes);
			TestEqual(TEXT("10 B"), Bytes.Num(), FFaunaAnchorNetModel::PacketBytes);
			TestEqual(TEXT("La constante es 10"), FFaunaAnchorNetModel::PacketBytes, 10);
			TArray<FFaunaGroupAnchor> Out;
			TestTrue(TEXT("Decodifica"), FFaunaAnchorNetModel::DecodeBatch(Bytes, Out));
			TestEqual(TEXT("Uno"), Out.Num(), 1);
			if (Out.Num() == 1)
			{
				TestTrue(TEXT("Igual a lo cuantizado"), Out[0] == FFaunaAnchorNetModel::Quantize(A));
				TestEqual(TEXT("Centroide a menos de un paso"), Out[0].CentroidCm, A.CentroidCm, 5.0f);
			}
		});

		It("codifica un lote de 24 en 240 B y lo lee entero", [this]()
		{
			TArray<FFaunaGroupAnchor> Anchors;
			FExploredRandom Rng(7);
			for (int32 i = 0; i < FFaunaAnchorNetModel::MaxGroupsPerClient; ++i)
			{
				FFaunaGroupAnchor A;
				A.GroupId = static_cast<uint16>(1000 + i);
				A.CentroidCm = ExploredNet::QuantizePosition7(FVector(Rng.RangeFloat(-3e5f, 3e5f), Rng.RangeFloat(-3e5f, 3e5f), Rng.RangeFloat(-5000.0f, 3000.0f)));
				A.BrainState = static_cast<uint8>(i % 14);
				Anchors.Add(A);
			}
			TArray<uint8> Bytes;
			TestTrue(TEXT("Codifica"), FFaunaAnchorNetModel::EncodeBatch(Anchors, Bytes));
			TestEqual(TEXT("240 B"), Bytes.Num(), 240);
			TArray<FFaunaGroupAnchor> Out;
			TestTrue(TEXT("Decodifica"), FFaunaAnchorNetModel::DecodeBatch(Bytes, Out));
			TestTrue(TEXT("Idéntico"), Out == Anchors);
		});

		It("rechaza lotes de más de 24, ids repetidos y tamaños que no son múltiplo de 10", [this]()
		{
			TArray<FFaunaGroupAnchor> Anchors;
			Anchors.SetNum(25);
			for (int32 i = 0; i < Anchors.Num(); ++i)
			{
				Anchors[i].GroupId = static_cast<uint16>(i);
			}
			TArray<uint8> Bytes;
			TestFalse(TEXT("25 grupos"), FFaunaAnchorNetModel::EncodeBatch(Anchors, Bytes));
			TestEqual(TEXT("Sin bytes"), Bytes.Num(), 0);

			Anchors.SetNum(2);
			Anchors[1].GroupId = Anchors[0].GroupId;
			TestFalse(TEXT("Id repetido al codificar"), FFaunaAnchorNetModel::EncodeBatch(Anchors, Bytes));

			TArray<uint8> Twice;
			FFaunaAnchorNetModel::Append(Anchors[0], Twice);
			FFaunaAnchorNetModel::Append(Anchors[0], Twice);
			TArray<FFaunaGroupAnchor> Out;
			TestFalse(TEXT("Id repetido al leer"), FFaunaAnchorNetModel::DecodeBatch(Twice, Out));
			TestEqual(TEXT("Todo o nada"), Out.Num(), 0);

			Twice.SetNum(19);
			TestFalse(TEXT("19 bytes"), FFaunaAnchorNetModel::DecodeBatch(Twice, Out));
			TArray<uint8> Big;
			Big.SetNumZeroed(250);
			TestFalse(TEXT("25 × 10 bytes"), FFaunaAnchorNetModel::DecodeBatch(Big, Out));
			TArray<uint8> Empty;
			TestTrue(TEXT("Lote vacío: válido, sin grupos"), FFaunaAnchorNetModel::DecodeBatch(Empty, Out) && Out.Num() == 0);
		});
	});

	Describe("Lotes al azar", [this]()
	{
		It("todo lote que acepta vuelve a codificarse igual byte a byte", [this]()
		{
			FExploredRandom Rng(0xA2C);
			int32 Accepted = 0;
			for (int32 i = 0; i < 20000; ++i)
			{
				TArray<uint8> Bytes;
				const int32 Count = Rng.RangeInt(0, 3);
				for (int32 b = 0; b < Count * FFaunaAnchorNetModel::PacketBytes; ++b)
				{
					Bytes.Add(static_cast<uint8>(Rng.NextUInt32() & 0xFF));
				}
				TArray<FFaunaGroupAnchor> Anchors;
				if (!FFaunaAnchorNetModel::DecodeBatch(Bytes, Anchors))
				{
					continue;
				}
				++Accepted;
				TArray<uint8> Again;
				if (!FFaunaAnchorNetModel::EncodeBatch(Anchors, Again) || Again != Bytes)
				{
					AddError(FString::Printf(TEXT("El lote %d no es canónico"), i));
					return;
				}
			}
			TestTrue(TEXT("Acepta bastantes"), Accepted > 10000);
		});
	});

	Describe("Relevancia y calendario", [this, NaN]()
	{
		It("elige como mucho 24 grupos dentro de 150 m, del más cercano al más lejano", [this, NaN]()
		{
			TArray<FFaunaGroupAnchor> Groups;
			for (int32 i = 0; i < 40; ++i)
			{
				FFaunaGroupAnchor A;
				A.GroupId = static_cast<uint16>(500 - i);
				A.CentroidCm = FVector(500.0 * i, 0.0, 0.0);
				Groups.Add(A);
			}
			Groups[3].CentroidCm = FVector(NaN, 0.0, 0.0);
			TArray<int32> Chosen;
			FFaunaAnchorNetModel::SelectForClient(Groups, FVector::ZeroVector, Chosen);
			TestEqual(TEXT("Tope de 24"), Chosen.Num(), 24);
			TestFalse(TEXT("El del centroide NaN no entra"), Chosen.Contains(3));
			TestEqual(TEXT("El más cercano primero"), Chosen[0], 0);
			TestEqual(TEXT("El último es el 24 (se saltó el 3)"), Chosen.Last(), 24);

			FFaunaAnchorNetModel::SelectForClient(Groups, FVector(0.0, 20000.0, 0.0), Chosen);
			TestEqual(TEXT("A 200 m no hay ninguno"), Chosen.Num(), 0);
			FFaunaAnchorNetModel::SelectForClient(Groups, FVector(NaN, 0.0, 0.0), Chosen);
			TestEqual(TEXT("Receptor NaN: ninguno"), Chosen.Num(), 0);
		});

		It("a igual distancia desempata por id y nunca repite un id", [this]()
		{
			TArray<FFaunaGroupAnchor> Groups;
			for (uint16 Id : {9, 3, 3, 5})
			{
				FFaunaGroupAnchor A;
				A.GroupId = Id;
				A.CentroidCm = FVector(100.0, 0.0, 0.0);
				Groups.Add(A);
			}
			TArray<int32> Chosen;
			FFaunaAnchorNetModel::SelectForClient(Groups, FVector::ZeroVector, Chosen);
			TestTrue(TEXT("3, 5, 9"), Chosen == TArray<int32>({1, 3, 0}));
		});

		It("manda cada grupo cada 2 s, repartidos y no todos en el mismo tick", [this]()
		{
			const double Tick = 1.0 / 30.0;
			int32 MaxInOneTick = 0;
			TArray<int32> Sent;
			Sent.SetNumZeroed(24);
			for (int32 Step = 0; Step < 300; ++Step)
			{
				const double Prev = Step * Tick;
				const double Now = (Step + 1) * Tick;
				int32 ThisTick = 0;
				for (int32 Id = 0; Id < 24; ++Id)
				{
					if (FFaunaAnchorNetModel::IsDue(static_cast<uint16>(Id), Prev, Now))
					{
						++Sent[Id];
						++ThisTick;
					}
				}
				MaxInOneTick = FMath::Max(MaxInOneTick, ThisTick);
			}
			for (int32 Id = 0; Id < 24; ++Id)
			{
				TestEqual(FString::Printf(TEXT("Grupo %d: 5 envíos en 10 s"), Id), Sent[Id], 5);
			}
			TestTrue(TEXT("Nunca más de 3 en el mismo tick"), MaxInOneTick <= 3);
			TestFalse(TEXT("Intervalo vacío"), FFaunaAnchorNetModel::IsDue(1, 5.0, 5.0));
			TestFalse(TEXT("Reloj hacia atrás"), FFaunaAnchorNetModel::IsDue(1, 5.0, 1.0));
		});
	});

	Describe("Arrastre en el cliente", [this]()
	{
		It("cierra la distancia al ancla justo en 1 s con cualquier paso", [this]()
		{
			for (float Dt : {1.0f / 60.0f, 1.0f / 144.0f, 0.1f, 0.37f})
			{
				double Local = 0.0;
				const double Anchor = 800.0;
				float Elapsed = 0.0f;
				int32 Steps = 0;
				while (Elapsed < FFaunaAnchorNetModel::PullSeconds && Steps < 1000)
				{
					Local += (Anchor - Local) * FFaunaAnchorNetModel::PullAlpha(Elapsed, Dt);
					Elapsed += Dt;
					++Steps;
					if (Elapsed < FFaunaAnchorNetModel::PullSeconds - 2.0f * Dt && Local >= Anchor)
					{
						AddError(TEXT("Llega antes de tiempo: sería un salto"));
					}
				}
				TestEqual(FString::Printf(TEXT("Paso %.4f s: en el ancla al segundo"), Dt), Local, Anchor, 1e-3);
			}
		});

		It("con deltas raros no mueve nada o termina, nunca pasa de 1", [this]()
		{
			TestEqual(TEXT("Delta 0"), FFaunaAnchorNetModel::PullAlpha(0.2f, 0.0f), 0.0f);
			TestEqual(TEXT("Delta negativo"), FFaunaAnchorNetModel::PullAlpha(0.2f, -1.0f), 0.0f);
			TestEqual(TEXT("Delta NaN"), FFaunaAnchorNetModel::PullAlpha(0.2f, std::numeric_limits<float>::quiet_NaN()), 0.0f);
			TestEqual(TEXT("Ancla vieja"), FFaunaAnchorNetModel::PullAlpha(5.0f, 0.016f), 1.0f);
			TestEqual(TEXT("Tiempo NaN: termina"), FFaunaAnchorNetModel::PullAlpha(std::numeric_limits<float>::quiet_NaN(), 0.016f), 1.0f);
			TestEqual(TEXT("Paso de 0,5 s al empezar: la mitad"), FFaunaAnchorNetModel::PullAlpha(0.0f, 0.5f), 0.5f);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
