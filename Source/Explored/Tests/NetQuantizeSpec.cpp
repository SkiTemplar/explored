#include "Misc/AutomationTest.h"

#include "Core/ExploredRandom.h"
#include "Core/NetQuantize.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FNetQuantizeSpec, "Explored.Net.Quantize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FNetQuantizeSpec)

void FNetQuantizeSpec::Define()
{
	using namespace ExploredNet;
	const double NaN = std::numeric_limits<double>::quiet_NaN();
	const double Inf = std::numeric_limits<double>::infinity();

	Describe("Bytes", [this]()
	{
		It("escribe en little-endian y lee lo mismo", [this]()
		{
			TArray<uint8> Bytes;
			FByteWriter W(Bytes);
			W.U16(0x1234);
			W.U32(0xA1B2C3D4u);
			W.I16(-2);
			W.I8(-128);
			TestEqual(TEXT("9 bytes"), Bytes.Num(), 9);
			TestEqual(TEXT("Primero el byte bajo"), static_cast<int32>(Bytes[0]), 0x34);
			FByteReader R(Bytes);
			TestEqual(TEXT("U16"), static_cast<int32>(R.U16()), 0x1234);
			TestTrue(TEXT("U32"), R.U32() == 0xA1B2C3D4u);
			TestEqual(TEXT("I16"), static_cast<int32>(R.I16()), -2);
			TestEqual(TEXT("I8"), static_cast<int32>(R.I8()), -128);
			TestTrue(TEXT("Leído entero"), R.IsDone());
		});

		It("leer de más marca el lector como roto y devuelve ceros", [this]()
		{
			TArray<uint8> Bytes = {1};
			FByteReader R(Bytes);
			TestEqual(TEXT("U16 sobre 1 byte"), static_cast<int32>(R.U16()), 1);
			TestFalse(TEXT("Roto"), R.bOk);
			TestEqual(TEXT("Sigue dando cero"), static_cast<int32>(R.U8()), 0);
			TestFalse(TEXT("No está completo"), R.IsDone());
		});
	});

	Describe("Escalares", [this, NaN, Inf]()
	{
		It("cuantiza 0–1 a un byte, acotado y a salvo de NaN", [this, NaN, Inf]()
		{
			TestEqual(TEXT("0"), static_cast<int32>(Quantize01(0.0f)), 0);
			TestEqual(TEXT("1"), static_cast<int32>(Quantize01(1.0f)), 255);
			TestEqual(TEXT("0,5"), static_cast<int32>(Quantize01(0.5f)), 128);
			TestEqual(TEXT("Negativo"), static_cast<int32>(Quantize01(-3.0f)), 0);
			TestEqual(TEXT("Más de 1"), static_cast<int32>(Quantize01(7.0f)), 255);
			TestEqual(TEXT("NaN"), static_cast<int32>(Quantize01(static_cast<float>(NaN))), 0);
			TestEqual(TEXT("+inf"), static_cast<int32>(Quantize01(static_cast<float>(Inf))), 0);
			for (int32 V = 0; V < 256; ++V)
			{
				if (Quantize01(Dequantize01(static_cast<uint8>(V))) != V)
				{
					AddError(FString::Printf(TEXT("Byte %d no vuelve igual"), V));
				}
			}
		});

		It("acota antes de convertir: nada de conversiones fuera de rango", [this, NaN, Inf]()
		{
			TestTrue(TEXT("1e300"), RoundClamped(1e300, -5, 5) == 5);
			TestTrue(TEXT("-1e300"), RoundClamped(-1e300, -5, 5) == -5);
			TestTrue(TEXT("tope INT64_MAX"), RoundClamped(1e300, 0, TNumericLimits<int64>::Max()) == TNumericLimits<int64>::Max());
			TestTrue(TEXT("tope INT64_MIN"), RoundClamped(-1e300, TNumericLimits<int64>::Min(), 0) == TNumericLimits<int64>::Min());
			TestTrue(TEXT("NaN → 0 dentro del rango"), RoundClamped(NaN, -5, 5) == 0);
			TestTrue(TEXT("NaN → Lo si 0 no cabe"), RoundClamped(NaN, 3, 5) == 3);
			TestTrue(TEXT("-inf"), RoundClamped(-Inf, 0, 9) == 0);
			TestTrue(TEXT("Redondeo a la mitad hacia arriba"), RoundClamped(2.5, 0, 9) == 3);
		});

		It("envuelve los ángulos en 8 y 16 bits", [this, NaN]()
		{
			TestEqual(TEXT("0°"), static_cast<int32>(QuantizeAngle8(0.0f)), 0);
			TestEqual(TEXT("360° = 0°"), static_cast<int32>(QuantizeAngle8(360.0f)), 0);
			TestEqual(TEXT("−90° = 270°"), static_cast<int32>(QuantizeAngle8(-90.0f)), 192);
			TestEqual(TEXT("359,9° redondea a 0, no a 256"), static_cast<int32>(QuantizeAngle8(359.9f)), 0);
			TestEqual(TEXT("720° + 45°"), static_cast<int32>(QuantizeAngle16(765.0f)), 8192);
			TestEqual(TEXT("NaN"), static_cast<int32>(QuantizeAngle16(static_cast<float>(NaN))), 0);
			TestEqual(TEXT("Paso de 16 bits"), DequantizeAngle16(1), 360.0f / 65536.0f, 1e-7f);
			for (int32 V = 0; V < 65536; V += 7)
			{
				if (QuantizeAngle16(DequantizeAngle16(static_cast<uint16>(V))) != V)
				{
					AddError(FString::Printf(TEXT("Ángulo %d no vuelve igual"), V));
					break;
				}
			}
		});

		It("da la diferencia angular más corta", [this]()
		{
			TestEqual(TEXT("350 → 10"), AngleDeltaDeg(350.0f, 10.0f), 20.0f, 1e-4f);
			TestEqual(TEXT("10 → 350"), AngleDeltaDeg(10.0f, 350.0f), -20.0f, 1e-4f);
			TestEqual(TEXT("Media vuelta"), AngleDeltaDeg(0.0f, 180.0f), 180.0f, 1e-4f);
			TestEqual(TEXT("−180 cuenta como 180"), AngleDeltaDeg(180.0f, 0.0f), 180.0f, 1e-4f);
		});
	});

	Describe("Posición en 7 bytes", [this, NaN, Inf]()
	{
		It("ocupa 7 bytes y vuelve con el error de medio paso", [this]()
		{
			FExploredRandom Rng(20260928);
			for (int32 i = 0; i < 5000; ++i)
			{
				const FVector P(Rng.RangeFloat(-640000.0f, 640000.0f), Rng.RangeFloat(-640000.0f, 640000.0f), Rng.RangeFloat(-20000.0f, 20000.0f));
				TArray<uint8> Bytes;
				FByteWriter W(Bytes);
				WritePosition7(W, P);
				if (Bytes.Num() != PositionBytes)
				{
					AddError(TEXT("No mide 7 bytes"));
					return;
				}
				FByteReader R(Bytes);
				const FVector Q = ReadPosition7(R);
				if (FMath::Abs(Q.X - P.X) > PositionXYStepCm * 0.5 + 1e-6 || FMath::Abs(Q.Y - P.Y) > PositionXYStepCm * 0.5 + 1e-6
					|| FMath::Abs(Q.Z - P.Z) > PositionZStepCm * 0.5 + 1e-6)
				{
					AddError(FString::Printf(TEXT("Error de más en (%f, %f, %f)"), P.X, P.Y, P.Z));
					return;
				}
				// Idempotente: cuantizar lo cuantizado no lo mueve.
				if (QuantizePosition7(Q) != Q)
				{
					AddError(TEXT("Cuantizar dos veces cambia el punto"));
					return;
				}
			}
		});

		It("conserva el signo en los extremos y acota lo que se sale", [this, NaN, Inf]()
		{
			TestEqual(TEXT("Máximo"), QuantizePosition7(FVector(PositionXYMaxCm, -PositionXYMaxCm, PositionZMaxCm)), FVector(PositionXYMaxCm, -PositionXYMaxCm, PositionZMaxCm));
			TestEqual(TEXT("Fuera de rango"), QuantizePosition7(FVector(1e12, -1e12, -1e9)), FVector(PositionXYMaxCm, -PositionXYMaxCm, -PositionZMaxCm));
			TestEqual(TEXT("NaN e inf"), QuantizePosition7(FVector(NaN, Inf, -Inf)), FVector(0.0, 0.0, 0.0));
			TestEqual(TEXT("−1 paso"), QuantizePosition7(FVector(-PositionXYStepCm, -PositionXYStepCm, -PositionZStepCm)), FVector(-PositionXYStepCm, -PositionXYStepCm, -PositionZStepCm));
			TestTrue(TEXT("Cubre de sobra el archipiélago de 6,4 km"), PositionXYMaxCm > 640000.0);

			// El mínimo de complemento a dos (−2^21 en X) no sale nunca de WritePosition7.
			TArray<uint8> Bytes = {0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00};
			FByteReader R(Bytes);
			ReadPosition7(R);
			TestFalse(TEXT("X = −2^21 se rechaza"), R.bOk);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
