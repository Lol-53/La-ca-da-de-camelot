#include "Systems/RunPowerPersistenceSubsystem.h"

#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRunPowerPersistence, Log, All);

const FString URunPowerPersistenceSubsystem::LegacyProgressionSlot(TEXT("TFG_ProgresoPermanente"));
const FString URunPowerPersistenceSubsystem::SelectedProfileSlot(TEXT("TFG_PerfilActivo"));

int32 FRunCollectibleInventory::Get(const EEnemyCollectibleType Type) const
{
	switch (Type)
	{
	case EEnemyCollectibleType::Luz:
		return Luz;
	case EEnemyCollectibleType::Vida:
		return Vida;
	case EEnemyCollectibleType::Energia:
		return Energia;
	case EEnemyCollectibleType::Mana:
		return Mana;
	default:
		return 0;
	}
}

void FRunCollectibleInventory::Add(
	const EEnemyCollectibleType Type,
	const int32 Amount)
{
	switch (Type)
	{
	case EEnemyCollectibleType::Luz:
		Luz += Amount;
		break;
	case EEnemyCollectibleType::Vida:
		Vida += Amount;
		break;
	case EEnemyCollectibleType::Energia:
		Energia += Amount;
		break;
	case EEnemyCollectibleType::Mana:
		Mana += Amount;
		break;
	default:
		break;
	}
}

int32 FRunPotionInventory::Get(const EPotionType Type) const
{
	switch (Type)
	{
	case EPotionType::Vida: return Vida;
	case EPotionType::Mana: return Mana;
	case EPotionType::Energia: return Energia;
	default: return 0;
	}
}

void FRunPotionInventory::Add(const EPotionType Type, const int32 Amount)
{
	switch (Type)
	{
	case EPotionType::Vida: Vida = FMath::Max(0, Vida + Amount); break;
	case EPotionType::Mana: Mana = FMath::Max(0, Mana + Amount); break;
	case EPotionType::Energia: Energia = FMath::Max(0, Energia + Amount); break;
	default: break;
	}
}

int32 FRunPersistentPowerState::CountActivePowers() const
{
	return
		static_cast<int32>(PowerUpActivoDeSala != 0 && PowerUpActivoDeSala != 1) +
		static_cast<int32>(bDobleDanyoPreparado || PowerUpActivoDeSala == 1) +
		static_cast<int32>(bAtaquesQueman) +
		static_cast<int32>(bAtaquesDesplazan) +
		static_cast<int32>(bAtaquesCriticos) +
		static_cast<int32>(bAtaquesDebilitan) +
		static_cast<int32>(bVelocidadAumentada) +
		static_cast<int32>(bTieneVidaExtra) +
		static_cast<int32>(bDanyoReducido) +
		static_cast<int32>(bProyectilesParalizan) +
		static_cast<int32>(bGuardiaDireccionalActiva) +
		static_cast<int32>(bRoboVidaActivo) +
		static_cast<int32>(bEsquivaActiva) +
		static_cast<int32>(bInversionDanyoActiva) +
		static_cast<int32>(bDashHabilitado) +
		static_cast<int32>(bAtaqueRapidoActivo) +
		static_cast<int32>(bProyectilesAutoapuntado) +
		static_cast<int32>(bProyectilesRebotan) +
		static_cast<int32>(bRecursosAlquimiaObtenidos) +
		static_cast<int32>(bAtaquesDesvianProyectiles);
}

void URunPowerPersistenceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (const USelectedProfileSaveGame* Profile = Cast<USelectedProfileSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SelectedProfileSlot, 0)))
	{
		ActiveSaveSlot = FMath::Clamp(Profile->ActiveSlot, 1, 3);
	}
	LoadPermanentProgression();
}

FString URunPowerPersistenceSubsystem::GetProgressionSlotName(const int32 SlotIndex) const
{
	return FString::Printf(TEXT("TFG_ProgresoPermanente_Slot%d"), FMath::Clamp(SlotIndex, 1, 3));
}

