#include "Misc/AutomationTest.h"

#include "Narrative/NotebookModel.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FNotebookSpec, "Explored.Notebook",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FNotebookSpec)

void FNotebookSpec::Define()
{
	It(TEXT("empieza sin páginas leídas"), [this]()
	{
		const FNotebookState S;
		TestEqual(TEXT("Leídas"), FNotebookModel::ReadCount(S), 0);
		TestFalse(TEXT("Completo"), FNotebookModel::IsComplete(S));
		TestFalse(TEXT("Críticas"), FNotebookModel::HasCriticalPages(S));
	});

	It(TEXT("solo la 1 y la 8 son críticas"), [this]()
	{
		for (int32 Page = 1; Page <= FNotebookModel::PageCount; ++Page)
		{
			TestEqual(FString::Printf(TEXT("Página %d"), Page), FNotebookModel::IsCritical(Page), Page == 1 || Page == 8);
		}
	});

	It(TEXT("marcar una página la deja leída sin tocar el estado de entrada"), [this]()
	{
		const FNotebookState S;
		const FNotebookMarkResult R = FNotebookModel::MarkRead(S, 3);
		TestTrue(TEXT("Cambia"), R.bChanged);
		TestTrue(TEXT("Leída"), FNotebookModel::IsRead(R.State, 3));
		TestFalse(TEXT("Otra no"), FNotebookModel::IsRead(R.State, 4));
		TestEqual(TEXT("Una"), FNotebookModel::ReadCount(R.State), 1);
		TestEqual(TEXT("Entrada intacta"), S.PagesMask, static_cast<uint8>(0));
	});

	It(TEXT("marcar dos veces la misma página no cambia nada"), [this]()
	{
		const FNotebookState S = FNotebookModel::MarkRead(FNotebookState(), 5).State;
		const FNotebookMarkResult Again = FNotebookModel::MarkRead(S, 5);
		TestFalse(TEXT("Sin cambio"), Again.bChanged);
		TestTrue(TEXT("Mismo estado"), Again.State == S);
	});

	It(TEXT("rechaza páginas fuera de 1-8"), [this]()
	{
		const int32 Bad[] = {0, 9, -1, 255, MIN_int32, MAX_int32};
		for (const int32 Page : Bad)
		{
			const FNotebookMarkResult R = FNotebookModel::MarkRead(FNotebookState(), Page);
			TestFalse(FString::Printf(TEXT("Página %d"), Page), R.bChanged);
			TestEqual(TEXT("Sin bits"), R.State.PagesMask, static_cast<uint8>(0));
			TestFalse(TEXT("No leída"), FNotebookModel::IsRead(FNotebookState{0xFF}, Page));
			TestFalse(TEXT("No crítica"), FNotebookModel::IsCritical(Page));
		}
	});

	It(TEXT("8 de 8 completa el cuaderno; 7 no"), [this]()
	{
		FNotebookState S;
		for (int32 Page = 1; Page <= 7; ++Page)
		{
			S = FNotebookModel::MarkRead(S, Page).State;
		}
		TestEqual(TEXT("Siete"), FNotebookModel::ReadCount(S), 7);
		TestFalse(TEXT("Incompleto"), FNotebookModel::IsComplete(S));
		TestFalse(TEXT("Falta la 8"), FNotebookModel::HasCriticalPages(S));
		S = FNotebookModel::MarkRead(S, 8).State;
		TestEqual(TEXT("Ocho"), FNotebookModel::ReadCount(S), 8);
		TestTrue(TEXT("Completo"), FNotebookModel::IsComplete(S));
	});

	It(TEXT("las dos críticas bastan para el camino crítico"), [this]()
	{
		FNotebookState S = FNotebookModel::MarkRead(FNotebookState(), 1).State;
		S = FNotebookModel::MarkRead(S, 8).State;
		TestTrue(TEXT("Críticas"), FNotebookModel::HasCriticalPages(S));
		TestFalse(TEXT("No completo"), FNotebookModel::IsComplete(S));
	});

	It(TEXT("da los ids de texto de página y nota"), [this]()
	{
		TestEqual(TEXT("Página 1"), FNotebookModel::PageTextId(1), FName(TEXT("cuaderno_p1")));
		TestEqual(TEXT("Nota 8"), FNotebookModel::NoteTextId(8), FName(TEXT("cuaderno_p8_nota")));
		TestTrue(TEXT("Página 0"), FNotebookModel::PageTextId(0) == NAME_None);
		TestTrue(TEXT("Nota 9"), FNotebookModel::NoteTextId(9) == NAME_None);
	});
}

#endif
