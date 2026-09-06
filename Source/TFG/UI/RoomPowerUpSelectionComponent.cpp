#include "UI/RoomPowerUpSelectionComponent.h"

#include "TFGCharacter.h"
#include "UI/RoomPowerUpSelectionWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRoomPowerUpSelection, Log, All);

namespace RoomPowerUps
{
	struct FDescription
	{
		int32 Id;
		const TCHAR* Title;
		const TCHAR* Description;
	};

	static const FDescription Descriptions[] =
	{
		{
			1,
			TEXT("DOBLE DAÑO"),
			TEXT("El siguiente ataque inflige el doble de daño. Esta habilidad se recarga cada 3s")
		},
		{
			2,
			TEXT("ATAQUE ÍGNEO"),
			TEXT("Tus ataques aplican una quemadura que inflige 10 de daño durante 2,5 segundos.")
		},
		{
			3,
			TEXT("FUERZA DESCOMUNAL"),
			TEXT("Tus ataques empujan al enemigo lejos de ti.")
		},
		{
			4,
			TEXT("GOLPE CRÍTICO"),
			TEXT("Tus ataques tienen un 25% de probabilidad de ser críticos, infligiendo el triple de daño.")
		},
		{
			5,
			TEXT("TOQUE DEBILITADOR"),
			TEXT("Los enemigos golpeados infligen solo el 50% de su daño durante 5 segundos.")
		},
		{
			6,
			TEXT("REFLEJOS MEJORADOS"),
			TEXT("Aumenta tu velocidad de movimiento en un 100%")
		},
		{
			7,
			TEXT("FAVOR DEL GRIAL"),
			TEXT("Obtienes una vida extra")
		},
		{
			8,
			TEXT("ARMADURA SUPERIOR"),
			TEXT("Reduce en un 50% todo el daño que recibes.")
		},
		{
			9,
			TEXT("PROYECTILES PARALIZANTES"),
			TEXT("Tus proyectiles paralizan a los enemigos haciendo que no te detecten durante 1 segundo.")
		},
		{
			10,
			TEXT("CONFIANZA CIEGA"),
			TEXT("Recibes solo un cuarto del daño que recies de frente, pero el doble si te atacan por la espalda.")
		},
		{
			11,
			TEXT("PASO SIGILOSO"),
			TEXT("Poder activo (R). Te vuelves indetectable y los enemigos no te detectan durante 3 segundos.")
		},
		{
			12,
			TEXT("DAÑO VAMPÍRICO"),
			TEXT("Recuperas vida igual a la mitad del daño que infliges a tus enemigos.")
		},
		{
			13,
			TEXT("ESQUIVA SUPERIOR"),
			TEXT("Obtienes un 20% de probabilidad de evitar por completo cada golpe recibido.")
		},
		{
			14,
			TEXT("DAÑO CURATIVO"),
			TEXT("El siguiente golpe recibido te cura. Esta habilidad vuelve a prepararse cada 5 segundos.")
		},
		{
			15,
			TEXT("ESQUIVE DIRECCIONAL"),
			TEXT("Pulsar dos veces una tecla de movimiento hace que hagas un esquive direccional, consumiendo 25 de stamina.")
		},
		{
			16,
			TEXT("SALTO METEÓRICO"),
			TEXT("Poder activo (R). Saltas hacia delante e infliges 30 de daño en area al aterrizar.")
		},
		{
			17,
			TEXT("SISTEMA MOTRIZ MEJORADO"),
			TEXT("Tus ataques de espada y patada se ejecutan mucho más rapido.")
		},
		{
			18,
			TEXT("BENDICIÓN DE NUNCA FALLA"),
			TEXT("Tus proyectiles buscan automaticamente al enemigo más cercano.")
		},
		{
			19,
			TEXT("REBOTE ARCANO"),
			TEXT("Tras golpear a un enemigo, tus proyectiles rebotan hacia el enemigo más cercano que aún no hayan golpeado.")
		},
		{
			20,
			TEXT("SUMINISTROS DE ALQUIMIA"),
			TEXT("Obtienes 9 recursos para pociones")
		},
		{
			21,
			TEXT("ATAQUE REFLECTANTE"),
			TEXT("Tus ataques de espada y patada desvían los proyectiles enemigos y los devuelven contra tus adversarios.")
		}
	};

	static const FDescription& GetDescription(const int32 Id)
	{
		const FDescription* SelectedDescription = &Descriptions[0];
		int32 DescriptionIndex = 0;
		bool bFound = false;
		while (DescriptionIndex < UE_ARRAY_COUNT(Descriptions) && !bFound)
		{
			const FDescription& Description = Descriptions[DescriptionIndex];
			if (Description.Id == Id)
			{
				SelectedDescription = &Description;
				bFound = true;
			}
			++DescriptionIndex;
		}
		return *SelectedDescription;
	}

