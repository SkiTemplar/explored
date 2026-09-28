#include "Misc/AutomationTest.h"

#include "Core/ExploredRandom.h"
#include "Sky/WorldClockNetModel.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FWorldClockNetModelSpec, "Explored.Net.WorldClock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FWorldClockNetModelSpec)

namespace WorldClockNetSpecDetail
{
	FWorldClockNetState Sample()
	{
		FWorldClockNetState S;
		S.Day = 37;
		S.Hours = 18.4567f;
		S.TimeScale = 1.0f;
		S.Season = ESeason::Cyclones;
		S.Weather = EWeatherState::Cyclone;
		S.Wind01 = 0.83f;
		S.WindFromDeg = 247.0f;
		S.Rain01 = 0.61f;
		S.SeaState01 = 0.92f;
		S.StormIntensity01 = 0.7f;
		S.CycloneCategory = 4;
		return S;
	}
}

void FWorldClockNetModelSpec::Define()
{
	using namespace WorldClockNetSpecDetail;
	const float NaN = std::numeric_limits<float>::quiet_NaN();

	Describe("Formato de 11 bytes", [this, NaN]()
	{
		It("mide exactamente 11 bytes, lo que cuenta el presupuesto de 08 §2.8", [this]()
		{
			TArray<uint8> Bytes = {9, 9, 9};
			FWorldClockNetModel::Encode(Sample(), Bytes);
			TestEqual(TEXT("11 B (sustituye lo que hubiera)"), Bytes.Num(), FWorldClockNetModel::PacketBytes);
			TestEqual(TEXT("La constante es 11"), FWorldClockNetModel::PacketBytes, 11);
		});

		It("va y vuelve con el error de cuantización de cada campo", [this]()
		{
			const FWorldClockNetState In = Sample();
			TArray<uint8> Bytes;
			FWorldClockNetModel::Encode(In, Bytes);
			FWorldClockNetState Out;
			TestTrue(TEXT("Decodifica"), FWorldClockNetModel::Decode(Bytes, Out));
			TestEqual(TEXT("Día"), Out.Day, In.Day);
			TestEqual(TEXT("Hora a 0,0005 h (1,8 s)"), Out.Hours, In.Hours, 0.0005f + 1e-5f);
			TestEqual(TEXT("TimeScale"), Out.TimeScale, 1.0f);
			TestTrue(TEXT("Estación"), Out.Season == ESeason::Cyclones);
			TestTrue(TEXT("Tiempo"), Out.Weather == EWeatherState::Cyclone);
			TestEqual(TEXT("Viento"), Out.Wind01, In.Wind01, 0.5f / 255.0f + 1e-6f);
			TestEqual(TEXT("Dirección del viento a 0,7°"), Out.WindFromDeg, In.WindFromDeg, 360.0f / 512.0f + 1e-4f);
			TestEqual(TEXT("Lluvia"), Out.Rain01, In.Rain01, 0.5f / 255.0f + 1e-6f);
			TestEqual(TEXT("Mar"), Out.SeaState01, In.SeaState01, 0.5f / 255.0f + 1e-6f);
			TestEqual(TEXT("Tormenta a 1/62"), Out.StormIntensity01, In.StormIntensity01, 0.5f / 31.0f + 1e-6f);
			TestEqual(TEXT("Categoría"), Out.CycloneCategory, 4);
		});

		It("lleva todas las estaciones, todos los tiempos y todas las categorías", [this]()
		{
			for (int32 Season = 0; Season < static_cast<int32>(ESeason::Count); ++Season)
			{
				for (int32 Weather = 0; Weather < static_cast<int32>(EWeatherState::Count); ++Weather)
				{
					for (int32 Category = 0; Category <= FWorldClockNetModel::MaxCycloneCategory; ++Category)
					{
						FWorldClockNetState S = Sample();
						S.Season = static_cast<ESeason>(Season);
						S.Weather = static_cast<EWeatherState>(Weather);
						S.CycloneCategory = Category;
						const FWorldClockNetState Q = FWorldClockNetModel::Quantize(S);
						if (Q.Season != S.Season || Q.Weather != S.Weather || Q.CycloneCategory != Category)
						{
							AddError(FString::Printf(TEXT("Estación %d, tiempo %d, categoría %d no vuelven"), Season, Weather, Category));
						}
					}
				}
			}
		});

		It("lleva el ×120 de dormir en grupo y acota lo que no cabe", [this]()
		{
			FWorldClockNetState S = Sample();
			S.TimeScale = 120.0f;
			TestEqual(TEXT("×120"), FWorldClockNetModel::Quantize(S).TimeScale, 120.0f);
			S.TimeScale = 1000.0f;
			TestEqual(TEXT("Tope 127,5"), FWorldClockNetModel::Quantize(S).TimeScale, FWorldClockNetModel::MaxTimeScale);
			S.TimeScale = -1.0f;
			TestEqual(TEXT("Nunca negativo"), FWorldClockNetModel::Quantize(S).TimeScale, 0.0f);
			S.CycloneCategory = 9;
			TestEqual(TEXT("Categoría acotada a 5"), FWorldClockNetModel::Quantize(S).CycloneCategory, 5);
			S.Day = 70000;
			TestEqual(TEXT("Día acotado a 65535"), FWorldClockNetModel::Quantize(S).Day, 65535);
			S.Day = -3;
			TestEqual(TEXT("Día negativo → 0"), FWorldClockNetModel::Quantize(S).Day, 0);
		});

		It("pasa de 23,9996 h a la medianoche del día siguiente, nunca a un 24:00", [this]()
		{
			FWorldClockNetState S = Sample();
			S.Day = 12;
			S.Hours = 23.9996f;
			const FWorldClockNetState Q = FWorldClockNetModel::Quantize(S);
			TestEqual(TEXT("Día 13"), Q.Day, 13);
			TestEqual(TEXT("00:00"), Q.Hours, 0.0f);
			S.Hours = 24.5f;
			TestEqual(TEXT("24,5 h envuelve a 0,5 h"), FWorldClockNetModel::Quantize(S).Hours, 0.5f, 1e-4f);
			S.Hours = -1.0f;
			TestEqual(TEXT("−1 h envuelve a 23 h"), FWorldClockNetModel::Quantize(S).Hours, 23.0f, 1e-4f);
		});

		It("convierte un estado con NaN en un paquete válido", [this, NaN]()
		{
			FWorldClockNetState S = Sample();
			S.Hours = NaN;
			S.Wind01 = NaN;
			S.WindFromDeg = NaN;
			S.TimeScale = NaN;
			S.StormIntensity01 = NaN;
			TArray<uint8> Bytes;
			FWorldClockNetModel::Encode(S, Bytes);
			FWorldClockNetState Out;
			TestTrue(TEXT("Decodifica"), FWorldClockNetModel::Decode(Bytes, Out));
			TestEqual(TEXT("Hora 0"), Out.Hours, 0.0f);
			TestEqual(TEXT("Escala 0"), Out.TimeScale, 0.0f);
		});
	});

	Describe("Paquetes corruptos", [this]()
	{
		It("rechaza tamaños distintos de 11 sin tocar el estado", [this]()
		{
			TArray<uint8> Bytes;
			FWorldClockNetModel::Encode(Sample(), Bytes);
			const FWorldClockNetState Before = FWorldClockNetModel::Quantize(FWorldClockNetState());
			for (int32 Size : {0, 1, 10, 12, 64})
			{
				TArray<uint8> Wrong = Bytes;
				Wrong.SetNumZeroed(Size);
				FWorldClockNetState Out = Before;
				TestFalse(FString::Printf(TEXT("%d bytes"), Size), FWorldClockNetModel::Decode(Wrong, Out));
				TestEqual(TEXT("Estado intacto"), Out.Day, Before.Day);
			}
		});

		It("rechaza hora imposible, tiempo inexistente, categoría 6–7 y bits reservados", [this]()
		{
			TArray<uint8> Good;
			FWorldClockNetModel::Encode(Sample(), Good);
			FWorldClockNetState Out;
			TArray<uint8> Bad = Good;
			Bad[2] = 0xC0; Bad[3] = 0x5D; // 24 000
			TestFalse(TEXT("24:00"), FWorldClockNetModel::Decode(Bad, Out));
			Bad = Good;
			Bad[5] = static_cast<uint8>((Good[5] & 0x03) | (9 << 2));
			TestFalse(TEXT("Tiempo 9"), FWorldClockNetModel::Decode(Bad, Out));
			Bad = Good;
			Bad[5] = static_cast<uint8>(Good[5] | 0x40);
			TestFalse(TEXT("Bit reservado 6"), FWorldClockNetModel::Decode(Bad, Out));
			Bad = Good;
			Bad[10] = static_cast<uint8>((Good[10] & 0x1F) | (6 << 5));
			TestFalse(TEXT("Categoría 6"), FWorldClockNetModel::Decode(Bad, Out));
		});

		It("todo paquete que acepta vuelve a codificarse igual byte a byte (100 000 al azar)", [this]()
		{
			FExploredRandom Rng(0x11B);
			int32 Accepted = 0;
			for (int32 i = 0; i < 100000; ++i)
			{
				TArray<uint8> Bytes;
				for (int32 b = 0; b < FWorldClockNetModel::PacketBytes; ++b)
				{
					Bytes.Add(static_cast<uint8>(Rng.NextUInt32() & 0xFF));
				}
				FWorldClockNetState S;
				if (!FWorldClockNetModel::Decode(Bytes, S))
				{
					continue;
				}
				++Accepted;
				TArray<uint8> Again;
				FWorldClockNetModel::Encode(S, Again);
				if (Again != Bytes)
				{
					AddError(FString::Printf(TEXT("El paquete %d no es canónico"), i));
					return;
				}
			}
			TestTrue(TEXT("Se aceptan bastantes para que la prueba diga algo"), Accepted > 1000);
		});
	});

	Describe("Cuándo se manda", [this, NaN]()
	{
		It("cada 5 s, o ya si cambia TimeScale, estación, tiempo o tormenta", [this, NaN]()
		{
			const FWorldClockNetState A = Sample();
			FWorldClockNetState B = A;
			B.Hours += 0.01f;
			B.Wind01 = 0.1f;
			TestFalse(TEXT("Hora y viento esperan a los 5 s"), FWorldClockNetModel::ShouldSend(A, B, 2.0));
			TestTrue(TEXT("A los 5 s sí"), FWorldClockNetModel::ShouldSend(A, B, 5.0));
			TestTrue(TEXT("Nunca enviado"), FWorldClockNetModel::ShouldSend(A, B, -1.0));
			TestTrue(TEXT("Reloj de red NaN"), FWorldClockNetModel::ShouldSend(A, B, NaN));

			B = A;
			B.TimeScale = 120.0f;
			TestTrue(TEXT("Dormir en grupo"), FWorldClockNetModel::ShouldSend(A, B, 0.1));
			B = A;
			B.Weather = EWeatherState::Gale;
			TestTrue(TEXT("Cambio de tiempo"), FWorldClockNetModel::ShouldSend(A, B, 0.1));
			B = A;
			B.Season = ESeason::Dry;
			TestTrue(TEXT("Cambio de estación"), FWorldClockNetModel::ShouldSend(A, B, 0.1));
			B = A;
			B.CycloneCategory = 5;
			TestTrue(TEXT("Sube el ciclón"), FWorldClockNetModel::ShouldSend(A, B, 0.1));
			B = A;
			B.StormIntensity01 = A.StormIntensity01 + 0.001f;
			TestFalse(TEXT("Ruido por debajo de un paso no dispara"), FWorldClockNetModel::ShouldSend(A, B, 0.1));
		});
	});

	Describe("Corrección del reloj del cliente", [this, NaN]()
	{
		It("corrige suave entre 0,95 y 1,05 y salta por encima de 6 minutos de juego", [this, NaN]()
		{
			FWorldClockCorrection C = FWorldClockNetModel::Correct(100.0, 100.0);
			TestFalse(TEXT("En hora: sin salto"), C.bHardSnap);
			TestEqual(TEXT("En hora: ×1"), C.LocalRateFactor, 1.0f);

			C = FWorldClockNetModel::Correct(100.0, 100.05);
			TestFalse(TEXT("3 min atrasado: suave"), C.bHardSnap);
			TestEqual(TEXT("Acelera un 2,5 %"), C.LocalRateFactor, 1.025f, 1e-5f);

			C = FWorldClockNetModel::Correct(100.0, 99.9);
			TestFalse(TEXT("6 min adelantado justo: todavía suave"), C.bHardSnap);
			TestEqual(TEXT("Frena al 0,95"), C.LocalRateFactor, 0.95f, 1e-5f);

			C = FWorldClockNetModel::Correct(100.0, 100.11);
			TestTrue(TEXT("6,6 min: salto duro"), C.bHardSnap);
			TestEqual(TEXT("Con salto no hay multiplicador"), C.LocalRateFactor, 1.0f);

			TestTrue(TEXT("Reloj local NaN: salto"), FWorldClockNetModel::Correct(NaN, 100.0).bHardSnap);
		});

		It("mide el error entre días con las horas totales, no con la hora del día", [this]()
		{
			FWorldClockNetState Server;
			Server.Day = 9;
			Server.Hours = 0.02f;
			FWorldClockNetState Client;
			Client.Day = 8;
			Client.Hours = 23.98f;
			const FWorldClockCorrection C = FWorldClockNetModel::Correct(Client.TotalHours(), Server.TotalHours());
			TestFalse(TEXT("2,4 min a través de la medianoche: suave"), C.bHardSnap);
			TestEqual(TEXT("Error +0,04 h"), C.ErrorHours, 0.04, 1e-4);
		});

		It("cierra el error sin saltos con el multiplicador que devuelve", [this]()
		{
			// Servidor a ×1; el cliente corrige cada paso de 0,01 h de juego.
			double Server = 200.0;
			double Client = 199.92;
			for (int32 Step = 0; Step < 2000; ++Step)
			{
				const FWorldClockCorrection C = FWorldClockNetModel::Correct(Client, Server);
				if (C.bHardSnap)
				{
					AddError(TEXT("No debería saltar"));
					return;
				}
				Server += 0.01;
				Client += 0.01 * C.LocalRateFactor;
			}
			TestEqual(TEXT("Error cerrado"), Server - Client, 0.0, 1e-4);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
