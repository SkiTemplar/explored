#include "Misc/AutomationTest.h"

#include "UI/ExploredInputSettingsSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FExploredFrontendInputSpec, "Explored.Frontend.Input",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FExploredFrontendInputSpec)

void FExploredFrontendInputSpec::Define()
{
	Describe("UExploredInputSettingsSubsystem", [this]()
	{
		It("devuelve la tecla por defecto cuando no hay remapeo", [this]()
		{
			UExploredInputSettingsSubsystem* Subsystem = NewObject<UExploredInputSettingsSubsystem>();
			Subsystem->RegisterAction(TEXT("Test_ActionA"), EKeys::SpaceBar);
			TestEqual(TEXT("Tecla por defecto"), Subsystem->GetKeyFor(TEXT("Test_ActionA"), EKeys::SpaceBar), FKey(EKeys::SpaceBar));
		});

		It("cambia la tecla y dispara OnBindingsChanged", [this]()
		{
			UExploredInputSettingsSubsystem* Subsystem = NewObject<UExploredInputSettingsSubsystem>();
			Subsystem->RegisterAction(TEXT("Test_ActionB"), EKeys::SpaceBar);

			FName FiredFor = NAME_None;
			int32 FireCount = 0;
			Subsystem->OnBindingsChanged.AddLambda([&FiredFor, &FireCount](FName ActionName)
			{
				FiredFor = ActionName;
				++FireCount;
			});

			const bool bChanged = Subsystem->SetKeyFor(TEXT("Test_ActionB"), EKeys::F);
			TestTrue(TEXT("El cambio se acepta"), bChanged);
			TestEqual(TEXT("La tecla efectiva es la nueva"), Subsystem->GetKeyFor(TEXT("Test_ActionB"), EKeys::SpaceBar), FKey(EKeys::F));
			TestEqual(TEXT("El delegado se dispara una vez"), FireCount, 1);
			TestEqual(TEXT("El delegado reporta la acción correcta"), FiredFor, FName(TEXT("Test_ActionB")));
		});

		It("rechaza una tecla ya usada por otra acción registrada", [this]()
		{
			UExploredInputSettingsSubsystem* Subsystem = NewObject<UExploredInputSettingsSubsystem>();
			Subsystem->RegisterAction(TEXT("Test_Jump"), EKeys::SpaceBar);
			Subsystem->RegisterAction(TEXT("Test_Sprint"), EKeys::LeftShift);

			int32 FireCount = 0;
			Subsystem->OnBindingsChanged.AddLambda([&FireCount](FName) { ++FireCount; });

			const bool bChanged = Subsystem->SetKeyFor(TEXT("Test_Sprint"), EKeys::SpaceBar);
			TestFalse(TEXT("Se rechaza el conflicto"), bChanged);
			TestEqual(TEXT("Sprint conserva su tecla"), Subsystem->GetKeyFor(TEXT("Test_Sprint"), EKeys::LeftShift), FKey(EKeys::LeftShift));
			TestEqual(TEXT("No dispara el delegado"), FireCount, 0);
		});

		It("ResetKeyFor devuelve la tecla por defecto", [this]()
		{
			UExploredInputSettingsSubsystem* Subsystem = NewObject<UExploredInputSettingsSubsystem>();
			Subsystem->RegisterAction(TEXT("Test_ActionC"), EKeys::SpaceBar);
			Subsystem->SetKeyFor(TEXT("Test_ActionC"), EKeys::F);
			Subsystem->ResetKeyFor(TEXT("Test_ActionC"));
			TestEqual(TEXT("Vuelve a la tecla por defecto"), Subsystem->GetKeyFor(TEXT("Test_ActionC"), EKeys::SpaceBar), FKey(EKeys::SpaceBar));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
