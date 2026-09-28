#include "Misc/AutomationTest.h"

#include "Boats/BoatNetStateModel.h"
#include "Core/ExploredRandom.h"
#include "Core/NetQuantize.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FBoatNetStateModelSpec, "Explored.Net.Boat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FBoatNetStateModelSpec)

void FBoatNetStateModelSpec::Define()
{
	const float NaN = std::numeric_limits<float>::quiet_NaN();

	Describe("Estado de 19 bytes", [this, NaN]()
	{
		It("mide 19 B y cuesta 3,0 kbps por barco ocupado", [this]()
		{
			TArray<uint8> Bytes;
			FBoatNetStateModel::EncodeState(FBoatNetState(), Bytes);
			TestEqual(TEXT("19 B"), Bytes.Num(), 19);
			TestEqual(TEXT("3,04 kbps"), FBoatNetStateModel::KbpsPerBoat(), 3.04, 1e-9);
		});

		It("va y vuelve con el error de cada campo", [this]()
		{
			FBoatState Boat;
			Boat.LocationCm = FVector(254321.3, -120045.8, -35.2);
			Boat.VelocityCmS = FVector2D(412.3, -87.6);
			Boat.YawDeg = 211.7f;
			Boat.RollDeg = -14.4f;
			Boat.bSailRaised = true;
			Boat.WaterInHullKg = 37.26f;
			Boat.HullDamage01 = 0.2f;
			const FBoatNetState In = FBoatNetStateModel::FromBoatState(Boat, 0.63f);
			TArray<uint8> Bytes;
			FBoatNetStateModel::EncodeState(In, Bytes);
			FBoatNetState Out;
			TestTrue(TEXT("Decodifica"), FBoatNetStateModel::DecodeState(Bytes, Out));
			TestEqual(TEXT("Posición"), Out.PositionCm, In.PositionCm, 5.0f);
			TestEqual(TEXT("Velocidad a 5 cm/s"), Out.VelocityCmS, In.VelocityCmS, 5.0f);
			TestEqual(TEXT("Rumbo a 0,003°"), ExploredNet::AngleDeltaDeg(In.YawDeg, Out.YawDeg), 0.0f, 0.003f);
			TestEqual(TEXT("Escora al grado"), Out.HeelDeg, -14.0f);
			TestTrue(TEXT("Vela izada"), Out.bSailRaised);
			TestEqual(TEXT("Trimado"), Out.SailTrim01, 0.63f, 0.5f / 127.0f + 1e-6f);
			TestEqual(TEXT("Agua a 0,05 kg"), Out.SwampWaterKg, 37.3f, 0.051f);
			TestEqual(TEXT("Integridad"), Out.HullIntegrity01, 0.8f, 0.5f / 255.0f + 1e-6f);
		});

		It("acota lo que se sale y envuelve la escora", [this, NaN]()
		{
			FBoatNetState S;
			S.VelocityCmS = FVector2D(1e7, -1e7);
			S.HeelDeg = 350.0f;
			S.SwampWaterKg = 1e9f;
			S.HullIntegrity01 = -2.0f;
			S.SailTrim01 = NaN;
			S.YawDeg = NaN;
			const FBoatNetState Q = FBoatNetStateModel::Quantize(S);
			TestEqual(TEXT("Velocidad máxima"), Q.VelocityCmS.X, 327670.0, 1e-6);
			TestEqual(TEXT("Velocidad mínima simétrica"), Q.VelocityCmS.Y, -327670.0, 1e-6);
			TestEqual(TEXT("350° de escora son −10°"), Q.HeelDeg, -10.0f);
			TestEqual(TEXT("Agua acotada"), Q.SwampWaterKg, 6553.5f, 0.01f);
			TestEqual(TEXT("Integridad no negativa"), Q.HullIntegrity01, 0.0f);
			TestEqual(TEXT("Trimado NaN → 0"), Q.SailTrim01, 0.0f);
			TestEqual(TEXT("Rumbo NaN → 0"), Q.YawDeg, 0.0f);
		});

		It("rechaza tamaño, byte reservado y valores fuera del rango simétrico", [this]()
		{
			TArray<uint8> Good;
			FBoatNetStateModel::EncodeState(FBoatNetState(), Good);
			FBoatNetState Out;
			TArray<uint8> Bad = Good;
			Bad[18] = 1;
			TestFalse(TEXT("Reservado"), FBoatNetStateModel::DecodeState(Bad, Out));
			Bad = Good;
			Bad[13] = 0x80;
			TestFalse(TEXT("Escora −128"), FBoatNetStateModel::DecodeState(Bad, Out));
			Bad = Good;
			Bad[7] = 0x00;
			Bad[8] = 0x80;
			TestFalse(TEXT("Velocidad −32 768"), FBoatNetStateModel::DecodeState(Bad, Out));
			Bad = Good;
			Bad.Pop();
			TestFalse(TEXT("18 bytes"), FBoatNetStateModel::DecodeState(Bad, Out));
		});

		It("todo estado que acepta vuelve a codificarse igual (50 000 al azar)", [this]()
		{
			FExploredRandom Rng(19);
			for (int32 i = 0; i < 50000; ++i)
			{
				TArray<uint8> Bytes;
				for (int32 b = 0; b < FBoatNetStateModel::StateBytes; ++b)
				{
					Bytes.Add(static_cast<uint8>(Rng.NextUInt32() & 0xFF));
				}
				Bytes[18] = 0;
				FBoatNetState S;
				if (!FBoatNetStateModel::DecodeState(Bytes, S))
				{
					continue;
				}
				TArray<uint8> Again;
				FBoatNetStateModel::EncodeState(S, Again);
				if (Again != Bytes)
				{
					AddError(FString::Printf(TEXT("El estado %d no es canónico"), i));
					return;
				}
			}
		});
	});

	Describe("Mandos de 4 bytes", [this]()
	{
		It("van y vuelven: escota, timón, banderas y palada", [this]()
		{
			FBoatNetControls In;
			In.Controls.SailTrim01 = 0.25f;
			In.Controls.Rudder = -0.5f;
			In.Controls.bAutoTrim = false;
			In.Controls.bBailing = true;
			In.Stroke = EBoatNetStroke::Starboard;
			TArray<uint8> Bytes;
			FBoatNetStateModel::EncodeControls(In, Bytes);
			TestEqual(TEXT("4 B"), Bytes.Num(), 4);
			FBoatNetControls Out;
			TestTrue(TEXT("Decodifica"), FBoatNetStateModel::DecodeControls(Bytes, Out));
			TestEqual(TEXT("Escota"), Out.Controls.SailTrim01, 0.25f, 0.5f / 255.0f);
			TestEqual(TEXT("Timón"), Out.Controls.Rudder, -0.5f, 0.5f / 127.0f);
			TestFalse(TEXT("Sin autotrimado"), Out.Controls.bAutoTrim);
			TestTrue(TEXT("Achicando"), Out.Controls.bBailing);
			TestTrue(TEXT("Palada a estribor"), Out.Stroke == EBoatNetStroke::Starboard);

			In.Controls.Rudder = 5.0f;
			FBoatNetStateModel::EncodeControls(In, Bytes);
			FBoatNetStateModel::DecodeControls(Bytes, Out);
			TestEqual(TEXT("Timón acotado a 1"), Out.Controls.Rudder, 1.0f);
		});

		It("el servidor descarta mandos manipulados", [this]()
		{
			TArray<uint8> Good;
			FBoatNetStateModel::EncodeControls(FBoatNetControls(), Good);
			FBoatNetControls Out;
			TArray<uint8> Bad = Good;
			Bad[1] = 0x80;
			TestFalse(TEXT("Timón −128"), FBoatNetStateModel::DecodeControls(Bad, Out));
			Bad = Good;
			Bad[2] = 0x04;
			TestFalse(TEXT("Bandera desconocida"), FBoatNetStateModel::DecodeControls(Bad, Out));
			Bad = Good;
			Bad[3] = 3;
			TestFalse(TEXT("Palada inexistente"), FBoatNetStateModel::DecodeControls(Bad, Out));
			Bad = Good;
			Bad.Add(0);
			TestFalse(TEXT("5 bytes"), FBoatNetStateModel::DecodeControls(Bad, Out));
		});
	});

	Describe("Corrección en el cliente", [this, NaN]()
	{
		It("llega al estado recibido en 200 ms", [this, NaN]()
		{
			double Local = 0.0;
			float Elapsed = 0.0f;
			const float Dt = 1.0f / 60.0f;
			while (Elapsed < FBoatNetStateModel::CorrectionSeconds)
			{
				Local += (50.0 - Local) * FBoatNetStateModel::CorrectionAlpha(Elapsed, Dt);
				Elapsed += Dt;
			}
			TestEqual(TEXT("En el objetivo"), Local, 50.0, 1e-4);
			TestEqual(TEXT("Delta NaN"), FBoatNetStateModel::CorrectionAlpha(0.0f, NaN), 0.0f);
			TestEqual(TEXT("Estado viejo"), FBoatNetStateModel::CorrectionAlpha(1.0f, Dt), 1.0f);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
