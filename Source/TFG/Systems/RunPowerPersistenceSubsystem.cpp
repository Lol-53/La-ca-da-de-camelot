#include "Systems/RunPowerPersistenceSubsystem.h"

#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRunPowerPersistence, Log, All);

namespace RunProgressionConstants {
constexpr int32 MinimumSaveSlot = 1;
constexpr int32 MaximumSaveSlot = 3;
constexpr int32 PotionCraftCost = 3;
constexpr int32 SingleUnlockMaximumLevel = 1;
constexpr int32 StandardUpgradeMaximumLevel = 10;
constexpr int32 HealthRegenerationUnlockCost = 30;
constexpr int32 ResurrectionUnlockCost = 40;
constexpr int32 StandardUpgradeBaseCost = 2;
constexpr int32 StandardUpgradeCostPerLevel = 2;
} // namespace RunProgressionConstants

const FString URunPowerPersistenceSubsystem::LegacyProgressionSlot(
    TEXT("TFG_ProgresoPermanente"));
const FString URunPowerPersistenceSubsystem::SelectedProfileSlot(
    TEXT("TFG_PerfilActivo"));

int32 FRunCollectibleInventory::Get(const EEnemyCollectibleType Type) const {
  int32 Count = 0;
  switch (Type) {
  case EEnemyCollectibleType::Luz:
    Count = Luz;
    break;
  case EEnemyCollectibleType::Vida:
    Count = Vida;
    break;
  case EEnemyCollectibleType::Energia:
    Count = Energia;
    break;
  case EEnemyCollectibleType::Mana:
    Count = Mana;
    break;
  default:
    break;
  }
  return Count;
}