bool URunPowerPersistenceSubsystem::DoesSaveSlotExist(const int32 SlotIndex) const
{
	if (SlotIndex < 1 || SlotIndex > 3)
	{
		return false;
	}
	return UGameplayStatics::DoesSaveGameExist(GetProgressionSlotName(SlotIndex), 0) ||
		(SlotIndex == 1 && UGameplayStatics::DoesSaveGameExist(LegacyProgressionSlot, 0));
}

bool URunPowerPersistenceSubsystem::DeleteSaveSlot(const int32 SlotIndex)
{
	if (SlotIndex < 1 || SlotIndex > 3)
	{
		return false;
	}

	bool bDeleteSucceeded = true;
	const FString SlotName = GetProgressionSlotName(SlotIndex);
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		bDeleteSucceeded &= UGameplayStatics::DeleteGameInSlot(SlotName, 0);
	}

	// Slot 1 used the legacy single-profile file in older project versions.
	// Delete it as well, otherwise LoadPermanentProgression would migrate it
	// back and make the deleted profile appear again.
	if (SlotIndex == 1 && UGameplayStatics::DoesSaveGameExist(LegacyProgressionSlot, 0))
	{
		bDeleteSucceeded &= UGameplayStatics::DeleteGameInSlot(LegacyProgressionSlot, 0);
	}

	if (SlotIndex == ActiveSaveSlot)
	{
		PermanentProgression = Cast<UPermanentProgressionSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UPermanentProgressionSaveGame::StaticClass()));
		CollectibleInventory = FRunCollectibleInventory();
		PotionInventory = FRunPotionInventory();
		PowerState = FRunPersistentPowerState();
		bHasStoredState = false;
		bPermanentReviveAvailable = false;
		UsedNpcIndices.Reset();
	}

	if (bDeleteSucceeded)
	{
		UE_LOG(LogRunPowerPersistence, Display,
			TEXT("[GUARDADO] Datos de la partida %d eliminados."), SlotIndex);
	}
	else
	{
		UE_LOG(LogRunPowerPersistence, Error,
			TEXT("[GUARDADO] Los datos de la partida %d no pudieron eliminarse por completo."),
			SlotIndex);
	}
	return bDeleteSucceeded;
}

void URunPowerPersistenceSubsystem::SaveSelectedProfile() const
{
	USelectedProfileSaveGame* Profile = Cast<USelectedProfileSaveGame>(
		UGameplayStatics::CreateSaveGameObject(USelectedProfileSaveGame::StaticClass()));
	if (Profile)
	{
		Profile->ActiveSlot = ActiveSaveSlot;
		UGameplayStatics::SaveGameToSlot(Profile, SelectedProfileSlot, 0);
	}
}

void URunPowerPersistenceSubsystem::SelectSaveSlot(const int32 SlotIndex)
{
	ActiveSaveSlot = FMath::Clamp(SlotIndex, 1, 3);
	PermanentProgression = nullptr;
	CollectibleInventory = FRunCollectibleInventory();
	PotionInventory = FRunPotionInventory();
	PowerState = FRunPersistentPowerState();
	bHasStoredState = false;
	UsedNpcIndices.Reset();
	LoadPermanentProgression();
	SavePermanentProgression();
	SaveSelectedProfile();
	UE_LOG(LogRunPowerPersistence, Display,
		TEXT("[GUARDADO] Partida %d seleccionada y cargada."), ActiveSaveSlot);
}