	static const TCHAR* GetNpcName(const int32 NpcIndex)
	{
		static const TCHAR* Names[] =
		{
			TEXT("Sir Lanzarote"),
			TEXT("Sir Gawain"),
			TEXT("Sir Galahad"),
			TEXT("Sir Palomides"),
			TEXT("Sir Tristan"),
			TEXT("Sir Perceval"),
			TEXT("Sir Bedevere")
		};
		return NpcIndex >= 0 && NpcIndex < UE_ARRAY_COUNT(Names)
			? Names[NpcIndex]
			: TEXT("NPC desconocido");
	}

	static bool GetNpcPowerIds(const int32 NpcIndex, TArray<int32>& OutPowerIds)
	{
		OutPowerIds.Reset(3);
		bool bValidNpc = true;
		switch (NpcIndex)
		{
		case 0: OutPowerIds = { 17, 6, 15 }; break; // Lanzarote
		case 1: OutPowerIds = { 16, 4, 1 }; break;  // Gawain
		case 2: OutPowerIds = { 12, 14, 7 }; break; // Galahad
		case 3: OutPowerIds = { 21, 3, 20 }; break; // Palomides
		case 4: OutPowerIds = { 18, 19, 9 }; break; // Tristan
		case 5: OutPowerIds = { 2, 5, 13 }; break;  // Perceval
		case 6: OutPowerIds = { 8, 10, 11 }; break; // Bedevere
		default: bValidNpc = false; break;
		}
		return bValidNpc;
	}
}

bool URoomPowerUpSelectionComponent::ShowSelection(
	const int32 SelectionSeed,
	const int32 NpcIndex)
{
	bool bSelectionShown = false;
	UWorld* World = !ActiveWidget && !bSelectionCommitted ? GetWorld() : nullptr;
	APlayerController* Controller = World
		? UGameplayStatics::GetPlayerController(World, 0)
		: nullptr;
	ATFGCharacter* Character = Controller
		? Cast<ATFGCharacter>(Controller->GetPawn())
		: nullptr;
	if (World && (!Controller || !Character))
	{
		UE_LOG(
			LogRoomPowerUpSelection,
			Error,
			TEXT("[SELECCION POWER-UP] No se encontro un ATFGCharacter controlado."));
	}
	else if (World)
	{
		TArray<int32> AllowedIds;
		if (!RoomPowerUps::GetNpcPowerIds(NpcIndex, AllowedIds))
		{
			UE_LOG(
				LogRoomPowerUpSelection,
				Error,
				TEXT("[SELECCION POWER-UP] Indice de NPC invalido (%d); no se mostrara una oferta sin propietario."),
				NpcIndex);
		}
		else
		{
			TArray<int32> CandidateIds;
			const bool bYaTienePowerActivo =
				Character->ObtenerPowerUpActivoDeSala() != 0;
			for (const int32 PowerId : AllowedIds)
			{
				const bool bPowerActivoIncompatible =
					Character->EsPowerUpActivoDeSala(PowerId) && bYaTienePowerActivo;
				if (!bPowerActivoIncompatible && !Character->TienePowerUpDeSala(PowerId))
				{
					CandidateIds.Add(PowerId);
				}
			}

			// Keep the two-card layout after revisiting a knight, but never borrow a
			// power from another character. New powers remain first in the pool.
			if (CandidateIds.Num() < 2)
			{
				for (const int32 PowerId : AllowedIds)
			{
					const bool bPowerActivoIncompatible =
						Character->EsPowerUpActivoDeSala(PowerId) && bYaTienePowerActivo;
					if (!bPowerActivoIncompatible)
					{
						CandidateIds.AddUnique(PowerId);
					}
				}
			}
			if (CandidateIds.Num() < 2)
			{
				UE_LOG(
					LogRoomPowerUpSelection,
					Warning,
					TEXT("[SELECCION POWER-UP] %s no tiene dos poderes compatibles disponibles."),
					RoomPowerUps::GetNpcName(NpcIndex));
			}
			else
			{
				FRandomStream Random(SelectionSeed ^ 0x50A3E11);
				const int32 FirstIndex = Random.RandRange(0, CandidateIds.Num() - 1);
				OptionAId = CandidateIds[FirstIndex];
				CandidateIds.RemoveAtSwap(FirstIndex);
				if (Character->EsPowerUpActivoDeSala(OptionAId))
				{
					CandidateIds.RemoveAll(
						[Character](const int32 Id)
						{
							return Character->EsPowerUpActivoDeSala(Id);
						});
				}
				if (CandidateIds.IsEmpty())
				{
					for (const int32 PowerId : AllowedIds)
					{
						if (!Character->EsPowerUpActivoDeSala(PowerId) &&
							PowerId != OptionAId)
						{
							CandidateIds.Add(PowerId);
						}
					}
				}
				OptionBId = CandidateIds[Random.RandRange(0, CandidateIds.Num() - 1)];

				const RoomPowerUps::FDescription& OptionA = RoomPowerUps::GetDescription(OptionAId);
				const RoomPowerUps::FDescription& OptionB = RoomPowerUps::GetDescription(OptionBId);
				ActiveWidget = CreateWidget<URoomPowerUpSelectionWidget>(
					Controller, URoomPowerUpSelectionWidget::StaticClass());
				if (!ActiveWidget)
				{
					UE_LOG(LogRoomPowerUpSelection, Error,
						TEXT("[SELECCION POWER-UP] No se pudo crear la interfaz."));
				}
				else
				{
					ActiveWidget->Configure(
						OptionAId, FText::FromString(OptionA.Title),
						FText::FromString(OptionA.Description), OptionBId,
						FText::FromString(OptionB.Title), FText::FromString(OptionB.Description));
					ActiveWidget->OnPowerUpChosen.AddUObject(
						this, &URoomPowerUpSelectionComponent::HandlePowerUpChosen);
					ActiveWidget->AddToViewport(1000);
					PlayerController = Controller;
					PlayerCharacter = Character;
					bOwnsGameInput = true;
					Character->MovimientoDesactivado = true;
					Controller->SetIgnoreMoveInput(true);
					Controller->SetIgnoreLookInput(true);
					Controller->SetShowMouseCursor(true);

					FInputModeUIOnly InputMode;
					InputMode.SetWidgetToFocus(ActiveWidget->TakeWidget());
					InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
					Controller->SetInputMode(InputMode);
					bSelectionShown = true;
					UE_LOG(
						LogRoomPowerUpSelection, Display,
						TEXT("[SELECCION POWER-UP] %s ofrece exclusivamente: opcion A '%s' (%d), opcion B '%s' (%d). Portal bloqueado."),
						RoomPowerUps::GetNpcName(NpcIndex), OptionA.Title, OptionAId,
						OptionB.Title, OptionBId);
				}
			}
		}
	}
	return bSelectionShown;
}

