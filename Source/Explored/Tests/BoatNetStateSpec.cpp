#include "Misc/AutomationTest.h"

#include "Boats/BoatNetState.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BoatNetStateSpecDetail
{
	FBoatState Typical()
	{
		FBoatState S;
		S.LocationCm = FVector(123456.4, -98765.6, -12.3);
		S.VelocityCmS = FVector2D(254.0, -31.0);
		S.YawDeg = 237.123f;
		S.RollDeg = -7.4f;
		S.bSailRaised = true;
		S.WaterInHullKg = 42.37f;
		S.HullDamage01 = 0.25f;
		S.Condition = EBoatCondition::Swamped;
		return S;
	}

	FExploredBoatNetState RoundTrip(const FExploredBoatNetState& In)
	{
		FExploredBoatNetState Out;
		const TArray<uint8> Bytes = In.ToBytes();
		FExploredBoatNetState::FromBytes(Bytes.GetData(), Bytes.Num(), Out);
		return Out;
	}
}

BEGIN_DEFINE_SPEC(FBoatNetStateSpec, "Explored.Boats.NetState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FBoatNetStateSpec)

void FBoatNetStateSpec::Define()
{
	using namespace BoatNetStateSpecDetail;

	Describe("Presupuesto", [this]()
	{
		It("ocupa 19 B y a 20 Hz cuesta 380 B/s (biblia 08 §2.5)", [this]()
		{
			TestEqual(TEXT("19 B"), FExploredBoatNetState::SizeBytes, 19);
			TestEqual(TEXT("Serializa 19 B"), FExploredBoatNetState().ToBytes().Num(), 19);
			TestEqual(TEXT("20 Hz"), FExploredBoatNetState::RateHz, 20.0f);
			TestEqual(TEXT("380 B/s"), FExploredBoatNetState::BytesPerSecond(), 380.0f);
			TestTrue(TEXT("Menos de 3,1 kbps por barco ocupado"), FExploredBoatNetState::BytesPerSecond() * 8.0f / 1000.0f < 3.1f);
		});
	});

	Describe("Ida y vuelta", [this]()
	{
		It("un estado normal vuelve igual por el cable y con el error de medio paso", [this]()
		{
			const FBoatState S = Typical();
			const FExploredBoatNetState N = FExploredBoatNetState::Quantize(S, 0.62f, 3);
			const FExploredBoatNetState Back = RoundTrip(N);
			TestTrue(TEXT("Mismos bytes de vuelta"), Back == N);
			const FBoatNetSnapshot D = Back.Dequantize();
			TestEqual(TEXT("X a 1 cm"), D.LocationCm.X, S.LocationCm.X, 0.5);
			TestEqual(TEXT("Y a 1 cm"), D.LocationCm.Y, S.LocationCm.Y, 0.5);
			TestEqual(TEXT("Z a 1 cm"), D.LocationCm.Z, S.LocationCm.Z, 0.5);
			TestEqual(TEXT("Velocidad X a 10 cm/s"), D.VelocityCmS.X, S.VelocityCmS.X, 5.0);
			TestEqual(TEXT("Velocidad Y a 10 cm/s"), D.VelocityCmS.Y, S.VelocityCmS.Y, 5.0);
			TestEqual(TEXT("Rumbo a 0,0055°"), D.YawDeg, S.YawDeg, 0.0028f);
			TestEqual(TEXT("Escora al grado"), D.RollDeg, S.RollDeg, 0.5f);
			TestTrue(TEXT("Vela izada"), D.bSailRaised);
			TestEqual(TEXT("Escota"), D.SailTrim01, 0.62f, 0.5f / 127.0f);
			TestEqual(TEXT("Agua a 0,1 kg"), D.WaterInHullKg, S.WaterInHullKg, 0.05f);
			TestEqual(TEXT("Daño a 1/255"), D.HullDamage01, S.HullDamage01, 0.5f / 255.0f);
			TestEqual(TEXT("Brechas"), D.Breaches, 3);
			TestTrue(TEXT("Anegado"), D.Condition == EBoatCondition::Swamped);
		});

		It("las cuatro condiciones y la vela arriada viajan", [this]()
		{
			for (int32 C = 0; C < 4; ++C)
			{
				FBoatState S;
				S.Condition = static_cast<EBoatCondition>(C);
				S.bSailRaised = false;
				const FBoatNetSnapshot D = RoundTrip(FExploredBoatNetState::Quantize(S, 0.0f, 0)).Dequantize();
				TestTrue(TEXT("Condición"), D.Condition == S.Condition);
				TestFalse(TEXT("Arriada"), D.bSailRaised);
			}
		});

		It("ApplyTo copia al cliente lo que viaja y deja lo demás", [this]()
		{
			const FExploredBoatNetState N = FExploredBoatNetState::Quantize(Typical(), 0.5f, 0);
			FBoatState Client;
			Client.bCrewAboard = true;
			Client.CargoKg = 33.0f;
			N.ApplyTo(Client);
			TestEqual(TEXT("Posición"), Client.LocationCm.X, 123456.0, 0.01);
			TestEqual(TEXT("Rumbo"), Client.YawDeg, 237.123f, 0.003f);
			TestTrue(TEXT("Estado"), Client.Condition == EBoatCondition::Swamped);
			TestTrue(TEXT("Tripulante sin tocar"), Client.bCrewAboard);
			TestEqual(TEXT("Carga sin tocar"), Client.CargoKg, 33.0f);
		});

		It("los bytes son little-endian y no dependen de la máquina", [this]()
		{
			FExploredBoatNetState N;
			N.PosX = 1;
			N.PosY = -1;
			N.PosZ = 0;
			N.VelX = 0x1234;
			N.Heading = 0xABCD;
			N.WaterDeciKg = 0x0102;
			uint8 Raw[FExploredBoatNetState::SizeBytes];
			N.ToBytes(Raw);
			TestEqual(TEXT("X en el bit 0"), Raw[0], static_cast<uint8>(0x01));
			// Y = -1 en 21 bits ocupa los bits 21–41: byte 2 bits 5–7, bytes 3–4 y byte 5 bits 0–1.
			TestEqual(TEXT("Y byte 2"), Raw[2], static_cast<uint8>(0xE0));
			TestEqual(TEXT("Y byte 3"), Raw[3], static_cast<uint8>(0xFF));
			TestEqual(TEXT("Y byte 4"), Raw[4], static_cast<uint8>(0xFF));
			TestEqual(TEXT("Y byte 5"), Raw[5], static_cast<uint8>(0x03));
			TestEqual(TEXT("Velocidad X baja"), Raw[7], static_cast<uint8>(0x34));
			TestEqual(TEXT("Velocidad X alta"), Raw[8], static_cast<uint8>(0x12));
			TestEqual(TEXT("Rumbo bajo"), Raw[11], static_cast<uint8>(0xCD));
			TestEqual(TEXT("Rumbo alto"), Raw[12], static_cast<uint8>(0xAB));
			TestEqual(TEXT("Agua baja"), Raw[15], static_cast<uint8>(0x02));
			TestEqual(TEXT("Integridad intacta por defecto"), Raw[17], static_cast<uint8>(0xFF));
		});
	});

	Describe("Extremos del rango", [this]()
	{
		It("la posición llega exacta a los extremos y se recorta al pasarlos", [this]()
		{
			FBoatState S;
			S.LocationCm = FVector(FExploredBoatNetState::MaxPositionXYCm, FExploredBoatNetState::MinPositionXYCm, FExploredBoatNetState::MaxPositionZCm);
			FBoatNetSnapshot D = RoundTrip(FExploredBoatNetState::Quantize(S, 0.0f, 0)).Dequantize();
			TestEqual(TEXT("X máxima"), D.LocationCm.X, static_cast<double>(FExploredBoatNetState::MaxPositionXYCm));
			TestEqual(TEXT("Y mínima"), D.LocationCm.Y, static_cast<double>(FExploredBoatNetState::MinPositionXYCm));
			TestEqual(TEXT("Z máxima"), D.LocationCm.Z, static_cast<double>(FExploredBoatNetState::MaxPositionZCm));
			TestTrue(TEXT("El archipiélago cabe (±3 km)"), FExploredBoatNetState::MaxPositionXYCm > 300000);
			TestTrue(TEXT("El naufragio en el fondo cabe (-70 m)"), FExploredBoatNetState::MinPositionZCm < -7000);

			S.LocationCm = FVector(1.0e9, -1.0e9, -1.0e9);
			D = RoundTrip(FExploredBoatNetState::Quantize(S, 0.0f, 0)).Dequantize();
			TestEqual(TEXT("X recortada"), D.LocationCm.X, static_cast<double>(FExploredBoatNetState::MaxPositionXYCm));
			TestEqual(TEXT("Y recortada"), D.LocationCm.Y, static_cast<double>(FExploredBoatNetState::MinPositionXYCm));
			TestEqual(TEXT("Z recortada"), D.LocationCm.Z, static_cast<double>(FExploredBoatNetState::MinPositionZCm));
		});

		It("velocidad, escora, agua, escota, daño y brechas se recortan a su campo", [this]()
		{
			FBoatState S;
			S.VelocityCmS = FVector2D(1.0e7, -1.0e7);
			S.RollDeg = 200.0f;
			S.WaterInHullKg = 1.0e7f;
			S.HullDamage01 = -3.0f;
			FBoatNetSnapshot D = RoundTrip(FExploredBoatNetState::Quantize(S, 5.0f, 1000)).Dequantize();
			TestEqual(TEXT("Velocidad máxima"), D.VelocityCmS.X, 327670.0);
			TestEqual(TEXT("Velocidad mínima simétrica"), D.VelocityCmS.Y, -327670.0);
			TestEqual(TEXT("Escora +127"), D.RollDeg, 127.0f);
			TestEqual(TEXT("Agua 6553,5 kg"), D.WaterInHullKg, 6553.5f, 1e-3f);
			TestEqual(TEXT("Daño negativo es intacto"), D.HullDamage01, 0.0f);
			TestEqual(TEXT("Escota largada del todo"), D.SailTrim01, 1.0f);
			TestEqual(TEXT("63 brechas como mucho"), D.Breaches, FExploredBoatNetState::MaxBreaches);

			S.RollDeg = -200.0f;
			S.WaterInHullKg = -5.0f;
			S.HullDamage01 = 7.0f;
			D = RoundTrip(FExploredBoatNetState::Quantize(S, -1.0f, -4)).Dequantize();
			TestEqual(TEXT("Escora -127"), D.RollDeg, -127.0f);
			TestEqual(TEXT("Sin agua negativa"), D.WaterInHullKg, 0.0f);
			TestEqual(TEXT("Destrozado"), D.HullDamage01, 1.0f);
			TestEqual(TEXT("Escota cazada"), D.SailTrim01, 0.0f);
			TestEqual(TEXT("Sin brechas negativas"), D.Breaches, 0);
		});

		It("el rumbo se envuelve: 359,999° es 0°, -90° es 270° y 720,5° es 0,5°", [this]()
		{
			auto Yaw = [](float Deg)
			{
				FBoatState S;
				S.YawDeg = Deg;
				return RoundTrip(FExploredBoatNetState::Quantize(S, 0.0f, 0)).Dequantize().YawDeg;
			};
			TestEqual(TEXT("359,999°"), Yaw(359.999f), 0.0f);
			TestEqual(TEXT("-90°"), Yaw(-90.0f), 270.0f, 0.003f);
			TestEqual(TEXT("720,5°"), Yaw(720.5f), 0.5f, 0.003f);
			TestEqual(TEXT("Último paso"), Yaw(360.0f - 360.0f / 65536.0f), 360.0f - 360.0f / 65536.0f, 1e-3f);
		});

		It("NaN e infinitos viajan como cero", [this]()
		{
			const float NaN = NAN;
			FBoatState S;
			S.LocationCm = FVector(NAN, INFINITY, -INFINITY);
			S.VelocityCmS = FVector2D(NAN, INFINITY);
			S.YawDeg = NaN;
			S.RollDeg = NaN;
			S.WaterInHullKg = INFINITY;
			S.HullDamage01 = NaN;
			const FBoatNetSnapshot D = RoundTrip(FExploredBoatNetState::Quantize(S, NaN, 0)).Dequantize();
			TestTrue(TEXT("Posición en el origen"), D.LocationCm.IsZero());
			TestTrue(TEXT("Parado"), D.VelocityCmS.IsZero());
			TestEqual(TEXT("Rumbo 0"), D.YawDeg, 0.0f);
			TestEqual(TEXT("Adrizado"), D.RollDeg, 0.0f);
			TestEqual(TEXT("Sin agua"), D.WaterInHullKg, 0.0f);
			TestEqual(TEXT("Intacto"), D.HullDamage01, 0.0f);
			TestEqual(TEXT("Escota cazada"), D.SailTrim01, 0.0f);
		});
	});

	Describe("Paquetes rotos", [this]()
	{
		It("rechaza tamaños distintos de 19 B y no toca la salida", [this]()
		{
			const TArray<uint8> Good = FExploredBoatNetState::Quantize(Typical(), 0.5f, 1).ToBytes();
			FExploredBoatNetState Out;
			Out.PosX = 77;
			TestFalse(TEXT("Nulo"), FExploredBoatNetState::FromBytes(nullptr, 19, Out));
			TestFalse(TEXT("Corto"), FExploredBoatNetState::FromBytes(Good.GetData(), 18, Out));
			TestFalse(TEXT("Largo"), FExploredBoatNetState::FromBytes(Good.GetData(), 20, Out));
			TestFalse(TEXT("Negativo"), FExploredBoatNetState::FromBytes(Good.GetData(), -1, Out));
			TestEqual(TEXT("Sin tocar"), Out.PosX, 77);
		});

		It("cualquier patrón de 19 B se decodifica a valores dentro de rango", [this]()
		{
			const uint8 Patterns[] = {0x00, 0xFF, 0x80, 0x7F, 0xAA};
			for (const uint8 Fill : Patterns)
			{
				uint8 Raw[FExploredBoatNetState::SizeBytes];
				for (uint8& B : Raw)
				{
					B = Fill;
				}
				FExploredBoatNetState N;
				TestTrue(TEXT("Se lee"), FExploredBoatNetState::FromBytes(Raw, FExploredBoatNetState::SizeBytes, N));
				const FBoatNetSnapshot D = N.Dequantize();
				TestTrue(TEXT("X en rango"), D.LocationCm.X >= FExploredBoatNetState::MinPositionXYCm && D.LocationCm.X <= FExploredBoatNetState::MaxPositionXYCm);
				TestTrue(TEXT("Z en rango"), D.LocationCm.Z >= FExploredBoatNetState::MinPositionZCm && D.LocationCm.Z <= FExploredBoatNetState::MaxPositionZCm);
				TestTrue(TEXT("Escora en ±127"), FMath::Abs(D.RollDeg) <= 127.0f);
				TestTrue(TEXT("Rumbo en [0, 360)"), D.YawDeg >= 0.0f && D.YawDeg < 360.0f);
				TestTrue(TEXT("Escota en [0, 1]"), D.SailTrim01 >= 0.0f && D.SailTrim01 <= 1.0f);
				TestTrue(TEXT("Daño en [0, 1]"), D.HullDamage01 >= 0.0f && D.HullDamage01 <= 1.0f);
				TestTrue(TEXT("Condición válida"), static_cast<int32>(D.Condition) <= static_cast<int32>(EBoatCondition::Wrecked));
				// Lo que se lee se vuelve a escribir igual salvo lo recortado (escora -128 y velocidad -32768).
				const FExploredBoatNetState Again = RoundTrip(N);
				TestTrue(TEXT("Estable al reenviar"), Again == N);
			}
		});

		It("cuantizar lo decuantizado no cambia nada (sin deriva al reenviar)", [this]()
		{
			const FExploredBoatNetState N = FExploredBoatNetState::Quantize(Typical(), 0.33f, 5);
			FBoatState S;
			N.ApplyTo(S);
			const FBoatNetSnapshot D = N.Dequantize();
			const FExploredBoatNetState Again = FExploredBoatNetState::Quantize(S, D.SailTrim01, D.Breaches);
			TestTrue(TEXT("Idéntico"), Again == N);
		});
	});
}

#endif