void URunPowerPersistenceSubsystem::LoadPermanentProgression()
{
	const FString SlotName = GetProgressionSlotName(ActiveSaveSlot);
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		PermanentProgression = Cast<UPermanentProgressionSaveGame>(
			UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	}
	else if (ActiveSaveSlot == 1 && UGameplayStatics::DoesSaveGameExist(LegacyProgressionSlot, 0))
	{
		// Preserve existing projects by migrating the old single save into slot 1.
		PermanentProgression = Cast<UPermanentProgressionSaveGame>(
			UGameplayStatics::LoadGameFromSlot(LegacyProgressionSlot, 0));
	}

	if (!PermanentProgression)
	{
		PermanentProgression = Cast<UPermanentProgressionSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UPermanentProgressionSaveGame::StaticClass()));
	}

	PermanentProgression->StoredLight = FMath::Max(0, PermanentProgression->StoredLight);
	PermanentProgression->StoredHealthResource =
		FMath::Max(0, PermanentProgression->StoredHealthResource);
	PermanentProgression->StoredEnergyResource =
		FMath::Max(0, PermanentProgression->StoredEnergyResource);
	PermanentProgression->StoredManaResource =
		FMath::Max(0, PermanentProgression->StoredManaResource);
	PermanentProgression->StoredHealthPotions =
		FMath::Max(0, PermanentProgression->StoredHealthPotions);
	PermanentProgression->StoredEnergyPotions =
		FMath::Max(0, PermanentProgression->StoredEnergyPotions);
	PermanentProgression->StoredManaPotions =
		FMath::Max(0, PermanentProgression->StoredManaPotions);
	PermanentProgression->DamageLevel = FMath::Clamp(PermanentProgression->DamageLevel, 0, 10);
	PermanentProgression->SpeedLevel = FMath::Clamp(PermanentProgression->SpeedLevel, 0, 10);
	PermanentProgression->HealthLevel = FMath::Clamp(PermanentProgression->HealthLevel, 0, 10);
	PermanentProgression->ManaLevel = FMath::Clamp(PermanentProgression->ManaLevel, 0, 10);
	CollectibleInventory.Luz = PermanentProgression->StoredLight;
	CollectibleInventory.Vida = PermanentProgression->StoredHealthResource;
	CollectibleInventory.Energia = PermanentProgression->StoredEnergyResource;
	CollectibleInventory.Mana = PermanentProgression->StoredManaResource;
	PotionInventory.Vida = PermanentProgression->StoredHealthPotions;
	PotionInventory.Energia = PermanentProgression->StoredEnergyPotions;
	PotionInventory.Mana = PermanentProgression->StoredManaPotions;
	bPermanentReviveAvailable = PermanentProgression->bReviveUnlocked;

	UE_LOG(LogRunPowerPersistence, Display,
		TEXT("[PROGRESO PERMANENTE] Partida %d cargada. Recursos: Luz=%d, Vida=%d, Energia=%d, Mana=%d. Pociones: Vida=%d, Energia=%d, Mana=%d. Mejoras: dano=%d, velocidad=%d, vida=%d, mana=%d, regeneracion=%s, resurreccion=%s."),
		ActiveSaveSlot,
		PermanentProgression->StoredLight,
		PermanentProgression->StoredHealthResource,
		PermanentProgression->StoredEnergyResource,
		PermanentProgression->StoredManaResource,
		PermanentProgression->StoredHealthPotions,
		PermanentProgression->StoredEnergyPotions,
		PermanentProgression->StoredManaPotions,
		PermanentProgression->DamageLevel,
		PermanentProgression->SpeedLevel,
		PermanentProgression->HealthLevel,
		PermanentProgression->ManaLevel,
		PermanentProgression->bHealthRegenUnlocked ? TEXT("SI") : TEXT("NO"),
		PermanentProgression->bReviveUnlocked ? TEXT("SI") : TEXT("NO"));
}

void URunPowerPersistenceSubsystem::SavePermanentProgression()
{
	if (!PermanentProgression)
	{
		return;
	}

	PermanentProgression->StoredLight = FMath::Max(0, CollectibleInventory.Luz);
	PermanentProgression->StoredHealthResource =
		FMath::Max(0, CollectibleInventory.Vida);
	PermanentProgression->StoredEnergyResource =
		FMath::Max(0, CollectibleInventory.Energia);
	PermanentProgression->StoredManaResource =
		FMath::Max(0, CollectibleInventory.Mana);
	PermanentProgression->StoredHealthPotions =
		FMath::Max(0, PotionInventory.Vida);
	PermanentProgression->StoredEnergyPotions =
		FMath::Max(0, PotionInventory.Energia);
	PermanentProgression->StoredManaPotions =
		FMath::Max(0, PotionInventory.Mana);
	if (!UGameplayStatics::SaveGameToSlot(PermanentProgression, GetProgressionSlotName(ActiveSaveSlot), 0))
	{
		UE_LOG(LogRunPowerPersistence, Error, TEXT("[PROGRESO PERMANENTE] No se pudo guardar la partida."));
	}
}

