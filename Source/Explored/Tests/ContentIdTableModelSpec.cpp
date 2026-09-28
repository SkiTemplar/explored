#include "Misc/AutomationTest.h"

#include "Items/ContentIdTableModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ContentIdTest
{
	TArray<FName> Names(std::initializer_list<const TCHAR*> Ids)
	{
		TArray<FName> Out;
		for (const TCHAR* Id : Ids)
		{
			Out.Add(FName(Id));
		}
		return Out;
	}

	FContentFile File(const TCHAR* Name, const char* Text)
	{
		FContentFile Out;
		Out.Name = Name;
		for (const char* C = Text; *C; ++C)
		{
			Out.Bytes.Add(static_cast<uint8>(*C));
		}
		return Out;
	}
}

BEGIN_DEFINE_SPEC(FContentIdTableModelSpec, "Explored.Items.ContentIds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FContentIdTableModelSpec)

void FContentIdTableModelSpec::Define()
{
	using namespace ContentIdTest;

	Describe("La tabla de ids de red", [this]()
	{
		It("numera por orden de bytes, lea los ids en el orden que los lea", [this]()
		{
			FContentIdTableModel A;
			FContentIdTableModel B;
			FString Error;
			TestTrue(TEXT("Tabla A"), A.SetIds(EContentKind::Item, Names({ TEXT("rama_seca"), TEXT("basalto"), TEXT("coco_maduro"), TEXT("rama_2"), TEXT("rama") }), Error));
			TestTrue(TEXT("Tabla B"), B.SetIds(EContentKind::Item, Names({ TEXT("rama"), TEXT("coco_maduro"), TEXT("rama_2"), TEXT("basalto"), TEXT("rama_seca") }), Error));
			TestTrue(TEXT("Misma tabla"), A.GetIds(EContentKind::Item) == B.GetIds(EContentKind::Item));
			TestEqual(TEXT("basalto es el 0"), (int32)A.ToNetId(EContentKind::Item, TEXT("basalto")), 0);
			// Orden por bytes: «rama» < «rama_2» < «rama_seca» ('2' 0x32 < 's' 0x73).
			TestEqual(TEXT("rama"), (int32)A.ToNetId(EContentKind::Item, TEXT("rama")), 2);
			TestEqual(TEXT("rama_2"), (int32)A.ToNetId(EContentKind::Item, TEXT("rama_2")), 3);
			TestEqual(TEXT("rama_seca"), (int32)A.ToNetId(EContentKind::Item, TEXT("rama_seca")), 4);
			TestTrue(TEXT("Ida y vuelta"), A.FromNetId(EContentKind::Item, 1) == FName(TEXT("coco_maduro")));
		});

		It("ordena el guion bajo antes que las letras, como Python y DataCheck", [this]()
		{
			FContentIdTableModel Table;
			FString Error;
			Table.SetIds(EContentKind::Item, Names({ TEXT("ab"), TEXT("a_b"), TEXT("a9"), TEXT("a") }), Error);
			const TArray<FName>& Ids = Table.GetIds(EContentKind::Item);
			TestTrue(TEXT("a, a9, a_b, ab"), Ids == Names({ TEXT("a"), TEXT("a9"), TEXT("a_b"), TEXT("ab") }));
		});

		It("numera cada fichero por separado", [this]()
		{
			FContentIdTableModel Table;
			FString Error;
			Table.SetIds(EContentKind::Item, Names({ TEXT("cuerda"), TEXT("hacha") }), Error);
			Table.SetIds(EContentKind::BuildingPiece, Names({ TEXT("hacha") }), Error);
			TestEqual(TEXT("hacha es el 1 en objetos"), (int32)Table.ToNetId(EContentKind::Item, TEXT("hacha")), 1);
			TestEqual(TEXT("y el 0 en piezas"), (int32)Table.ToNetId(EContentKind::BuildingPiece, TEXT("hacha")), 0);
			TestEqual(TEXT("La tabla de logros está vacía"), Table.Num(EContentKind::Achievement), 0);
		});

		It("devuelve «ninguno» con ids o números que no están", [this]()
		{
			FContentIdTableModel Table;
			FString Error;
			Table.SetIds(EContentKind::Item, Names({ TEXT("basalto") }), Error);
			TestEqual(TEXT("Id desconocido"), (int32)Table.ToNetId(EContentKind::Item, TEXT("objeto_retirado")), (int32)FContentIdTableModel::InvalidNetId);
			TestEqual(TEXT("NAME_None"), (int32)Table.ToNetId(EContentKind::Item, NAME_None), (int32)FContentIdTableModel::InvalidNetId);
			TestTrue(TEXT("Número fuera de rango"), Table.FromNetId(EContentKind::Item, 1).IsNone());
			TestTrue(TEXT("El número de «ninguno»"), Table.FromNetId(EContentKind::Item, FContentIdTableModel::InvalidNetId).IsNone());
			TestTrue(TEXT("Tabla fuera de rango"), Table.FromNetId(EContentKind::Count, 0).IsNone());
		});

		It("rechaza ids repetidos, vacíos o con caracteres raros sin tocar la tabla", [this]()
		{
			FContentIdTableModel Table;
			FString Error;
			Table.SetIds(EContentKind::Item, Names({ TEXT("basalto") }), Error);
			TestFalse(TEXT("Repetido"), Table.SetIds(EContentKind::Item, Names({ TEXT("coco"), TEXT("coco") }), Error));
			TestFalse(TEXT("Repetido sin distinguir mayúsculas (FName)"), Table.SetIds(EContentKind::Item, Names({ TEXT("coco"), TEXT("COCO") }), Error));
			TestFalse(TEXT("Con espacio"), Table.SetIds(EContentKind::Item, Names({ TEXT("coco maduro") }), Error));
			TestFalse(TEXT("Con tilde"), Table.SetIds(EContentKind::Item, Names({ TEXT("cañas") }), Error));
			TestFalse(TEXT("Con guion"), Table.SetIds(EContentKind::Item, Names({ TEXT("coco-maduro") }), Error));
			TArray<FName> WithNone = Names({ TEXT("coco") });
			WithNone.Add(NAME_None);
			TestFalse(TEXT("Vacío"), Table.SetIds(EContentKind::Item, WithNone, Error));
			TestFalse(TEXT("Da un motivo"), Error.IsEmpty());
			TestEqual(TEXT("Sigue la tabla anterior"), Table.Num(EContentKind::Item), 1);
			TestEqual(TEXT("Con basalto"), (int32)Table.ToNetId(EContentKind::Item, TEXT("basalto")), 0);
		});

		It("admite 65 535 ids y rechaza uno más", [this]()
		{
			TArray<FName> Many;
			for (int32 I = 0; I <= FContentIdTableModel::MaxIdsPerKind; ++I)
			{
				Many.Add(FName(*FString::Printf(TEXT("id_%05d"), I)));
			}
			FContentIdTableModel Table;
			FString Error;
			TestFalse(TEXT("65 536 no caben"), Table.SetIds(EContentKind::Item, Many, Error));
			Many.Pop();
			TestTrue(TEXT("65 535 sí"), Table.SetIds(EContentKind::Item, Many, Error));
			TestEqual(TEXT("El último no choca con «ninguno»"), (int32)Table.ToNetId(EContentKind::Item, Many.Last()), 0xFFFE);
		});
	});

	Describe("El versionado", [this]()
	{
		It("un id nuevo desplaza a los que van detrás, pero cambia el hash y el saludo rechaza la mezcla", [this]()
		{
			// Estrategia documentada en ContentIdTableModel.h: la tabla es estable
			// para unos mismos ficheros y el hash impide mezclar dos versiones.
			const char* Before = "[{\"id\":\"basalto\"},{\"id\":\"rama_seca\"}]";
			const char* After = "[{\"id\":\"basalto\"},{\"id\":\"coco_nuevo\"},{\"id\":\"rama_seca\"}]";
			FContentIdTableModel Old;
			FContentIdTableModel New;
			FString Error;
			Old.SetIds(EContentKind::Item, Names({ TEXT("basalto"), TEXT("rama_seca") }), Error);
			New.SetIds(EContentKind::Item, Names({ TEXT("basalto"), TEXT("coco_nuevo"), TEXT("rama_seca") }), Error);
			TestEqual(TEXT("Lo de delante no se mueve"), (int32)New.ToNetId(EContentKind::Item, TEXT("basalto")), (int32)Old.ToNetId(EContentKind::Item, TEXT("basalto")));
			TestNotEqual(TEXT("Lo de detrás sí"), (int32)New.ToNetId(EContentKind::Item, TEXT("rama_seca")), (int32)Old.ToNetId(EContentKind::Item, TEXT("rama_seca")));

			const uint64 HashOld = FContentIdTableModel::ComputeContentHash({ File(TEXT("items.json"), Before) });
			const uint64 HashNew = FContentIdTableModel::ComputeContentHash({ File(TEXT("items.json"), After) });
			TestTrue(TEXT("El hash cambia: servidor y cliente no llegan a hablar"), HashOld != HashNew);
		});
	});

	Describe("El hash de contenido", [this]()
	{
		It("es FNV-1a de 64 bits (valores de referencia)", [this]()
		{
			const uint8 A[] = { 'a' };
			const uint8 FooBar[] = { 'f', 'o', 'o', 'b', 'a', 'r' };
			TestTrue(TEXT("Vacío"), FContentIdTableModel::Fnv1a64(nullptr, 0) == 0xcbf29ce484222325ull);
			TestTrue(TEXT("«a»"), FContentIdTableModel::Fnv1a64(A, 1) == 0xaf63dc4c8601ec8cull);
			TestTrue(TEXT("«foobar»"), FContentIdTableModel::Fnv1a64(FooBar, 6) == 0x85944171f73967e8ull);
		});

		It("coincide con el de Tools/DataCheck y no depende del orden de lectura", [this]()
		{
			// El mismo valor está en Tools/DataCheck/tests/test_inventory.py.
			const uint64 Expected = 0x2867c518f7eaf856ull;
			const uint64 Forward = FContentIdTableModel::ComputeContentHash({ File(TEXT("a.json"), "[]"), File(TEXT("b.json"), "{}") });
			const uint64 Backward = FContentIdTableModel::ComputeContentHash({ File(TEXT("b.json"), "{}"), File(TEXT("a.json"), "[]") });
			TestTrue(TEXT("Valor de referencia"), Forward == Expected);
			TestTrue(TEXT("Mismo valor en otro orden"), Backward == Expected);
		});

		It("cambia con un espacio de más y al mover bytes de un fichero al siguiente", [this]()
		{
			const uint64 Base = FContentIdTableModel::ComputeContentHash({ File(TEXT("a.json"), "[]"), File(TEXT("b.json"), "{}") });
			const uint64 Space = FContentIdTableModel::ComputeContentHash({ File(TEXT("a.json"), "[]"), File(TEXT("b.json"), "{} ") });
			const uint64 Shifted = FContentIdTableModel::ComputeContentHash({ File(TEXT("a.json"), "[]{"), File(TEXT("b.json"), "}") });
			const uint64 Renamed = FContentIdTableModel::ComputeContentHash({ File(TEXT("a.json"), "[]"), File(TEXT("c.json"), "{}") });
			TestTrue(TEXT("Un espacio"), Space != Base);
			TestTrue(TEXT("Bytes movidos (la concatenación sola no lo vería)"), Shifted != Base);
			TestTrue(TEXT("Fichero renombrado"), Renamed != Base);
			TestTrue(TEXT("Estable entre llamadas"), Base == FContentIdTableModel::ComputeContentHash({ File(TEXT("a.json"), "[]"), File(TEXT("b.json"), "{}") }));
		});
	});
}

#endif