void URoomPowerUpSelectionComponent::HandlePowerUpChosen(const int32 PowerUpId)
{
	if (bSelectionCommitted ||
		(PowerUpId != OptionAId && PowerUpId != OptionBId))
	{
		return;
	}
	bSelectionCommitted = true;

	const RoomPowerUps::FDescription& ChosenPower =
		RoomPowerUps::GetDescription(PowerUpId);
	const bool bGranted = PlayerCharacter.IsValid() &&
		PlayerCharacter->OtorgarPowerUpDeSala(PowerUpId);

	UE_LOG(
		LogRoomPowerUpSelection,
		Display,
		TEXT("[SELECCION POWER-UP] Elegido '%s' (%d), concedido=%s. Interfaz cerrada; solicitando apertura del portal."),
		ChosenPower.Title,
		PowerUpId,
		bGranted ? TEXT("si") : TEXT("no"));

	if (ActiveWidget)
	{
		ActiveWidget->OnPowerUpChosen.RemoveAll(this);
		ActiveWidget->RemoveFromParent();
		ActiveWidget = nullptr;
	}
	RestoreGameInput();

	OnSelectionFinished.Broadcast();
	DestroyComponent();
}

void URoomPowerUpSelectionComponent::RestoreGameInput()
{
	if (!bOwnsGameInput)
	{
		return;
	}
	bOwnsGameInput = false;

	if (PlayerCharacter.IsValid())
	{
		PlayerCharacter->MovimientoDesactivado = false;
	}

	if (PlayerController.IsValid())
	{
		PlayerController->SetIgnoreMoveInput(false);
		PlayerController->SetIgnoreLookInput(false);
		PlayerController->SetShowMouseCursor(false);
		FInputModeGameOnly InputMode;
		PlayerController->SetInputMode(InputMode);
	}
}

void URoomPowerUpSelectionComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (ActiveWidget)
	{
		ActiveWidget->OnPowerUpChosen.RemoveAll(this);
		ActiveWidget->RemoveFromParent();
		ActiveWidget = nullptr;
	}
	RestoreGameInput();
	Super::EndPlay(EndPlayReason);
}