void URunPowerPersistenceSubsystem::StorePowerState(
	const FRunPersistentPowerState& NewState)
{
	PowerState = NewState;
	bHasStoredState = true;
	UE_LOG(
		LogRunPowerPersistence,
		Display,
		TEXT("[PERSISTENCIA PODERES] Guardados %d poderes para el cambio de escena."),
		PowerState.CountActivePowers());
}

int32 URunPowerPersistenceSubsystem::GetPersistentPowerCount() const
{
	return bHasStoredState ? PowerState.CountActivePowers() : 0;
}

bool URunPowerPersistenceSubsystem::HasPersistentRoomPowerUp(
	const int32 PowerUpId) const
{
	if (!bHasStoredState)
	{
		return false;
	}

	switch (PowerUpId)
	{
	case 1:
		return PowerState.bDobleDanyoPreparado ||
			PowerState.PowerUpActivoDeSala == 1;
	case 2:
		return PowerState.bAtaquesQueman;
	case 3:
		return PowerState.bAtaquesDesplazan;
	case 4:
		return PowerState.bAtaquesCriticos;
	case 5:
		return PowerState.bAtaquesDebilitan;
	case 6:
		return PowerState.bVelocidadAumentada;
	case 7:
		return PowerState.bTieneVidaExtra;
	case 8:
		return PowerState.bDanyoReducido;
	case 9:
		return PowerState.bProyectilesParalizan;
	case 10:
		return PowerState.bGuardiaDireccionalActiva;
	case 11:
		return PowerState.PowerUpActivoDeSala == 11;
	case 12:
		return PowerState.bRoboVidaActivo;
	case 13:
		return PowerState.bEsquivaActiva;
	case 14:
		return PowerState.bInversionDanyoActiva;
	case 15:
		return PowerState.bDashHabilitado;
	case 16:
		return PowerState.PowerUpActivoDeSala == 16;
	case 17:
		return PowerState.bAtaqueRapidoActivo;
	case 18:
		return PowerState.bProyectilesAutoapuntado;
	case 19:
		return PowerState.bProyectilesRebotan;
	case 20:
		return PowerState.bRecursosAlquimiaObtenidos;
	case 21:
		return PowerState.bAtaquesDesvianProyectiles;
	default:
		return false;
	}
}

void URunPowerPersistenceSubsystem::AddCollectible(
	const EEnemyCollectibleType Type,
	const int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}

	CollectibleInventory.Add(Type, Amount);
	SavePermanentProgression();
	UE_LOG(
		LogRunPowerPersistence,
		Display,
		TEXT("[RECOLECTABLE] Guardado tipo %d: +%d. Cantidad=%d, total=%d."),
		static_cast<int32>(Type),
		Amount,
		CollectibleInventory.Get(Type),
		CollectibleInventory.Total());
}

int32 URunPowerPersistenceSubsystem::GetCollectibleCount(
	const EEnemyCollectibleType Type) const
{
	return CollectibleInventory.Get(Type);
}

int32 URunPowerPersistenceSubsystem::GetPotionCount(const EPotionType Type) const
{
	return PotionInventory.Get(Type);
}

int32 URunPowerPersistenceSubsystem::GetPotionCraftCost(const EPotionType Type) const
{
	return 3;
}