void FRunCollectibleInventory::Add(const EEnemyCollectibleType Type,
                                   const int32 Amount) {
  switch (Type) {
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

int32 FRunPotionInventory::Get(const EPotionType Type) const {
  int32 Count = 0;
  switch (Type) {
  case EPotionType::Vida:
    Count = Vida;
    break;
  case EPotionType::Mana:
    Count = Mana;
    break;
  case EPotionType::Energia:
    Count = Energia;
    break;
  default:
    break;
  }
  return Count;
}

void FRunPotionInventory::Add(const EPotionType Type, const int32 Amount) {
  switch (Type) {
  case EPotionType::Vida:
    Vida = FMath::Max(0, Vida + Amount);
    break;
  case EPotionType::Mana:
    Mana = FMath::Max(0, Mana + Amount);
    break;
  case EPotionType::Energia:
    Energia = FMath::Max(0, Energia + Amount);
    break;
  default:
    break;
  }
}

int32 FRunPersistentPowerState::CountActivePowers() const {
  return static_cast<int32>(PowerUpActivoDeSala != 0 &&
                            PowerUpActivoDeSala != 1) +
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

void URunPowerPersistenceSubsystem::Initialize(
    FSubsystemCollectionBase &Collection) {
  Super::Initialize(Collection);
  if (const USelectedProfileSaveGame *Profile = Cast<USelectedProfileSaveGame>(
          UGameplayStatics::LoadGameFromSlot(SelectedProfileSlot, 0))) {
    ActiveSaveSlot = FMath::Clamp(Profile->ActiveSlot, 1, 3);
  }
  LoadPermanentProgression();
}

FString URunPowerPersistenceSubsystem::GetProgressionSlotName(
    const int32 SlotIndex) const {
  return FString::Printf(
      TEXT("TFG_ProgresoPermanente_Slot%d"),
      FMath::Clamp(SlotIndex, RunProgressionConstants::MinimumSaveSlot,
                   RunProgressionConstants::MaximumSaveSlot));
}

bool URunPowerPersistenceSubsystem::DoesSaveSlotExist(
    const int32 SlotIndex) const {
  const bool bValidSlot =
      SlotIndex >= RunProgressionConstants::MinimumSaveSlot &&
      SlotIndex <= RunProgressionConstants::MaximumSaveSlot;
  bool bExists = false;
  if (bValidSlot) {
    bExists = UGameplayStatics::DoesSaveGameExist(
                  GetProgressionSlotName(SlotIndex), 0) ||
              (SlotIndex == RunProgressionConstants::MinimumSaveSlot &&
               UGameplayStatics::DoesSaveGameExist(LegacyProgressionSlot, 0));
  }
  return bExists;
}

bool URunPowerPersistenceSubsystem::DeleteSaveSlot(const int32 SlotIndex) {
  const bool bValidSlot =
      SlotIndex >= RunProgressionConstants::MinimumSaveSlot &&
      SlotIndex <= RunProgressionConstants::MaximumSaveSlot;
  bool bDeleteSucceeded = false;
  if (bValidSlot) {
    bDeleteSucceeded = true;
    const FString SlotName = GetProgressionSlotName(SlotIndex);
    if (UGameplayStatics::DoesSaveGameExist(SlotName, 0)) {
      bDeleteSucceeded &= UGameplayStatics::DeleteGameInSlot(SlotName, 0);
    }

    // Slot 1 used the legacy single-profile file in older project versions.
    // Delete it as well, otherwise LoadPermanentProgression would migrate it
    // back and make the deleted profile appear again.
    if (SlotIndex == RunProgressionConstants::MinimumSaveSlot &&
        UGameplayStatics::DoesSaveGameExist(LegacyProgressionSlot, 0)) {
      bDeleteSucceeded &=
          UGameplayStatics::DeleteGameInSlot(LegacyProgressionSlot, 0);
    }

    if (SlotIndex == ActiveSaveSlot) {
      PermanentProgression = Cast<UPermanentProgressionSaveGame>(
          UGameplayStatics::CreateSaveGameObject(
              UPermanentProgressionSaveGame::StaticClass()));
      CollectibleInventory = FRunCollectibleInventory();
      PotionInventory = FRunPotionInventory();
      PowerState = FRunPersistentPowerState();
      bHasStoredState = false;
      bPermanentReviveAvailable = false;
      UsedNpcIndices.Reset();
    }

    if (bDeleteSucceeded) {
      UE_LOG(LogRunPowerPersistence, Display,
             TEXT("[GUARDADO] Datos de la partida %d eliminados."), SlotIndex);
    } else {
      UE_LOG(LogRunPowerPersistence, Error,
             TEXT("[GUARDADO] Los datos de la partida %d no pudieron "
                  "eliminarse por completo."),
             SlotIndex);
    }
  }
  return bDeleteSucceeded;
}

void URunPowerPersistenceSubsystem::SaveSelectedProfile() const {
  USelectedProfileSaveGame *Profile =
      Cast<USelectedProfileSaveGame>(UGameplayStatics::CreateSaveGameObject(
          USelectedProfileSaveGame::StaticClass()));
  if (Profile) {
    Profile->ActiveSlot = ActiveSaveSlot;
    UGameplayStatics::SaveGameToSlot(Profile, SelectedProfileSlot, 0);
  }
}

void URunPowerPersistenceSubsystem::SelectSaveSlot(const int32 SlotIndex) {
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

void URunPowerPersistenceSubsystem::LoadPermanentProgression() {
  const FString SlotName = GetProgressionSlotName(ActiveSaveSlot);
  if (UGameplayStatics::DoesSaveGameExist(SlotName, 0)) {
    PermanentProgression = Cast<UPermanentProgressionSaveGame>(
        UGameplayStatics::LoadGameFromSlot(SlotName, 0));
  } else if (ActiveSaveSlot == 1 &&
             UGameplayStatics::DoesSaveGameExist(LegacyProgressionSlot, 0)) {
    // Preserve existing projects by migrating the old single save into slot 1.
    PermanentProgression = Cast<UPermanentProgressionSaveGame>(
        UGameplayStatics::LoadGameFromSlot(LegacyProgressionSlot, 0));
  }

  if (!PermanentProgression) {
    PermanentProgression = Cast<UPermanentProgressionSaveGame>(
        UGameplayStatics::CreateSaveGameObject(
            UPermanentProgressionSaveGame::StaticClass()));
  }

  PermanentProgression->StoredLight =
      FMath::Max(0, PermanentProgression->StoredLight);
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
  PermanentProgression->DamageLevel =
      FMath::Clamp(PermanentProgression->DamageLevel, 0, 10);
  PermanentProgression->SpeedLevel =
      FMath::Clamp(PermanentProgression->SpeedLevel, 0, 10);
  PermanentProgression->HealthLevel =
      FMath::Clamp(PermanentProgression->HealthLevel, 0, 10);
  PermanentProgression->ManaLevel =
      FMath::Clamp(PermanentProgression->ManaLevel, 0, 10);
  CollectibleInventory.Luz = PermanentProgression->StoredLight;
  CollectibleInventory.Vida = PermanentProgression->StoredHealthResource;
  CollectibleInventory.Energia = PermanentProgression->StoredEnergyResource;
  CollectibleInventory.Mana = PermanentProgression->StoredManaResource;
  PotionInventory.Vida = PermanentProgression->StoredHealthPotions;
  PotionInventory.Energia = PermanentProgression->StoredEnergyPotions;
  PotionInventory.Mana = PermanentProgression->StoredManaPotions;
  bPermanentReviveAvailable = PermanentProgression->bReviveUnlocked;

  UE_LOG(LogRunPowerPersistence, Display,
         TEXT("[PROGRESO PERMANENTE] Partida %d cargada. Recursos: Luz=%d, "
              "Vida=%d, Energia=%d, Mana=%d. Pociones: Vida=%d, Energia=%d, "
              "Mana=%d. Mejoras: dano=%d, velocidad=%d, vida=%d, mana=%d, "
              "regeneracion=%s, resurreccion=%s."),
         ActiveSaveSlot, PermanentProgression->StoredLight,
         PermanentProgression->StoredHealthResource,
         PermanentProgression->StoredEnergyResource,
         PermanentProgression->StoredManaResource,
         PermanentProgression->StoredHealthPotions,
         PermanentProgression->StoredEnergyPotions,
         PermanentProgression->StoredManaPotions,
         PermanentProgression->DamageLevel, PermanentProgression->SpeedLevel,
         PermanentProgression->HealthLevel, PermanentProgression->ManaLevel,
         PermanentProgression->bHealthRegenUnlocked ? TEXT("SI") : TEXT("NO"),
         PermanentProgression->bReviveUnlocked ? TEXT("SI") : TEXT("NO"));
}

void URunPowerPersistenceSubsystem::SavePermanentProgression() {
  if (!PermanentProgression) {
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
  PermanentProgression->StoredManaPotions = FMath::Max(0, PotionInventory.Mana);
  if (!UGameplayStatics::SaveGameToSlot(
          PermanentProgression, GetProgressionSlotName(ActiveSaveSlot), 0)) {
    UE_LOG(LogRunPowerPersistence, Error,
           TEXT("[PROGRESO PERMANENTE] No se pudo guardar la partida."));
  }
}

void URunPowerPersistenceSubsystem::StorePowerState(
    const FRunPersistentPowerState &NewState) {
  PowerState = NewState;
  bHasStoredState = true;
  UE_LOG(LogRunPowerPersistence, Display,
         TEXT("[PERSISTENCIA PODERES] Guardados %d poderes para el cambio de "
              "escena."),
         PowerState.CountActivePowers());
}

int32 URunPowerPersistenceSubsystem::GetPersistentPowerCount() const {
  return bHasStoredState ? PowerState.CountActivePowers() : 0;
}

bool URunPowerPersistenceSubsystem::HasPersistentRoomPowerUp(
    const int32 PowerUpId) const {
  bool bHasPowerUp = false;
  if (bHasStoredState) {
    switch (PowerUpId) {
    case 1:
      bHasPowerUp = PowerState.bDobleDanyoPreparado ||
                    PowerState.PowerUpActivoDeSala == 1;
      break;
    case 2: bHasPowerUp = PowerState.bAtaquesQueman; break;
    case 3: bHasPowerUp = PowerState.bAtaquesDesplazan; break;
    case 4: bHasPowerUp = PowerState.bAtaquesCriticos; break;
    case 5: bHasPowerUp = PowerState.bAtaquesDebilitan; break;
    case 6: bHasPowerUp = PowerState.bVelocidadAumentada; break;
    case 7: bHasPowerUp = PowerState.bTieneVidaExtra; break;
    case 8: bHasPowerUp = PowerState.bDanyoReducido; break;
    case 9: bHasPowerUp = PowerState.bProyectilesParalizan; break;
    case 10: bHasPowerUp = PowerState.bGuardiaDireccionalActiva; break;
    case 11: bHasPowerUp = PowerState.PowerUpActivoDeSala == 11; break;
    case 12: bHasPowerUp = PowerState.bRoboVidaActivo; break;
    case 13: bHasPowerUp = PowerState.bEsquivaActiva; break;
    case 14: bHasPowerUp = PowerState.bInversionDanyoActiva; break;
    case 15: bHasPowerUp = PowerState.bDashHabilitado; break;
    case 16: bHasPowerUp = PowerState.PowerUpActivoDeSala == 16; break;
    case 17: bHasPowerUp = PowerState.bAtaqueRapidoActivo; break;
    case 18: bHasPowerUp = PowerState.bProyectilesAutoapuntado; break;
    case 19: bHasPowerUp = PowerState.bProyectilesRebotan; break;
    case 20: bHasPowerUp = PowerState.bRecursosAlquimiaObtenidos; break;
    case 21: bHasPowerUp = PowerState.bAtaquesDesvianProyectiles; break;
    default: bHasPowerUp = false; break;
    }
  }
  return bHasPowerUp;
}

void URunPowerPersistenceSubsystem::AddCollectible(
    const EEnemyCollectibleType Type, const int32 Amount) {
  if (Amount <= 0) {
    return;
  }

  CollectibleInventory.Add(Type, Amount);
  SavePermanentProgression();
  UE_LOG(LogRunPowerPersistence, Display,
         TEXT("[RECOLECTABLE] Guardado tipo %d: +%d. Cantidad=%d, total=%d."),
         static_cast<int32>(Type), Amount, CollectibleInventory.Get(Type),
         CollectibleInventory.Total());
}

int32 URunPowerPersistenceSubsystem::GetCollectibleCount(
    const EEnemyCollectibleType Type) const {
  return CollectibleInventory.Get(Type);
}

int32 URunPowerPersistenceSubsystem::GetPotionCount(
    const EPotionType Type) const {
  return PotionInventory.Get(Type);
}

int32 URunPowerPersistenceSubsystem::GetPotionCraftCost(
    const EPotionType Type) const {
  return RunProgressionConstants::PotionCraftCost;
}

bool URunPowerPersistenceSubsystem::CanCraftPotion(
    const EPotionType Type) const {
  EEnemyCollectibleType ResourceType = EEnemyCollectibleType::Vida;
  bool bValidPotionType = true;
  switch (Type) {
  case EPotionType::Vida:
    ResourceType = EEnemyCollectibleType::Vida;
    break;
  case EPotionType::Mana:
    ResourceType = EEnemyCollectibleType::Mana;
    break;
  case EPotionType::Energia:
    ResourceType = EEnemyCollectibleType::Energia;
    break;
  default:
    bValidPotionType = false;
    break;
  }
  const bool bCanCraft =
      bValidPotionType &&
      GetCollectibleCount(ResourceType) >= GetPotionCraftCost(Type);
  return bCanCraft;
}

bool URunPowerPersistenceSubsystem::CraftPotion(const EPotionType Type) {
  bool bCrafted = false;
  if (CanCraftPotion(Type)) {
    EEnemyCollectibleType ResourceType = EEnemyCollectibleType::Vida;
    bool bValidPotionType = true;
    switch (Type) {
    case EPotionType::Vida:
      ResourceType = EEnemyCollectibleType::Vida;
      break;
    case EPotionType::Mana:
      ResourceType = EEnemyCollectibleType::Mana;
      break;
    case EPotionType::Energia:
      ResourceType = EEnemyCollectibleType::Energia;
      break;
    default:
      bValidPotionType = false;
      break;
    }

    if (bValidPotionType) {
      const int32 Cost = GetPotionCraftCost(Type);
      CollectibleInventory.Add(ResourceType, -Cost);
      PotionInventory.Add(Type, 1);
      SavePermanentProgression();
      UE_LOG(LogRunPowerPersistence, Display,
             TEXT("[ALQUIMIA] Pocion fabricada. Tipo=%d, coste=%d, recursos "
                  "restantes=%d, pociones=%d."),
             static_cast<int32>(Type), Cost, GetCollectibleCount(ResourceType),
             GetPotionCount(Type));
      bCrafted = true;
    }
  } else {
    UE_LOG(LogRunPowerPersistence, Warning,
           TEXT("[ALQUIMIA] No hay suficientes recursos para fabricar la "
                "pocion tipo=%d."),
           static_cast<int32>(Type));
  }
  return bCrafted;
}

bool URunPowerPersistenceSubsystem::ConsumePotion(const EPotionType Type) {
  const bool bConsumed = GetPotionCount(Type) > 0;
  if (bConsumed) {
    PotionInventory.Add(Type, -1);
    SavePermanentProgression();
    UE_LOG(LogRunPowerPersistence, Display,
           TEXT("[ALQUIMIA] Pocion consumida. Tipo=%d, restantes=%d."),
           static_cast<int32>(Type), GetPotionCount(Type));
  }
  return bConsumed;
}

int32 URunPowerPersistenceSubsystem::GetLightCount() const {
  return CollectibleInventory.Luz;
}

int32 URunPowerPersistenceSubsystem::GetPermanentUpgradeLevel(
    const EPermanentUpgradeType Type) const {
  int32 Level = 0;
  if (PermanentProgression) {
    switch (Type) {
    case EPermanentUpgradeType::Danyo:
      Level = PermanentProgression->DamageLevel;
      break;
    case EPermanentUpgradeType::Velocidad:
      Level = PermanentProgression->SpeedLevel;
      break;
    case EPermanentUpgradeType::Vida:
      Level = PermanentProgression->HealthLevel;
      break;
    case EPermanentUpgradeType::Mana:
      Level = PermanentProgression->ManaLevel;
      break;
    case EPermanentUpgradeType::RegeneracionVida:
      Level = PermanentProgression->bHealthRegenUnlocked ? 1 : 0;
      break;
    case EPermanentUpgradeType::Resurreccion:
      Level = PermanentProgression->bReviveUnlocked ? 1 : 0;
      break;
    default:
      break;
    }
  }
  return Level;
}

int32 URunPowerPersistenceSubsystem::GetPermanentUpgradeMaxLevel(
    const EPermanentUpgradeType Type) const {
  return Type == EPermanentUpgradeType::RegeneracionVida ||
                 Type == EPermanentUpgradeType::Resurreccion
             ? RunProgressionConstants::SingleUnlockMaximumLevel
             : RunProgressionConstants::StandardUpgradeMaximumLevel;
}

int32 URunPowerPersistenceSubsystem::GetPermanentUpgradeCost(
    const EPermanentUpgradeType Type) const {
  int32 Cost = RunProgressionConstants::StandardUpgradeBaseCost +
               GetPermanentUpgradeLevel(Type) *
                   RunProgressionConstants::StandardUpgradeCostPerLevel;
  if (Type == EPermanentUpgradeType::RegeneracionVida) {
    Cost = RunProgressionConstants::HealthRegenerationUnlockCost;
  } else if (Type == EPermanentUpgradeType::Resurreccion) {
    Cost = RunProgressionConstants::ResurrectionUnlockCost;
  }
  return Cost;
}

bool URunPowerPersistenceSubsystem::IsPermanentUpgradeUnlocked(
    const EPermanentUpgradeType Type) const {
  return GetPermanentUpgradeLevel(Type) >= GetPermanentUpgradeMaxLevel(Type);
}

bool URunPowerPersistenceSubsystem::CanPurchasePermanentUpgrade(
    const EPermanentUpgradeType Type) const {
  return !IsPermanentUpgradeUnlocked(Type) &&
         GetLightCount() >= GetPermanentUpgradeCost(Type);
}

bool URunPowerPersistenceSubsystem::PurchasePermanentUpgrade(
    const EPermanentUpgradeType Type) {
  const bool bPurchased =
      PermanentProgression && CanPurchasePermanentUpgrade(Type);
  if (bPurchased) {
    const int32 Cost = GetPermanentUpgradeCost(Type);
    CollectibleInventory.Luz -= Cost;
    switch (Type) {
    case EPermanentUpgradeType::Danyo:
      ++PermanentProgression->DamageLevel;
      break;
    case EPermanentUpgradeType::Velocidad:
      ++PermanentProgression->SpeedLevel;
      break;
    case EPermanentUpgradeType::Vida:
      ++PermanentProgression->HealthLevel;
      break;
    case EPermanentUpgradeType::Mana:
      ++PermanentProgression->ManaLevel;
      break;
    case EPermanentUpgradeType::RegeneracionVida:
      PermanentProgression->bHealthRegenUnlocked = true;
      break;
    case EPermanentUpgradeType::Resurreccion:
      PermanentProgression->bReviveUnlocked = true;
      bPermanentReviveAvailable = true;
      break;
    default:
      break;
    }

    SavePermanentProgression();
    UE_LOG(LogRunPowerPersistence, Display,
           TEXT("[MEJORA PERMANENTE] Comprada tipo=%d por %d Luz. Nivel=%d/%d, "
                "Luz restante=%d."),
           static_cast<int32>(Type), Cost, GetPermanentUpgradeLevel(Type),
           GetPermanentUpgradeMaxLevel(Type), GetLightCount());
  } else {
    UE_LOG(LogRunPowerPersistence, Warning,
           TEXT("[MEJORA PERMANENTE] Compra rechazada. Tipo=%d, Luz=%d, "
                "coste=%d, nivel=%d/%d."),
           static_cast<int32>(Type), GetLightCount(),
           GetPermanentUpgradeCost(Type), GetPermanentUpgradeLevel(Type),
           GetPermanentUpgradeMaxLevel(Type));
  }
  return bPurchased;
}

float URunPowerPersistenceSubsystem::GetPermanentDamageMultiplier() const {
  return 1.0f + GetPermanentUpgradeLevel(EPermanentUpgradeType::Danyo) * 0.05f;
}

float URunPowerPersistenceSubsystem::GetPermanentSpeedMultiplier() const {
  return 1.0f +
         GetPermanentUpgradeLevel(EPermanentUpgradeType::Velocidad) * 0.03f;
}

float URunPowerPersistenceSubsystem::GetPermanentHealthBonus() const {
  return GetPermanentUpgradeLevel(EPermanentUpgradeType::Vida) * 10.0f;
}

float URunPowerPersistenceSubsystem::GetPermanentManaBonus() const {
  return GetPermanentUpgradeLevel(EPermanentUpgradeType::Mana) * 10.0f;
}

float URunPowerPersistenceSubsystem::GetPermanentHealthRegenPerSecond() const {
  return PermanentProgression && PermanentProgression->bHealthRegenUnlocked
             ? 2.0f
             : 0.0f;
}

bool URunPowerPersistenceSubsystem::IsPermanentReviveAvailable() const {
  return PermanentProgression && PermanentProgression->bReviveUnlocked &&
         bPermanentReviveAvailable;
}

void URunPowerPersistenceSubsystem::ConsumePermanentRevive() {
  if (bPermanentReviveAvailable) {
    bPermanentReviveAvailable = false;
    UE_LOG(LogRunPowerPersistence, Display,
           TEXT("[MEJORA PERMANENTE] Resurreccion consumida durante esta "
                "partida."));
  }
}

void URunPowerPersistenceSubsystem::MarkFirstReturnDialoguePending() {
  if (!PermanentProgression || PermanentProgression->bFirstReturnDialogueSeen) {
    return;
  }

  PermanentProgression->bFirstReturnDialoguePending = true;
  SavePermanentProgression();
  UE_LOG(LogRunPowerPersistence, Display,
         TEXT("[DIALOGO ARTURO] Primer regreso pendiente para la partida %d."),
         ActiveSaveSlot);
}

bool URunPowerPersistenceSubsystem::ShouldShowFirstReturnDialogue() const {
  return PermanentProgression &&
         !PermanentProgression->bFirstReturnDialogueSeen &&
         PermanentProgression->bFirstReturnDialoguePending;
}

void URunPowerPersistenceSubsystem::CompleteFirstReturnDialogue() {
  if (!ShouldShowFirstReturnDialogue()) {
    return;
  }

  PermanentProgression->bFirstReturnDialoguePending = false;
  PermanentProgression->bFirstReturnDialogueSeen = true;
  SavePermanentProgression();
  UE_LOG(LogRunPowerPersistence, Display,
         TEXT("[DIALOGO ARTURO] Primer regreso consumido y guardado en la "
              "partida %d."),
         ActiveSaveSlot);
}

int32 URunPowerPersistenceSubsystem::ChooseUniqueNpcIndex(
    const int32 SelectionSeed, const int32 NpcCount) {
  int32 ChosenIndex = INDEX_NONE;
  if (NpcCount > 0) {
    UsedNpcIndices.RemoveAll([NpcCount](const int32 Index) {
      return Index < 0 || Index >= NpcCount;
    });

    if (UsedNpcIndices.Num() >= NpcCount) {
      UsedNpcIndices.Reset();
      UE_LOG(LogRunPowerPersistence, Display,
             TEXT("[NPC UNICO] Todos los personajes aparecieron; comienza un "
                  "nuevo ciclo."));
    }

    TArray<int32> AvailableIndices;
    AvailableIndices.Reserve(NpcCount - UsedNpcIndices.Num());
    for (int32 Index = 0; Index < NpcCount; ++Index) {
      if (!UsedNpcIndices.Contains(Index)) {
        AvailableIndices.Add(Index);
      }
    }

    FRandomStream RandomStream(
        HashCombineFast(static_cast<uint32>(SelectionSeed),
                        static_cast<uint32>(UsedNpcIndices.Num() + 1)));
    ChosenIndex =
        AvailableIndices[RandomStream.RandRange(0, AvailableIndices.Num() - 1)];
    UsedNpcIndices.Add(ChosenIndex);
  }
  return ChosenIndex;
}

void URunPowerPersistenceSubsystem::ResetPersistentPowers() {
  PowerState = FRunPersistentPowerState();
  CurrentRunRoomNumber = 1;
  UsedNpcIndices.Reset();
  // Enemy resources belong to the permanent player inventory. Starting a
  // fresh run resets powers, but never discards collectibles or crafted
  // potions from the player's permanent inventory.
  SavePermanentProgression();
  bPermanentReviveAvailable =
      PermanentProgression && PermanentProgression->bReviveUnlocked;
  bHasStoredState = false;
  UE_LOG(LogRunPowerPersistence, Display,
         TEXT("[PERSISTENCIA PODERES] Nueva partida iniciada. Recursos "
              "conservados: Luz=%d, Vida=%d, Energia=%d, Mana=%d. Pociones "
              "conservadas: Vida=%d, Energia=%d, Mana=%d."),
         CollectibleInventory.Luz, CollectibleInventory.Vida,
         CollectibleInventory.Energia, CollectibleInventory.Mana,
         PotionInventory.Vida, PotionInventory.Energia, PotionInventory.Mana);
}

void URunPowerPersistenceSubsystem::SetCurrentRunRoomNumber(
    const int32 RoomNumber) {
  CurrentRunRoomNumber = FMath::Max(1, RoomNumber);
}
