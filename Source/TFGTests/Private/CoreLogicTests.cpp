#if WITH_DEV_AUTOMATION_TESTS

#include "EnemyResourcePickup.h"
#include "Misc/AutomationTest.h"
#include "Systems/RunPowerPersistenceSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTFGCollectibleInventoryTest,
	"TFG.Cpp.Unit.Inventory.Collectibles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFGCollectibleInventoryTest::RunTest(const FString& Parameters)
{
	FRunCollectibleInventory Inventory;
	TestEqual(TEXT("El inventario empieza vacio"), Inventory.Total(), 0);

	Inventory.Add(EEnemyCollectibleType::Luz, 2);
	Inventory.Add(EEnemyCollectibleType::Vida, 3);
	Inventory.Add(EEnemyCollectibleType::Energia, 4);
	Inventory.Add(EEnemyCollectibleType::Mana, 5);

	TestEqual(TEXT("Luz"), Inventory.Get(EEnemyCollectibleType::Luz), 2);
	TestEqual(TEXT("Vida"), Inventory.Get(EEnemyCollectibleType::Vida), 3);
	TestEqual(TEXT("Energia"), Inventory.Get(EEnemyCollectibleType::Energia), 4);
	TestEqual(TEXT("Mana"), Inventory.Get(EEnemyCollectibleType::Mana), 5);
	TestEqual(TEXT("Total"), Inventory.Total(), 14);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTFGPotionInventoryTest,
	"TFG.Cpp.Unit.Inventory.Potions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFGPotionInventoryTest::RunTest(const FString& Parameters)
{
	FRunPotionInventory Inventory;
	Inventory.Add(EPotionType::Vida, 3);
	Inventory.Add(EPotionType::Mana, 2);
	Inventory.Add(EPotionType::Energia, 1);

	TestEqual(TEXT("Pociones de vida"), Inventory.Get(EPotionType::Vida), 3);
	TestEqual(TEXT("Pociones de mana"), Inventory.Get(EPotionType::Mana), 2);
	TestEqual(TEXT("Pociones de energia"), Inventory.Get(EPotionType::Energia), 1);
	TestEqual(TEXT("Total de pociones"), Inventory.Total(), 6);

	Inventory.Add(EPotionType::Mana, -20);
	TestEqual(TEXT("Las pociones nunca son negativas"), Inventory.Get(EPotionType::Mana), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTFGPersistentPowerCountTest,
	"TFG.Cpp.Unit.Powers.PersistentCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFGPersistentPowerCountTest::RunTest(const FString& Parameters)
{
	FRunPersistentPowerState State;
	TestEqual(TEXT("No hay poderes al empezar"), State.CountActivePowers(), 0);

	State.bAtaquesQueman = true;
	State.bDashHabilitado = true;
	State.bProyectilesAutoapuntado = true;
	TestEqual(TEXT("Cuenta poderes pasivos distintos"), State.CountActivePowers(), 3);

	State.PowerUpActivoDeSala = 11;
	TestEqual(TEXT("Cuenta el poder activo equipado"), State.CountActivePowers(), 4);

	State = FRunPersistentPowerState();
	State.bDobleDanyoPreparado = true;
	State.PowerUpActivoDeSala = 1;
	TestEqual(TEXT("Doble dano no se cuenta dos veces"), State.CountActivePowers(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTFGCollectibleNamesTest,
	"TFG.Cpp.Unit.Inventory.CollectibleNames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFGCollectibleNamesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Nombre Luz"), FString(AEnemyResourcePickup::TypeToString(EEnemyCollectibleType::Luz)), FString(TEXT("LUZ")));
	TestEqual(TEXT("Nombre Vida"), FString(AEnemyResourcePickup::TypeToString(EEnemyCollectibleType::Vida)), FString(TEXT("VIDA")));
	TestEqual(TEXT("Nombre Energia"), FString(AEnemyResourcePickup::TypeToString(EEnemyCollectibleType::Energia)), FString(TEXT("ENERGIA")));
	TestEqual(TEXT("Nombre Mana"), FString(AEnemyResourcePickup::TypeToString(EEnemyCollectibleType::Mana)), FString(TEXT("MANA")));
	return true;
}

#endif