bool URunPowerPersistenceSubsystem::CanCraftPotion(const EPotionType Type) const
{
	EEnemyCollectibleType ResourceType = EEnemyCollectibleType::Vida;
	switch (Type)
	{
	case EPotionType::Vida: ResourceType = EEnemyCollectibleType::Vida; break;
	case EPotionType::Mana: ResourceType = EEnemyCollectibleType::Mana; break;
	case EPotionType::Energia: ResourceType = EEnemyCollectibleType::Energia; break;
	default: return false;
	}
	return GetCollectibleCount(ResourceType) >= GetPotionCraftCost(Type);
}

bool URunPowerPersistenceSubsystem::CraftPotion(const EPotionType Type)
{
	if (!CanCraftPotion(Type))
	{
		UE_LOG(LogRunPowerPersistence, Warning,
			TEXT("[ALQUIMIA] No hay suficientes recursos para fabricar la pocion tipo=%d."),
			static_cast<int32>(Type));
		return false;
	}

	EEnemyCollectibleType ResourceType = EEnemyCollectibleType::Vida;
	switch (Type)
	{
	case EPotionType::Vida: ResourceType = EEnemyCollectibleType::Vida; break;
	case EPotionType::Mana: ResourceType = EEnemyCollectibleType::Mana; break;
	case EPotionType::Energia: ResourceType = EEnemyCollectibleType::Energia; break;
	default: return false;
	}

	const int32 Cost = GetPotionCraftCost(Type);
	CollectibleInventory.Add(ResourceType, -Cost);
	PotionInventory.Add(Type, 1);
	SavePermanentProgression();
	UE_LOG(LogRunPowerPersistence, Display,
		TEXT("[ALQUIMIA] Pocion fabricada. Tipo=%d, coste=%d, recursos restantes=%d, pociones=%d."),
		static_cast<int32>(Type), Cost, GetCollectibleCount(ResourceType), GetPotionCount(Type));
	return true;
}

bool URunPowerPersistenceSubsystem::ConsumePotion(const EPotionType Type)
{
	if (GetPotionCount(Type) <= 0)
	{
		return false;
	}
	PotionInventory.Add(Type, -1);
	SavePermanentProgression();
	UE_LOG(LogRunPowerPersistence, Display,
		TEXT("[ALQUIMIA] Pocion consumida. Tipo=%d, restantes=%d."),
		static_cast<int32>(Type), GetPotionCount(Type));
	return true;
}

int32 URunPowerPersistenceSubsystem::GetLightCount() const
{
	return CollectibleInventory.Luz;
}

int32 URunPowerPersistenceSubsystem::GetPermanentUpgradeLevel(const EPermanentUpgradeType Type) const
{
	if (!PermanentProgression)
	{
		return 0;
	}

	switch (Type)
	{
	case EPermanentUpgradeType::Danyo: return PermanentProgression->DamageLevel;
	case EPermanentUpgradeType::Velocidad: return PermanentProgression->SpeedLevel;
	case EPermanentUpgradeType::Vida: return PermanentProgression->HealthLevel;
	case EPermanentUpgradeType::Mana: return PermanentProgression->ManaLevel;
	case EPermanentUpgradeType::RegeneracionVida: return PermanentProgression->bHealthRegenUnlocked ? 1 : 0;
	case EPermanentUpgradeType::Resurreccion: return PermanentProgression->bReviveUnlocked ? 1 : 0;
	default: return 0;
	}
}

int32 URunPowerPersistenceSubsystem::GetPermanentUpgradeMaxLevel(const EPermanentUpgradeType Type) const
{
	return Type == EPermanentUpgradeType::RegeneracionVida ||
		Type == EPermanentUpgradeType::Resurreccion ? 1 : 10;
}

int32 URunPowerPersistenceSubsystem::GetPermanentUpgradeCost(const EPermanentUpgradeType Type) const
{
	if (Type == EPermanentUpgradeType::RegeneracionVida)
	{
		return 30;
	}
	if (Type == EPermanentUpgradeType::Resurreccion)
	{
		return 40;
	}
	return 2 + GetPermanentUpgradeLevel(Type) * 2;
}

