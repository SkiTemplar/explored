#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "UObject/StrongObjectPtr.h"

#include "UI/ExploredInputSettingsSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FExploredFrontendInputSpec, "Explored.Frontend.Input",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	// UExploredInputSettingsSubsystem hereda de ULocalPlayerSubsystem, cuya
	// UCLASS exige Within = LocalPlayer (y ULocalPlayer, a su vez, Within =
	// Engine): NewObject<UExploredInputSettingsSubsystem>() sin ese outer
	// crea el objeto dentro de un Package y dispara un ensure (ClassWithin
	// inválido). Ninguno de los dos objetos se inicializa de verdad
	// (LocalPlayer::Init, etc.); solo existen para darle al subsistema un
	// outer del tipo que su reflection exige.
	TStrongObjectPtr<ULocalPlayer> DummyLocalPlayer;
	TStrongObjectPtr<UExploredInputSettingsSubsystem> Subsystem;

END_DEFINE_SPEC(FExploredFrontendInputSpec)

void FExploredFrontendInputSpec::Define()
{
	BeforeEach([this]()
	{
		DummyLocalPlayer = TStrongObjectPtr<ULocalPlayer>(NewObject<ULocalPlayer>(GEngine));
		Subsystem = TStrongObjectPtr<UExploredInputSettingsSubsystem>(NewObject<UExploredInputSettingsSubsystem>(DummyLocalPlayer.Get()));
		// Sin esto, SetKeyFor escribía Overrides en el Game.ini real del usuario y
		// el subsistema arrancaba con los remapeos que el CDO leyó de ese ini.
		Subsystem->UseTransientStorageForTesting();
	});

	AfterEach([this]()
	{
		Subsystem.Reset();
		DummyLocalPlayer.Reset();
	});

	Describe("UExploredInputSettingsSubsystem", [this]()
	{
		It("devuelve la tecla por defecto cuando no hay remapeo", [this]()
		{
			Subsystem->RegisterAction(TEXT("Test_ActionA"), EKeys::SpaceBar);
			TestEqual(TEXT("Tecla por defecto"), Subsystem->GetKeyFor(TEXT("Test_ActionA"), EKeys::SpaceBar), FKey(EKeys::SpaceBar));
		});

		It("cambia la tecla y dispara OnBindingsChanged", [this]()
		{
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
			Subsystem->RegisterAction(TEXT("Test_Jump"), EKeys::SpaceBar);
			Subsystem->RegisterAction(TEXT("Test_Sprint"), EKeys::LeftShift);

			int32 FireCount = 0;
			Subsystem->OnBindingsChanged.AddLambda([&FireCount](FName) { ++FireCount; });

			const bool bChanged = Subsystem->SetKeyFor(TEXT("Test_Sprint"), EKeys::SpaceBar);
			TestFalse(TEXT("Se rechaza el conflicto"), bChanged);
			TestEqual(TEXT("Sprint conserva su tecla"), Subsystem->GetKeyFor(TEXT("Test_Sprint"), EKeys::LeftShift), FKey(EKeys::LeftShift));
			TestEqual(TEXT("No dispara el delegado"), FireCount, 0);
		});

		It("rechaza teclas reservadas y de mando", [this]()
		{
			Subsystem->RegisterAction(TEXT("Test_ActionD"), EKeys::SpaceBar);
			TestFalse(TEXT("Escape está reservada"), Subsystem->SetKeyFor(TEXT("Test_ActionD"), EKeys::Escape));
			TestFalse(TEXT("W es de movimiento"), Subsystem->SetKeyFor(TEXT("Test_ActionD"), EKeys::W));
			TestFalse(TEXT("Los botones de mando no se remapean"), Subsystem->SetKeyFor(TEXT("Test_ActionD"), EKeys::Gamepad_FaceButton_Bottom));
			TestTrue(TEXT("Un botón de ratón sí"), Subsystem->SetKeyFor(TEXT("Test_ActionD"), EKeys::ThumbMouseButton));
		});

		It("GetConflictFor nombra la acción que ya usa la tecla", [this]()
		{
			Subsystem->RegisterAction(TEXT("Test_Interact"), EKeys::E);
			Subsystem->RegisterAction(TEXT("Test_Jump2"), EKeys::SpaceBar);
			TestEqual(TEXT("E la usa interactuar"), Subsystem->GetConflictFor(TEXT("Test_Jump2"), EKeys::E), FName(TEXT("Test_Interact")));
			TestTrue(TEXT("F está libre"), Subsystem->GetConflictFor(TEXT("Test_Jump2"), EKeys::F).IsNone());
		});

		It("ResetKeyFor devuelve la tecla por defecto", [this]()
		{
			Subsystem->RegisterAction(TEXT("Test_ActionC"), EKeys::SpaceBar);
			Subsystem->SetKeyFor(TEXT("Test_ActionC"), EKeys::F);
			Subsystem->ResetKeyFor(TEXT("Test_ActionC"));
			TestEqual(TEXT("Vuelve a la tecla por defecto"), Subsystem->GetKeyFor(TEXT("Test_ActionC"), EKeys::SpaceBar), FKey(EKeys::SpaceBar));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
