#include "Misc/AutomationTest.h"

#include "Cartography/CartographyModel.h"
#include "Ruins/RuinsModel.h"
#include "UI/MapViewLogic.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FExploredMapViewLogicSpec, "Explored.UI.MapViewLogic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FExploredMapViewLogicSpec)

namespace MapViewLogicSpecDetail
{
	bool InsideSheet(const ExploredMapView::FMapView& View)
	{
		const double Half = View.Size * 0.5;
		return View.Center.X - Half >= -1.e-9 && View.Center.Y - Half >= -1.e-9
			&& View.Center.X + Half <= 1.0 + 1.e-9 && View.Center.Y + Half <= 1.0 + 1.e-9;
	}

	FMapMark Mark(double X, double Y)
	{
		FMapMark M;
		M.StampId = FName(TEXT("water"));
		M.Position = FVector2D(X, Y);
		return M;
	}
}

void FExploredMapViewLogicSpec::Define()
{
	using namespace ExploredMapView;
	using namespace MapViewLogicSpecDetail;

	Describe("zona visible", [this]()
	{
		It("recorta el lado y mantiene la zona dentro de la hoja", [this]()
		{
			FMapView View;
			View.Size = 5.0;
			View.Center = FVector2D(3.0, -2.0);
			const FMapView Whole = ClampView(View);
			TestEqual(TEXT("Como mucho la hoja entera"), Whole.Size, MaxViewSize);
			TestTrue(TEXT("Centrada"), Whole.Center.Equals(FVector2D(0.5, 0.5), 1.e-12));

			View.Size = 0.001;
			View.Center = FVector2D(0.0, 1.0);
			const FMapView Close = ClampView(View);
			TestEqual(TEXT("Como poco el lado mínimo"), Close.Size, MinViewSize);
			TestTrue(TEXT("En la esquina, sin salirse"), InsideSheet(Close));
			TestTrue(TEXT("Pegada a la esquina"), Close.Center.Equals(FVector2D(MinViewSize * 0.5, 1.0 - MinViewSize * 0.5), 1.e-12));
		});

		It("valores no finitos vuelven a la hoja entera", [this]()
		{
			FMapView View;
			View.Size = std::numeric_limits<double>::quiet_NaN();
			View.Center = FVector2D(std::numeric_limits<double>::infinity(), 0.2);
			const FMapView Out = ClampView(View);
			TestEqual(TEXT("Lado"), Out.Size, MaxViewSize);
			TestTrue(TEXT("Centro"), Out.Center.Equals(FVector2D(0.5, 0.5), 1.e-12));
		});

		It("acercar con la rueda deja quieto el punto bajo el cursor", [this]()
		{
			FMapView View;
			const FVector2D Anchor(0.3, 0.6);
			const FVector2D LocalSize(800.0, 600.0);
			const FSheetTransform Before = FSheetTransform::Make(View, LocalSize);
			const FMapView Zoomed = ZoomAt(View, 2.0, Anchor);
			TestEqual(TEXT("Mitad de lado"), Zoomed.Size, 0.5, 1.e-12);
			const FSheetTransform After = FSheetTransform::Make(Zoomed, LocalSize);
			TestTrue(TEXT("El ancla no se mueve en pantalla"), Before.MapToLocal(Anchor).Equals(After.MapToLocal(Anchor), 1.e-6));
			TestTrue(TEXT("Dentro de la hoja"), InsideSheet(Zoomed));
		});

		It("alejar al máximo vuelve a la hoja entera y un factor inválido no cambia nada", [this]()
		{
			FMapView View;
			View.Size = 0.2;
			View.Center = FVector2D(0.2, 0.2);
			const FMapView Out = ZoomAt(View, 0.01, FVector2D(0.1, 0.1));
			TestEqual(TEXT("Hoja entera"), Out.Size, MaxViewSize);
			TestTrue(TEXT("Centrada"), Out.Center.Equals(FVector2D(0.5, 0.5), 1.e-12));
			const FMapView Same = ZoomAt(View, 0.0, FVector2D(0.1, 0.1));
			TestEqual(TEXT("Factor cero"), Same.Size, 0.2, 1.e-12);
			const FMapView Nan = ZoomAt(View, std::numeric_limits<double>::quiet_NaN(), FVector2D(0.1, 0.1));
			TestEqual(TEXT("Factor NaN"), Nan.Size, 0.2, 1.e-12);
		});

		It("arrastrar mueve la hoja con el puntero y no se sale", [this]()
		{
			FMapView View;
			View.Size = 0.5;
			const FMapView Dragged = PanByPixels(View, FVector2D(100.0, 0.0), 400.0);
			TestEqual(TEXT("Arrastrar a la derecha enseña lo de la izquierda"), Dragged.Center.X, 0.5 - 0.125, 1.e-12);
			const FMapView Far = PanByPixels(View, FVector2D(-100000.0, -100000.0), 400.0);
			TestTrue(TEXT("Tope en el borde"), Far.Center.Equals(FVector2D(0.75, 0.75), 1.e-12));
			const FMapView Degenerate = PanByPixels(View, FVector2D(10.0, 10.0), 0.0);
			TestTrue(TEXT("Sin tamaño no se mueve"), Degenerate.Center.Equals(View.Center, 1.e-12));
		});

		It("las flechas mueven una fracción de la zona visible", [this]()
		{
			FMapView View;
			View.Size = 0.4;
			const FMapView Up = PanByFraction(View, FVector2D(0.0, -1.0));
			TestEqual(TEXT("Arriba"), Up.Center.Y, 0.5 - 0.4 * PanStepFraction, 1.e-12);
			const FMapView Big = PanByFraction(View, FVector2D(7.0, 0.0));
			TestEqual(TEXT("La dirección se limita a ±1"), Big.Center.X, 0.5 + 0.4 * PanStepFraction, 1.e-12);
		});
	});

	Describe("hoja ↔ pantalla", [this]()
	{
		It("la hoja ocupa el mayor cuadrado centrado y la ida y vuelta es exacta", [this]()
		{
			FMapView View;
			View.Size = 0.25;
			View.Center = FVector2D(0.4, 0.3);
			const FSheetTransform T = FSheetTransform::Make(View, FVector2D(1000.0, 600.0));
			TestEqual(TEXT("Lado"), T.Side, 600.0, 1.e-9);
			TestTrue(TEXT("Margen horizontal"), T.Offset.Equals(FVector2D(200.0, 0.0), 1.e-9));
			TestTrue(TEXT("Esquina de la zona visible"), T.MapToLocal(FVector2D(0.275, 0.175)).Equals(FVector2D(200.0, 0.0), 1.e-6));
			const FVector2D Local(512.0, 123.0);
			TestTrue(TEXT("Ida y vuelta"), T.MapToLocal(T.LocalToMap(Local)).Equals(Local, 1.e-6));
			TestTrue(TEXT("Dentro"), T.IsInsideSheet(FVector2D(500.0, 300.0)));
			TestFalse(TEXT("En el margen"), T.IsInsideSheet(FVector2D(100.0, 300.0)));
		});

		It("sin tamaño no divide entre cero", [this]()
		{
			const FSheetTransform T = FSheetTransform::Make(FMapView(), FVector2D(0.0, 0.0));
			const FVector2D Map = T.LocalToMap(FVector2D(10.0, 10.0));
			TestTrue(TEXT("Finito"), FMath::IsFinite(Map.X) && FMath::IsFinite(Map.Y));
			TestFalse(TEXT("Nada está dentro"), T.IsInsideSheet(FVector2D(0.0, 0.0)));
		});

		It("elige la marca más cercana dentro del radio", [this]()
		{
			TArray<FMapMark> Marks = { Mark(0.5, 0.5), Mark(0.52, 0.5), Mark(0.9, 0.9) };
			const FSheetTransform T = FSheetTransform::Make(FMapView(), FVector2D(500.0, 500.0));
			TestEqual(TEXT("La de la derecha"), HitTestMarks(Marks, T, FVector2D(259.0, 250.0)), 1);
			TestEqual(TEXT("La del centro"), HitTestMarks(Marks, T, FVector2D(251.0, 250.0)), 0);
			TestEqual(TEXT("Lejos de todas"), HitTestMarks(Marks, T, FVector2D(100.0, 100.0)), INDEX_NONE);
			TestEqual(TEXT("Sin marcas"), HitTestMarks(TArray<FMapMark>(), T, FVector2D(250.0, 250.0)), INDEX_NONE);
		});
	});

	Describe("teclado y mando", [this]()
	{
		It("flechas y cruceta desplazan; hombros, gatillos y RePág acercan y alejan", [this]()
		{
			TestTrue(TEXT("Arriba"), PanDirectionForKey(FName(TEXT("Up"))).Equals(FVector2D(0.0, -1.0), 0.0));
			TestTrue(TEXT("Cruceta derecha"), PanDirectionForKey(FName(TEXT("Gamepad_DPad_Right"))).Equals(FVector2D(1.0, 0.0), 0.0));
			TestTrue(TEXT("Una letra no desplaza"), PanDirectionForKey(FName(TEXT("W"))).IsZero());
			TestEqual(TEXT("RB acerca"), ZoomFactorForKey(FName(TEXT("Gamepad_RightShoulder"))), ZoomStep);
			TestEqual(TEXT("AvPág aleja"), ZoomFactorForKey(FName(TEXT("PageDown"))), 1.0 / ZoomStep);
			TestEqual(TEXT("Otra tecla no"), ZoomFactorForKey(FName(TEXT("M"))), 1.0);
			TestTrue(TEXT("A del mando pulsa"), IsSheetAcceptKey(FName(TEXT("Gamepad_FaceButton_Bottom"))));
			TestFalse(TEXT("B no pulsa"), IsSheetAcceptKey(FName(TEXT("Gamepad_FaceButton_Right"))));
		});

		It("el stick tiene zona muerta y no salta al salir de ella", [this]()
		{
			TestEqual(TEXT("Dentro de la zona muerta"), StickPanAmount(0.2f), 0.0);
			TestEqual(TEXT("Justo al salir"), StickPanAmount(StickDeadZone + 1.e-4f), 0.0, 1.e-5);
			TestEqual(TEXT("A fondo"), StickPanAmount(1.0f), StickPanFraction, 1.e-9);
			TestEqual(TEXT("A fondo hacia el otro lado"), StickPanAmount(-1.0f), -StickPanFraction, 1.e-9);
			TestEqual(TEXT("Más allá de 1 se recorta"), StickPanAmount(3.0f), StickPanFraction, 1.e-9);
			TestEqual(TEXT("NaN"), StickPanAmount(std::numeric_limits<float>::quiet_NaN()), 0.0);
		});
	});

	Describe("sellos de story_es.json", [this]()
	{
		It("lee las etiquetas bilingües de map_marks y descarta sellos desconocidos o repetidos", [this]()
		{
			const FString Json = TEXT("{\"version\": 1, \"map_marks\": ["
				"{\"id\": \"water\", \"label\": \"Agua dulce\", \"labelEn\": \"Fresh water\"},"
				"{\"id\": \"volcano\", \"label\": \"Volcán\", \"labelEn\": \"Volcano\"},"
				"{\"id\": \"cave\", \"label\": \"Cueva\"},"
				"{\"id\": \"water\", \"label\": \"Otra\", \"labelEn\": \"Other\"},"
				"{\"label\": \"Sin id\"}]}");
			TArray<FMapStampLabel> Stamps;
			FString Error;
			if (!TestTrue(TEXT("Se lee"), ParseMapStampLabels(Json, Stamps, Error)) || !TestEqual(TEXT("Dos sellos"), Stamps.Num(), 2))
			{
				return;
			}
			TestEqual(TEXT("Orden del fichero"), Stamps[0].Id, FName(TEXT("water")));
			TestEqual(TEXT("Español"), Stamps[0].LabelEs, FString(TEXT("Agua dulce")));
			TestEqual(TEXT("Inglés"), Stamps[0].LabelEn, FString(TEXT("Fresh water")));
			TestTrue(TEXT("Sin inglés: vacío (Pick cae al español)"), Stamps[1].LabelEn.IsEmpty());
		});

		It("falla con un texto que no es JSON o sin map_marks y hay sellos de reserva", [this]()
		{
			TArray<FMapStampLabel> Stamps;
			FString Error;
			TestFalse(TEXT("No es JSON"), ParseMapStampLabels(TEXT("{"), Stamps, Error));
			TestFalse(TEXT("Con motivo"), Error.IsEmpty());
			TestFalse(TEXT("Sin map_marks"), ParseMapStampLabels(TEXT("{\"version\": 1}"), Stamps, Error));
			TestEqual(TEXT("Reserva: todos los del modelo"), FallbackStampLabels().Num(), FCartographyModel::KnownStamps().Num());
		});

		It("Y del mando cambia el foco entre la hoja y el cuaderno", [this]()
		{
			TestTrue(TEXT("Y"), IsMapFocusSwitchKey(FName(TEXT("Gamepad_FaceButton_Top"))));
			TestFalse(TEXT("A no"), IsMapFocusSwitchKey(FName(TEXT("Gamepad_FaceButton_Bottom"))));
		});
	});

	Describe("texto de las marcas", [this]()
	{
		It("limita la longitud y quita los saltos de línea sin recortar mientras se escribe", [this]()
		{
			TestEqual(TEXT("Salto de línea"), LimitMarkTextWhileTyping(TEXT("Agua\nbuena")), FString(TEXT("Agua buena")));
			TestEqual(TEXT("Espacio final conservado"), LimitMarkTextWhileTyping(TEXT("Cueva ")), FString(TEXT("Cueva ")));
			const FString Long(TEXT("0123456789012345678901234567890123456789"));
			TestEqual(TEXT("Tope"), LimitMarkTextWhileTyping(Long).Len(), FCartographyModel::MaxMarkTextLength);
			TestEqual(TEXT("Quedan"), RemainingMarkChars(TEXT("abc")), FCartographyModel::MaxMarkTextLength - 3);
			TestEqual(TEXT("Nunca negativo"), RemainingMarkChars(Long), 0);
		});

		It("se puede marcar con un sello conocido o con una nota con texto", [this]()
		{
			TestTrue(TEXT("Sello"), CanCommitMark(FName(TEXT("cave")), FString()));
			TestFalse(TEXT("Sello desconocido"), CanCommitMark(FName(TEXT("volcano")), TEXT("Humo")));
			TestTrue(TEXT("Nota"), CanCommitMark(NAME_None, TEXT("Aquí dormí")));
			TestFalse(TEXT("Nota en blanco"), CanCommitMark(NAME_None, TEXT("   ")));
		});

		It("M y View guardan el mapa salvo escribiendo; Volver y Start siempre", [this]()
		{
			TestTrue(TEXT("M"), ShouldCloseMapOnKey(FName(TEXT("M")), false));
			TestFalse(TEXT("M escribiendo es una letra"), ShouldCloseMapOnKey(FName(TEXT("M")), true));
			TestTrue(TEXT("View"), ShouldCloseMapOnKey(FName(TEXT("Gamepad_Special_Left")), false));
			TestTrue(TEXT("Escape escribiendo"), ShouldCloseMapOnKey(FName(TEXT("Escape")), true));
			TestTrue(TEXT("B"), ShouldCloseMapOnKey(FName(TEXT("Gamepad_FaceButton_Right")), false));
			TestTrue(TEXT("Start"), ShouldCloseMapOnKey(FName(TEXT("Gamepad_Special_Right")), false));
			TestFalse(TEXT("Otra letra"), ShouldCloseMapOnKey(FName(TEXT("N")), false));
		});
	});

	Describe("trazos generados", [this]()
	{
		It("el papel seco y sin pasadas no tiene manchas; mojado, más cuanto más", [this]()
		{
			TestEqual(TEXT("Seco"), WetStains(7u, 0.0f, 0).Num(), 0);
			TestEqual(TEXT("Empapado"), WetStains(7u, 1.0f, 0).Num(), MaxWetStains);
			TestTrue(TEXT("Más con más agua"), WetStains(7u, 0.3f, 0).Num() < WetStains(7u, 0.8f, 0).Num());
			TestEqual(TEXT("Cercos por pasadas, con tope"), WetStains(7u, 0.0f, 99).Num(), MaxDriedStains);
			TestEqual(TEXT("NaN no mancha"), WetStains(7u, std::numeric_limits<float>::quiet_NaN(), 0).Num(), 0);
		});

		It("las manchas son deterministas, caen en la hoja y los cercos no se mueven al secarse", [this]()
		{
			const TArray<FWetStain> Wet = WetStains(42u, 0.6f, 2);
			const TArray<FWetStain> Again = WetStains(42u, 0.6f, 2);
			const TArray<FWetStain> Dry = WetStains(42u, 0.0f, 2);
			if (!TestEqual(TEXT("Mismo número"), Wet.Num(), Again.Num()) || !TestEqual(TEXT("Dos cercos"), Dry.Num(), 2))
			{
				return;
			}
			for (int32 I = 0; I < Wet.Num(); ++I)
			{
				TestTrue(TEXT("Deterministas"), Wet[I].Center.Equals(Again[I].Center, 0.0) && Wet[I].Radius == Again[I].Radius);
				TestTrue(TEXT("En la hoja"), Wet[I].Center.X > 0.0 && Wet[I].Center.X < 1.0 && Wet[I].Center.Y > 0.0 && Wet[I].Center.Y < 1.0);
				TestTrue(TEXT("Opacidad válida"), Wet[I].Alpha > 0.0f && Wet[I].Alpha <= 1.0f);
			}
			TestTrue(TEXT("El último cerco no cambia al secarse"), Wet.Last().bDried && Wet.Last().Center.Equals(Dry.Last().Center, 0.0));
		});

		It("el camino de estrellas va de la ruina a la isla con flecha y estrella", [this]()
		{
			FWayfindingAnnotation A;
			A.Technique = EWayfindingTechnique::StarPath;
			A.From = FVector2D(0.0, 0.0);
			A.To = FVector2D(1200.0, 600.0);
			A.DistanceMeters = static_cast<float>(A.To.Size());
			const TArray<FPolyline> Strokes = AnnotationStrokes(A);
			if (!TestTrue(TEXT("Hay trazos"), Strokes.Num() > 3))
			{
				return;
			}
			const FVector2D From = FCartographyModel::WorldToMap(A.From);
			const FVector2D To = FCartographyModel::WorldToMap(A.To);
			TestTrue(TEXT("Empieza en la ruina"), Strokes[0][0].Equals(From, 1.e-9));
			bool bArrowAtTo = false;
			for (const FPolyline& Line : Strokes)
			{
				bArrowAtTo |= Line.Num() == 3 && Line[1].Equals(To, 1.e-9);
				for (const FVector2D& P : Line)
				{
					TestTrue(TEXT("Puntos finitos"), FMath::IsFinite(P.X) && FMath::IsFinite(P.Y));
				}
			}
			TestTrue(TEXT("Flecha en la isla"), bArrowAtTo);
		});

		It("nubes y color del agua dibujan un anillo con el radio de la anotación", [this]()
		{
			FWayfindingAnnotation A;
			A.Technique = EWayfindingTechnique::FixedClouds;
			A.From = A.To = FVector2D(300.0, -900.0);
			A.DistanceMeters = 180.0f;
			const TArray<FPolyline> Ring = AnnotationStrokes(A);
			if (!TestEqual(TEXT("Doce trazos"), Ring.Num(), 12))
			{
				return;
			}
			const FVector2D Center = FCartographyModel::WorldToMap(A.To);
			TestEqual(TEXT("Radio"), FVector2D::Distance(Ring[0][0], Center), FCartographyModel::MetersToMap(180.0), 1.e-9);

			A.DistanceMeters = 0.0f;
			const TArray<FPolyline> Tiny = AnnotationStrokes(A);
			TestEqual(TEXT("Radio mínimo"), FVector2D::Distance(Tiny[0][0], Center), MinAnnotationRadius, 1.e-9);

			A.Technique = EWayfindingTechnique::SwellReading;
			TestEqual(TEXT("Sin rumbo también es un anillo"), AnnotationStrokes(A).Num(), 12);
		});

		It("un rumbo muy largo no genera más de 200 trazos", [this]()
		{
			FWayfindingAnnotation A;
			A.Technique = EWayfindingTechnique::BirdsAtDusk;
			A.From = FVector2D(-30000.0, -30000.0);
			A.To = FVector2D(30000.0, 30000.0);
			TestTrue(TEXT("Acotado"), AnnotationStrokes(A).Num() <= 201);
		});

		It("los bocetos de recetas son deterministas y caben en su recuadro", [this]()
		{
			const TArray<FPolyline> A = RecipeDoodle(FName(TEXT("hacha_piedra")));
			const TArray<FPolyline> B = RecipeDoodle(FName(TEXT("hacha_piedra")));
			TestTrue(TEXT("Entre 2 y 4 trazos"), A.Num() >= 2 && A.Num() <= 4);
			TestEqual(TEXT("Deterministas"), A.Num(), B.Num());
			for (int32 S = 0; S < A.Num() && S < B.Num(); ++S)
			{
				TestTrue(TEXT("Al menos cuatro puntos"), A[S].Num() >= 4);
				for (int32 I = 0; I < A[S].Num() && I < B[S].Num(); ++I)
				{
					TestTrue(TEXT("Mismo punto"), A[S][I].Equals(B[S][I], 0.0));
					TestTrue(TEXT("Dentro del recuadro"), A[S][I].X >= 0.0 && A[S][I].X <= 1.0 && A[S][I].Y >= 0.0 && A[S][I].Y <= 1.0);
				}
			}
			TestTrue(TEXT("Hash estable FNV-1a"), StableHash(TEXT("a")) == 0xE40C292Cu);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
