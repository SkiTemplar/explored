#include "Narrative/NotebookModel.h"

namespace NotebookDetail
{
	uint8 BitFor(int32 Page)
	{
		return static_cast<uint8>(1u << static_cast<uint32>(Page - 1));
	}
}

bool FNotebookModel::IsValidPage(int32 Page)
{
	return Page >= 1 && Page <= PageCount;
}

bool FNotebookModel::IsCritical(int32 Page)
{
	return Page == 1 || Page == PageCount;
}

bool FNotebookModel::IsRead(const FNotebookState& State, int32 Page)
{
	return IsValidPage(Page) && (State.PagesMask & NotebookDetail::BitFor(Page)) != 0;
}

FNotebookMarkResult FNotebookModel::MarkRead(const FNotebookState& State, int32 Page)
{
	FNotebookMarkResult Result;
	Result.State = State;
	if (!IsValidPage(Page) || IsRead(State, Page))
	{
		return Result;
	}
	Result.State.PagesMask = static_cast<uint8>(State.PagesMask | NotebookDetail::BitFor(Page));
	Result.bChanged = true;
	return Result;
}

int32 FNotebookModel::ReadCount(const FNotebookState& State)
{
	int32 Count = 0;
	for (uint32 Bits = State.PagesMask; Bits != 0; Bits &= Bits - 1)
	{
		++Count;
	}
	return Count;
}

bool FNotebookModel::IsComplete(const FNotebookState& State)
{
	return State.PagesMask == AllPagesMask;
}

bool FNotebookModel::HasCriticalPages(const FNotebookState& State)
{
	return IsRead(State, 1) && IsRead(State, PageCount);
}

FName FNotebookModel::PageTextId(int32 Page)
{
	return IsValidPage(Page) ? FName(*FString::Printf(TEXT("cuaderno_p%d"), Page)) : FName(NAME_None);
}

FName FNotebookModel::NoteTextId(int32 Page)
{
	return IsValidPage(Page) ? FName(*FString::Printf(TEXT("cuaderno_p%d_nota"), Page)) : FName(NAME_None);
}