bool URunPowerPersistenceSubsystem::IsPermanentUpgradeUnlocked(const EPermanentUpgradeType Type) const
{
	return GetPermanentUpgradeLevel(Type) >= GetPermanentUpgradeMaxLevel(Type);
}

bool URunPowerPersistenceSubsystem::CanPurchasePermanentUpgrade(const EPermanentUpgradeType Type) const
{
	return !IsPermanentUpgradeUnlocked(Type) && GetLightCount() >= GetPermanentUpgradeCost(Type);
}

bool URunPowerPersistenceSubsystem::PurchasePermanentUpgrade(const EPermanentUpgradeType Type)
{
	if (!PermanentProgression || !CanPurchasePermanentUpgrade(Type))
	{
		UE_LOG(LogRunPowerPersistence, Warning,
			TEXT("[MEJORA PERMANENTE] Compra rechazada. Tipo=%d, Luz=%d, coste=%d, nivel=%d/%d."),
			static_cast<int32>(Type), GetLightCount(), GetPermanentUpgradeCost(Type),
			GetPermanentUpgradeLevel(Type), GetPermanentUpgradeMaxLevel(Type));
		return false;
	}

	const int32 Cost = GetPermanentUpgradeCost(Type);
	CollectibleInventory.Luz -= Cost;
	switch (Type)
	{
	case EPermanentUpgradeType::Danyo: ++PermanentProgression->DamageLevel; break;
	case EPermanentUpgradeType::Velocidad: ++PermanentProgression->SpeedLevel; break;
	case EPermanentUpgradeType::Vida: ++PermanentProgression->HealthLevel; break;
	case EPermanentUpgradeType::Mana: ++PermanentProgression->ManaLevel; break;
	case EPermanentUpgradeType::RegeneracionVida: PermanentProgression->bHealthRegenUnlocked = true; break;
	case EPermanentUpgradeType::Resurreccion:
		PermanentProgression->bReviveUnlocked = true;
		bPermanentReviveAvailable = true;
		break;
	default: break;
	}

	SavePermanentProgression();
	UE_LOG(LogRunPowerPersistence, Display,
		TEXT("[MEJORA PERMANENTE] Comprada tipo=%d por %d Luz. Nivel=%d/%d, Luz restante=%d."),
		static_cast<int32>(Type), Cost, GetPermanentUpgradeLevel(Type),
		GetPermanentUpgradeMaxLevel(Type), GetLightCount());
	return true;
}

float URunPowerPersistenceSubsystem::GetPermanentDamageMultiplier() const
{
	return 1.0f + GetPermanentUpgradeLevel(EPermanentUpgradeType::Danyo) * 0.05f;
}

float URunPowerPersistenceSubsystem::GetPermanentSpeedMultiplier() const
{
	return 1.0f + GetPermanentUpgradeLevel(EPermanentUpgradeType::Velocidad) * 0.03f;
}

float URunPowerPersistenceSubsystem::GetPermanentHealthBonus() const
{
	return GetPermanentUpgradeLevel(EPermanentUpgradeType::Vida) * 10.0f;
}

float URunPowerPersistenceSubsystem::GetPermanentManaBonus() const
{
	return GetPermanentUpgradeLevel(EPermanentUpgradeType::Mana) * 10.0f;
}

float URunPowerPersistenceSubsystem::GetPermanentHealthRegenPerSecond() const
{
	return PermanentProgression && PermanentProgression->bHealthRegenUnlocked ? 2.0f : 0.0f;
}

bool URunPowerPersistenceSubsystem::IsPermanentReviveAvailable() const
{
	return PermanentProgression && PermanentProgression->bReviveUnlocked && bPermanentReviveAvailable;
}

void URunPowerPersistenceSubsystem::ConsumePermanentRevive()
{
	if (bPermanentReviveAvailable)
	{
		bPermanentReviveAvailable = false;
		UE_LOG(LogRunPowerPersistence, Display,
			TEXT("[MEJORA PERMANENTE] Resurreccion consumida durante esta partida."));
	}
}

void URunPowerPersistenceSubsystem::MarkFirstReturnDialoguePending()
{
	if (!PermanentProgression || PermanentProgression->bFirstReturnDialogueSeen)
	{
		return;
	}

	PermanentProgression->bFirstReturnDialoguePending = true;
	SavePermanentProgression();
	UE_LOG(
		LogRunPowerPersistence,
		Display,
		TEXT("[DIALOGO ARTURO] Primer regreso pendiente para la partida %d."),
		ActiveSaveSlot);
}

bool URunPowerPersistenceSubsystem::ShouldShowFirstReturnDialogue() const
{
	return PermanentProgression &&
		!PermanentProgression->bFirstReturnDialogueSeen &&
		PermanentProgression->bFirstReturnDialoguePending;
}

void URunPowerPersistenceSubsystem::CompleteFirstReturnDialogue()
{
	if (!ShouldShowFirstReturnDialogue())
	{
		return;
	}

	PermanentProgression->bFirstReturnDialoguePending = false;
	PermanentProgression->bFirstReturnDialogueSeen = true;
	SavePermanentProgression();
	UE_LOG(
		LogRunPowerPersistence,
		Display,
		TEXT("[DIALOGO ARTURO] Primer regreso consumido y guardado en la partida %d."),
		ActiveSaveSlot);
}

int32 URunPowerPersistenceSubsystem::ChooseUniqueNpcIndex(
	const int32 SelectionSeed,
	const int32 NpcCount)
{
	if (NpcCount <= 0)
	{
		return INDEX_NONE;
	}

	UsedNpcIndices.RemoveAll(
		[NpcCount](const int32 Index)
		{
			return Index < 0 || Index >= NpcCount;
		});

	if (UsedNpcIndices.Num() >= NpcCount)
	{
		UsedNpcIndices.Reset();
		UE_LOG(
			LogRunPowerPersistence,
			Display,
			TEXT("[NPC UNICO] Todos los personajes aparecieron; comienza un nuevo ciclo."));
	}

	TArray<int32> AvailableIndices;
	AvailableIndices.Reserve(NpcCount - UsedNpcIndices.Num());
	for (int32 Index = 0; Index < NpcCount; ++Index)
	{
		if (!UsedNpcIndices.Contains(Index))
		{
			AvailableIndices.Add(Index);
		}
	}

	FRandomStream RandomStream(
		HashCombineFast(
			static_cast<uint32>(SelectionSeed),
			static_cast<uint32>(UsedNpcIndices.Num() + 1)));
	const int32 ChosenIndex =
		AvailableIndices[RandomStream.RandRange(0, AvailableIndices.Num() - 1)];
	UsedNpcIndices.Add(ChosenIndex);
	return ChosenIndex;
}

void URunPowerPersistenceSubsystem::ResetPersistentPowers()
{
	PowerState = FRunPersistentPowerState();
	CurrentRunRoomNumber = 1;
	UsedNpcIndices.Reset();
	// Enemy resources belong to the permanent player inventory. Starting a
	// fresh run resets powers, but never discards collectibles or crafted
	// potions from the player's permanent inventory.
	SavePermanentProgression();
	bPermanentReviveAvailable = PermanentProgression && PermanentProgression->bReviveUnlocked;
	bHasStoredState = false;
	UE_LOG(
		LogRunPowerPersistence,
		Display,
		TEXT("[PERSISTENCIA PODERES] Nueva partida iniciada. Recursos conservados: Luz=%d, Vida=%d, Energia=%d, Mana=%d. Pociones conservadas: Vida=%d, Energia=%d, Mana=%d."),
		CollectibleInventory.Luz,
		CollectibleInventory.Vida,
		CollectibleInventory.Energia,
		CollectibleInventory.Mana,
		PotionInventory.Vida,
		PotionInventory.Energia,
		PotionInventory.Mana);
}

void URunPowerPersistenceSubsystem::SetCurrentRunRoomNumber(
	const int32 RoomNumber)
{
	CurrentRunRoomNumber = FMath::Max(1, RoomNumber);
}
