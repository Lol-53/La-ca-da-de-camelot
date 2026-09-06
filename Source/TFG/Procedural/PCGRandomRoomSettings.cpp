#include "Procedural/PCGRandomRoomSettings.h"

#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/PCGPointData.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Helpers/PCGHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "PCGContext.h"
#include "PCGPoint.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Systems/RunPowerPersistenceSubsystem.h"
#include "TimerManager.h"
#include "UI/RoomPowerUpSelectionComponent.h"
#include "UObject/UnrealType.h"

#include "EngineUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGRandomRoomSettings)

#define LOCTEXT_NAMESPACE "PCGRandomRoomSettings"

// The implementation deliberately runs on the game thread because it reads Blueprint instance data.

const FName UPCGRandomRoomSettings::FloorPinLabel(TEXT("Floor"));
const FName UPCGRandomRoomSettings::WallsPinLabel(TEXT("Walls"));

namespace ProceduralRoom
{
	static const FName ProceduralEnemyTag(TEXT("ProceduralRoomEnemy"));
	static const FName ProceduralPropTag(TEXT("ProceduralRoomProp"));
	static const FName ProceduralObstacleTag(TEXT("ProceduralRoomObstacle"));
	static const FName ProceduralLegacyDecorationTag(TEXT("ProceduralRoomLegacyDecoration"));
	static const FName ProceduralRoomTag(TEXT("ProceduralRoom"));
	static const FName ProceduralRoomClearedTag(TEXT("ProceduralRoomCleared"));
	static const FName ProceduralDialogueNpcTag(TEXT("ProceduralRoomDialogueNPC"));
	static const FName ProceduralDecorationComponentTag(TEXT("ProceduralRoomDecoration"));

	constexpr double MinimumWallHeightScale = 3.0;
	// The wall mesh is only 12.5 cm thick before scaling and the PCG spawner
	// applies an additional 0.5 scale on its depth. A value of 4 produces a
	// final 25 cm wall, thick enough for stable Lumen/VSM occlusion.
	constexpr double MinimumWallThicknessScale = 4.0;
	// Slightly overlap adjacent modules so their end vertices never expose a
	// sub-pixel slit to the exterior light.
	constexpr double WallLengthOverlapMultiplier = 1.02;
	constexpr double WallJointSealWidth = 30.0;
	// Runtime measurement of SM_Crypt_Wall after the PCG graph's mesh scaling:
	// min Z = 0, max Z = 450, final Z scale = 1.5, so its upper edge is Z 675.
	constexpr double GeneratedWallTopZ = 675.0;
	constexpr double GeneratedTorchZ = 430.0;

	struct FRoomConfig
	{
		double HalfWidth = 1200.0;
		double HalfLength = 900.0;
		double TileSize = 200.0;
		int32 MinTilesX = 5;
		int32 MinTilesY = 4;
		double IrregularChance = 0.70;
		double IrregularFillRatio = 0.72;
		bool bCreateExitOpening = false;
	};

	struct FBoundaryEdge
	{
		FIntPoint Cell = FIntPoint::ZeroValue;
		FIntPoint Direction = FIntPoint::ZeroValue;
		FVector LocalPosition = FVector::ZeroVector;
		double WallYaw = 0.0;
		double ExitYaw = 0.0;
		bool bExterior = true;
	};

	struct FRoomLayout
	{
		int32 TilesX = 0;
		int32 TilesY = 0;
		bool bIrregular = false;
		TArray<FIntPoint> Cells;
		TSet<FIntPoint> CellSet;
		TArray<FBoundaryEdge> BoundaryEdges;
		int32 ExitEdgeIndex = INDEX_NONE;
	};

	static const FIntPoint Directions[] =
	{
		FIntPoint(1, 0),
		FIntPoint(-1, 0),
		FIntPoint(0, 1),
		FIntPoint(0, -1)
	};

	static const FProperty* FindProperty(const UObject* Object, const FName Name)
	{
		return Object ? Object->GetClass()->FindPropertyByName(Name) : nullptr;
	}

	static FProperty* FindProperty(UObject* Object, const FName Name)
	{
		return Object ? Object->GetClass()->FindPropertyByName(Name) : nullptr;
	}

	static double ReadNumber(const UObject* Object, const FName Name, const double Fallback)
	{
		const FNumericProperty* Property = CastField<FNumericProperty>(FindProperty(Object, Name));
		double Result = Fallback;
		if (Property)
		{
			const void* Value = Property->ContainerPtrToValuePtr<void>(Object);
			Result = Property->IsFloatingPoint()
				? Property->GetFloatingPointPropertyValue(Value)
				: static_cast<double>(Property->GetSignedIntPropertyValue(Value));
		}
		return Result;
	}

	static bool ReadBool(const UObject* Object, const FName Name, const bool Fallback)
	{
		const FBoolProperty* Property = CastField<FBoolProperty>(FindProperty(Object, Name));
		return Property ? Property->GetPropertyValue_InContainer(Object) : Fallback;
	}

	static int32 ReadArrayLength(const UObject* Object, const FName Name)
	{
		const FArrayProperty* Property = CastField<FArrayProperty>(FindProperty(Object, Name));
		int32 Length = 0;
		if (Property)
		{
			const void* ArrayValue = Property->ContainerPtrToValuePtr<void>(Object);
			FScriptArrayHelper ArrayHelper(Property, ArrayValue);
			Length = ArrayHelper.Num();
		}
		return Length;
	}

	static bool WriteInteger(UObject* Object, const FName Name, const int64 Value)
	{
		FNumericProperty* Property = CastField<FNumericProperty>(FindProperty(Object, Name));
		const bool bCanWrite = Property && !Property->IsFloatingPoint();
		if (bCanWrite)
		{
			Property->SetIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Object), Value);
		}
		return bCanWrite;
	}

	static bool WriteBool(UObject* Object, const FName Name, const bool Value)
	{
		FBoolProperty* Property = CastField<FBoolProperty>(FindProperty(Object, Name));
		const bool bCanWrite = Property != nullptr;
		if (bCanWrite)
		{
			Property->SetPropertyValue_InContainer(Object, Value);
		}
		return bCanWrite;
	}

	static bool WriteNumber(UObject* Object, const FName Name, const double Value)
	{
		FNumericProperty* Property = CastField<FNumericProperty>(FindProperty(Object, Name));
		const bool bCanWrite = Property != nullptr;
		if (bCanWrite)
		{
			void* Destination = Property->ContainerPtrToValuePtr<void>(Object);
			if (Property->IsFloatingPoint())
			{
				Property->SetFloatingPointPropertyValue(Destination, Value);
			}
			else
			{
				Property->SetIntPropertyValue(Destination, FMath::RoundToInt64(Value));
			}
		}
		return bCanWrite;
	}

	struct FGeneratedDecorationCounts
	{
		int32 Ceilings = 0;
		int32 Torches = 0;
		int32 FloorFires = 0;
	};

	static void DestroyGeneratedDecorations(AActor* RoomActor)
	{
		if (!IsValid(RoomActor))
		{
			return;
		}

		TInlineComponentArray<UActorComponent*> Components(RoomActor);
		for (UActorComponent* Component : Components)
		{
			if (IsValid(Component) && Component->ComponentHasTag(ProceduralDecorationComponentTag))
			{
				Component->DestroyComponent();
			}
		}
	}

	static USceneComponent* CreateDecorationAnchor(
		AActor* RoomActor,
		const FName BaseName,
		const FTransform& RelativeTransform)
	{
		USceneComponent* Anchor = IsValid(RoomActor) && RoomActor->GetRootComponent()
			? NewObject<USceneComponent>(
				RoomActor,
				MakeUniqueObjectName(RoomActor, USceneComponent::StaticClass(), BaseName))
			: nullptr;
		if (Anchor)
		{
			Anchor->ComponentTags.AddUnique(ProceduralDecorationComponentTag);
			RoomActor->AddInstanceComponent(Anchor);
			Anchor->SetupAttachment(RoomActor->GetRootComponent());
			Anchor->SetMobility(EComponentMobility::Movable);
			Anchor->RegisterComponent();
			Anchor->SetRelativeTransform(RelativeTransform);
		}
		return Anchor;
	}

	static UStaticMeshComponent* CreateDecorationMesh(
		AActor* RoomActor,
		USceneComponent* Parent,
		UStaticMesh* Mesh,
		const FName BaseName,
		const FTransform& RelativeTransform,
		const bool bCollision)
	{
		UStaticMeshComponent* MeshComponent = IsValid(RoomActor) && IsValid(Parent) && IsValid(Mesh)
			? NewObject<UStaticMeshComponent>(
				RoomActor,
				MakeUniqueObjectName(RoomActor, UStaticMeshComponent::StaticClass(), BaseName))
			: nullptr;
		if (MeshComponent)
		{
		MeshComponent->ComponentTags.AddUnique(ProceduralDecorationComponentTag);
		RoomActor->AddInstanceComponent(MeshComponent);
		MeshComponent->SetupAttachment(Parent);
		MeshComponent->SetMobility(EComponentMobility::Movable);
		MeshComponent->SetStaticMesh(Mesh);
		MeshComponent->SetCollisionEnabled(bCollision
			? ECollisionEnabled::QueryAndPhysics
			: ECollisionEnabled::NoCollision);
		if (bCollision)
		{
			MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
		}
		MeshComponent->SetGenerateOverlapEvents(false);
		MeshComponent->SetCastShadow(true);
		MeshComponent->RegisterComponent();
		MeshComponent->SetRelativeTransform(RelativeTransform);
		}
		return MeshComponent;
	}

	static UHierarchicalInstancedStaticMeshComponent* CreateDecorationInstancer(
		AActor* RoomActor,
		UStaticMesh* Mesh,
		const FName BaseName,
		const bool bCollision = true)
	{
		UHierarchicalInstancedStaticMeshComponent* Instancer =
			IsValid(RoomActor) && RoomActor->GetRootComponent() && IsValid(Mesh)
			? NewObject<UHierarchicalInstancedStaticMeshComponent>(
				RoomActor,
				MakeUniqueObjectName(
					RoomActor,
					UHierarchicalInstancedStaticMeshComponent::StaticClass(),
					BaseName))
			: nullptr;
		if (Instancer)
		{
		Instancer->ComponentTags.AddUnique(ProceduralDecorationComponentTag);
		RoomActor->AddInstanceComponent(Instancer);
		Instancer->SetupAttachment(RoomActor->GetRootComponent());
		Instancer->SetMobility(EComponentMobility::Movable);
		Instancer->SetStaticMesh(Mesh);
		Instancer->SetCollisionEnabled(
			bCollision
				? ECollisionEnabled::QueryAndPhysics
				: ECollisionEnabled::NoCollision);
		Instancer->SetCollisionResponseToAllChannels(
			bCollision
				? ECR_Block
				: ECR_Ignore);
		Instancer->SetGenerateOverlapEvents(false);
		Instancer->SetCastShadow(true);
		// SM_Ceiling_Flat is a zero-thickness plane. Its stone material is already
		// opaque, but without two-sided shadows light arriving from the back face
		// crosses the ceiling and illuminates the room as if it were translucent.
		Instancer->bCastShadowAsTwoSided = true;
		Instancer->RegisterComponent();
		}
		return Instancer;
	}

	static bool CreateGeneratedWallJointSeals(AActor* RoomActor)
	{
		bool bSealsAvailable = false;
		const bool bRoomValid = IsValid(RoomActor) && RoomActor->GetRootComponent();
		if (bRoomValid)
		{
			TInlineComponentArray<UActorComponent*> ExistingComponents(RoomActor);
			int32 ExistingIndex = 0;
			while (ExistingIndex < ExistingComponents.Num() && !bSealsAvailable)
			{
				UActorComponent* ExistingComponent = ExistingComponents[ExistingIndex];
				bSealsAvailable = IsValid(ExistingComponent) &&
					ExistingComponent->GetName().StartsWith(TEXT("ProceduralWallJointSeals"));
				++ExistingIndex;
			}
		}

		UInstancedStaticMeshComponent* WallInstancer = nullptr;
		if (bRoomValid && !bSealsAvailable)
		{
			TInlineComponentArray<UInstancedStaticMeshComponent*> Instancers(RoomActor);
			int32 InstancerIndex = 0;
			while (InstancerIndex < Instancers.Num() && !WallInstancer)
			{
				UInstancedStaticMeshComponent* Candidate = Instancers[InstancerIndex];
				if (IsValid(Candidate) && IsValid(Candidate->GetStaticMesh()) &&
					Candidate->GetStaticMesh()->GetName() == TEXT("SM_Crypt_Wall"))
				{
					WallInstancer = Candidate;
				}
				++InstancerIndex;
			}
			if (!WallInstancer)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("Procedural wall joint seals were skipped because the generated wall instancer was not found."));
			}
		}

		UStaticMesh* CubeMesh = WallInstancer
			? LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"))
			: nullptr;
		if (WallInstancer && !CubeMesh)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("Procedural wall joint seals could not load the cube mesh."));
		}

		UHierarchicalInstancedStaticMeshComponent* JointSeals = CubeMesh
			? CreateDecorationInstancer(RoomActor, CubeMesh,
				TEXT("ProceduralWallJointSeals"), false)
			: nullptr;
		if (JointSeals)
		{
			UStaticMesh* WallMesh = WallInstancer->GetStaticMesh();
			if (WallMesh->GetMaterial(0))
			{
				JointSeals->SetMaterial(0, WallMesh->GetMaterial(0));
			}

			const FBoxSphereBounds WallBounds = WallMesh->GetBounds();
			const double WallMinX = WallBounds.Origin.X - WallBounds.BoxExtent.X;
			const double WallMaxX = WallBounds.Origin.X + WallBounds.BoxExtent.X;
			const FBoxSphereBounds CubeBounds = CubeMesh->GetBounds();
			const double SealScaleX = WallJointSealWidth /
				FMath::Max(1.0, CubeBounds.BoxExtent.X * 2.0);
			const double SealScaleY = WallJointSealWidth /
				FMath::Max(1.0, CubeBounds.BoxExtent.Y * 2.0);
			const double SealScaleZ = GeneratedWallTopZ /
				FMath::Max(1.0, CubeBounds.BoxExtent.Z * 2.0);
			const double SealLocalZ = GeneratedWallTopZ * 0.5 -
				CubeBounds.Origin.Z * SealScaleZ;

			TMap<FIntPoint, FVector> UniqueLocalEndpoints;
			for (int32 InstanceIndex = 0;
				InstanceIndex < WallInstancer->GetInstanceCount(); ++InstanceIndex)
			{
				FTransform WallWorldTransform;
				if (WallInstancer->GetInstanceTransform(
					InstanceIndex, WallWorldTransform, true))
				{
					const double EndpointXs[] = {WallMinX, WallMaxX};
					for (const double EndpointX : EndpointXs)
					{
						const FVector WorldEndpoint = WallWorldTransform.TransformPosition(
							FVector(EndpointX, WallBounds.Origin.Y, WallBounds.Origin.Z));
						FVector LocalEndpoint = RoomActor->GetActorTransform()
							.InverseTransformPosition(WorldEndpoint);
						LocalEndpoint.Z = SealLocalZ;
						const FIntPoint EndpointKey(
							FMath::RoundToInt(LocalEndpoint.X),
							FMath::RoundToInt(LocalEndpoint.Y));
						UniqueLocalEndpoints.Add(EndpointKey, LocalEndpoint);
					}
				}
			}

			for (const TPair<FIntPoint, FVector>& Endpoint : UniqueLocalEndpoints)
			{
				JointSeals->AddInstance(FTransform(
					FRotator::ZeroRotator, Endpoint.Value,
					FVector(SealScaleX, SealScaleY, SealScaleZ)));
			}
			UE_LOG(LogTemp, Display,
				TEXT("Procedural wall joints sealed at %d generated module endpoints using %.0f cm opaque columns."),
				UniqueLocalEndpoints.Num(), WallJointSealWidth);
			bSealsAvailable = true;
		}
		return bSealsAvailable;
	}

	static UParticleSystemComponent* CreateDecorationFire(
		AActor* RoomActor,
		USceneComponent* Parent,
		UParticleSystem* Template,
		const FName BaseName,
		const FTransform& RelativeTransform)
	{
		UParticleSystemComponent* FireComponent =
			IsValid(RoomActor) && IsValid(Parent) && IsValid(Template)
			? NewObject<UParticleSystemComponent>(
				RoomActor,
				MakeUniqueObjectName(RoomActor, UParticleSystemComponent::StaticClass(), BaseName))
			: nullptr;
		if (FireComponent)
		{
		FireComponent->ComponentTags.AddUnique(ProceduralDecorationComponentTag);
		RoomActor->AddInstanceComponent(FireComponent);
		FireComponent->SetupAttachment(Parent);
		FireComponent->SetMobility(EComponentMobility::Movable);
		FireComponent->SetTemplate(Template);
		FireComponent->bAutoActivate = true;
		FireComponent->RegisterComponent();
		FireComponent->SetRelativeTransform(RelativeTransform);
		FireComponent->ActivateSystem(true);
		}
		return FireComponent;
	}

	static UPointLightComponent* CreateDecorationLight(
		AActor* RoomActor,
		USceneComponent* Parent,
		const FName BaseName,
		const FVector& RelativeLocation,
		const float Intensity,
		const float AttenuationRadius)
	{
		UPointLightComponent* Light = IsValid(RoomActor) && IsValid(Parent)
			? NewObject<UPointLightComponent>(
				RoomActor,
				MakeUniqueObjectName(RoomActor, UPointLightComponent::StaticClass(), BaseName))
			: nullptr;
		if (Light)
		{
		Light->ComponentTags.AddUnique(ProceduralDecorationComponentTag);
		RoomActor->AddInstanceComponent(Light);
		Light->SetupAttachment(Parent);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetIntensity(Intensity);
		Light->SetAttenuationRadius(AttenuationRadius);
		Light->SetLightColor(FLinearColor(1.0f, 0.36f, 0.10f));
		Light->SetCastShadows(true);
		Light->RegisterComponent();
		Light->SetRelativeLocation(RelativeLocation);
		}
		return Light;
	}

	static void ConfigureEnemySensing(AActor* Enemy)
	{
		UClass* SensingClass = LoadClass<UActorComponent>(
			nullptr,
			TEXT("/Script/AIModule.PawnSensingComponent"));
		UActorComponent* Sensing = IsValid(Enemy) && SensingClass
			? Enemy->GetComponentByClass(TSubclassOf<UActorComponent>(SensingClass))
			: nullptr;
		if (!Sensing)
		{
			return;
		}

		WriteNumber(Sensing, TEXT("SightRadius"), 1200.0);
		WriteNumber(Sensing, TEXT("SensingInterval"), 0.25);
		WriteNumber(Sensing, TEXT("PeripheralVisionAngle"), 180.0);
		WriteNumber(Sensing, TEXT("PeripheralVisionCosine"), -1.0);
		WriteBool(Sensing, TEXT("bOnlySensePlayers"), true);
	}

	static void InvokeNoParameterFunction(UObject* Object, const FName FunctionName)
	{
		if (IsValid(Object))
		{
			if (UFunction* Function = Object->FindFunction(FunctionName))
			{
				Object->ProcessEvent(Function, nullptr);
			}
		}
	}

	static void ActivateRoomExit(AActor* RoomActor)
	{
		if (!IsValid(RoomActor))
		{
			return;
		}

		if (UFunction* ClearRoomFunction = RoomActor->FindFunction(TEXT("ClearRoom")))
		{
			RoomActor->ProcessEvent(ClearRoomFunction, nullptr);
			UE_LOG(
				LogTemp,
				Display,
				TEXT("Room exit activation requested; ClearRoom invoked and the portal was opened."));
		}

		// Keep the portal usable even if its Blueprint activation node is rebuilt.
		if (FObjectPropertyBase* ExitProperty = CastField<FObjectPropertyBase>(
			FindProperty(RoomActor, TEXT("RoomExitRef"))))
		{
			if (AActor* ExitActor = Cast<AActor>(
				ExitProperty->GetObjectPropertyValue_InContainer(RoomActor)))
			{
				WriteBool(ExitActor, TEXT("bIsActive"), true);
				ExitActor->SetActorHiddenInGame(false);
				ExitActor->SetActorEnableCollision(true);
			}
		}
	}

	static void KeepRoomExitLocked(AActor* RoomActor)
	{
		if (!IsValid(RoomActor))
		{
			return;
		}

		if (FObjectPropertyBase* ExitProperty = CastField<FObjectPropertyBase>(
			FindProperty(RoomActor, TEXT("RoomExitRef"))))
		{
			if (AActor* ExitActor = Cast<AActor>(
				ExitProperty->GetObjectPropertyValue_InContainer(RoomActor)))
			{
				InvokeNoParameterFunction(ExitActor, TEXT("DeactivateExit"));
				WriteBool(ExitActor, TEXT("bIsActive"), false);
				ExitActor->SetActorHiddenInGame(true);
				ExitActor->SetActorEnableCollision(false);
			}
		}
	}

	static bool SnapDialogueNpcToFloor(AActor* Npc, double& OutFloorZ)
	{
		bool bSnapped = false;
		UWorld* World = IsValid(Npc) ? Npc->GetWorld() : nullptr;
		const FVector CurrentLocation = IsValid(Npc)
			? Npc->GetActorLocation()
			: FVector::ZeroVector;
		const FVector TraceStart = CurrentLocation + FVector(0.0, 0.0, 25.0);
		const FVector TraceEnd = CurrentLocation - FVector(0.0, 0.0, 1500.0);
		FCollisionQueryParams QueryParams(
			SCENE_QUERY_STAT(ProceduralDialogueNpcFloor),
			false,
			Npc);
		FHitResult FloorHit;
		const bool bFloorFound = World && World->LineTraceSingleByChannel(
			FloorHit,
			TraceStart,
			TraceEnd,
			ECC_Visibility,
			QueryParams);
		if (bFloorFound)
		{
		double CapsuleHalfHeight = 0.0;
		if (const UCapsuleComponent* Capsule =
			Npc->FindComponentByClass<UCapsuleComponent>())
		{
			CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		}

		OutFloorZ = FloorHit.ImpactPoint.Z;
		const FVector GroundedLocation(
			CurrentLocation.X,
			CurrentLocation.Y,
			OutFloorZ + CapsuleHalfHeight + 1.0);
		Npc->SetActorLocation(
			GroundedLocation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);

		if (ACharacter* NpcCharacter = Cast<ACharacter>(Npc))
		{
			if (UCharacterMovementComponent* Movement =
				NpcCharacter->GetCharacterMovement())
			{
				Movement->StopMovementImmediately();
			}
		}
		bSnapped = true;
		}
		return bSnapped;
	}

	static void SpawnDialogueNpcAndWaitForExit(
		AActor* RoomActor,
		const FTransform& NpcWorldTransform)
	{
		UWorld* World = IsValid(RoomActor) ? RoomActor->GetWorld() : nullptr;
		if (World)
		{
		KeepRoomExitLocked(RoomActor);
		static const TCHAR* NpcClassPaths[] =
		{
			TEXT("/Game/MyContent/Npcs/BP_Sir_Lanzarote.BP_Sir_Lanzarote_C"),
			TEXT("/Game/MyContent/Npcs/BP_Sir_Gawain.BP_Sir_Gawain_C"),
			TEXT("/Game/MyContent/Npcs/BP_Sir_Galahad.BP_Sir_Galahad_C"),
			TEXT("/Game/MyContent/Npcs/BP_Sir_Palomides.BP_Sir_Palomides_C"),
			TEXT("/Game/MyContent/Npcs/BP_Sir_Tristan.BP_Sir_Tristan_C"),
			TEXT("/Game/MyContent/Npcs/BP_Sir_Perceval.BP_Sir_Perceval_C"),
			TEXT("/Game/MyContent/Npcs/BP_Sir_Bedevere.BP_Sir_Bedevere_C")
		};

		const int32 NpcCount = UE_ARRAY_COUNT(NpcClassPaths);
		int32 NpcIndex = static_cast<int32>(
			HashCombineFast(
				GetTypeHash(RoomActor->GetActorLocation()),
				GetTypeHash(RoomActor->GetFName())) % static_cast<uint32>(NpcCount));
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (URunPowerPersistenceSubsystem* Persistence =
				GameInstance->GetSubsystem<URunPowerPersistenceSubsystem>())
			{
				NpcIndex = Persistence->ChooseUniqueNpcIndex(
					HashCombineFast(
						GetTypeHash(RoomActor->GetActorLocation()),
						GetTypeHash(RoomActor->GetFName())),
					NpcCount);
			}
		}

		UClass* NpcClass = LoadClass<AActor>(
			nullptr,
			NpcClassPaths[NpcIndex]);
		if (!NpcClass)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("Unique room dialogue NPC %s could not be loaded; opening the portal to avoid a soft lock."),
				NpcClassPaths[NpcIndex]);
			ActivateRoomExit(RoomActor);
		}
		else
		{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		AActor* Npc = World->SpawnActor<AActor>(
			NpcClass,
			NpcWorldTransform,
			SpawnParameters);
		if (!IsValid(Npc))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("Room dialogue NPC failed to spawn; opening the portal to avoid a soft lock."));
			ActivateRoomExit(RoomActor);
		}
		else
		{
		double NpcFloorZ = 0.0;
		if (SnapDialogueNpcToFloor(Npc, NpcFloorZ))
		{
			UE_LOG(
				LogTemp,
				Display,
				TEXT("NPCBase placed directly on the room floor at Z %.1f; actor location is %s."),
				NpcFloorZ,
				*Npc->GetActorLocation().ToCompactString());
		}
		else
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("NPCBase floor trace failed at %s; normal character gravity will finish grounding it."),
				*Npc->GetActorLocation().ToCompactString());
		}

		Npc->Tags.AddUnique(ProceduralDialogueNpcTag);
		WriteInteger(Npc, TEXT("IndexDialogo"), 0);
		const int32 DialogueLineCount = ReadArrayLength(Npc, TEXT("Dialogo"));
		UE_LOG(
			LogTemp,
			Display,
			TEXT("All room enemies defeated. Portal remains locked; unique NPC %s spawned at %s with %d dialogue lines."),
			*NpcClass->GetName(),
			*Npc->GetActorLocation().ToCompactString(),
			DialogueLineCount);

		if (DialogueLineCount <= 0)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("NPCBase has no dialogue lines; opening the portal to avoid a soft lock."));
			ActivateRoomExit(RoomActor);
		}
		else
		{
		const TWeakObjectPtr<AActor> WeakRoomActor(RoomActor);
		const TWeakObjectPtr<AActor> WeakNpc(Npc);
		const TWeakObjectPtr<UWorld> WeakWorld(World);
		const TSharedRef<FTimerHandle> DialogueTimer = MakeShared<FTimerHandle>();
		const TSharedRef<bool> bDialogueStarted = MakeShared<bool>(false);
		World->GetTimerManager().SetTimer(
			*DialogueTimer,
			FTimerDelegate::CreateLambda(
				[WeakRoomActor, WeakNpc, WeakWorld, DialogueTimer, bDialogueStarted, NpcIndex]()
			{
				const bool bContextValid = WeakWorld.IsValid() && WeakRoomActor.IsValid();
				if (!bContextValid)
				{
					if (WeakWorld.IsValid())
					{
						WeakWorld->GetTimerManager().ClearTimer(*DialogueTimer);
					}
				}
				else if (!WeakNpc.IsValid())
				{
					UE_LOG(
						LogTemp,
						Warning,
						TEXT("NPCBase disappeared before finishing its dialogue; opening the portal to avoid a soft lock."));
					ActivateRoomExit(WeakRoomActor.Get());
					WeakWorld->GetTimerManager().ClearTimer(*DialogueTimer);
				}
				else
				{
				const int32 DialogueIndex = FMath::RoundToInt(
					ReadNumber(WeakNpc.Get(), TEXT("IndexDialogo"), 0.0));
				if (DialogueIndex > 0)
				{
					if (!*bDialogueStarted)
					{
						*bDialogueStarted = true;
						UE_LOG(
							LogTemp,
							Display,
							TEXT("NPCBase dialogue started; portal remains locked."));
					}
				}
				else if (*bDialogueStarted)
				{
					UE_LOG(
						LogTemp,
						Display,
						TEXT("NPCBase dialogue finished; showing the two-option power-up selection while the portal remains locked."));
					WeakWorld->GetTimerManager().ClearTimer(*DialogueTimer);

					URoomPowerUpSelectionComponent* SelectionComponent =
						NewObject<URoomPowerUpSelectionComponent>(
							WeakRoomActor.Get(),
							MakeUniqueObjectName(
								WeakRoomActor.Get(),
								URoomPowerUpSelectionComponent::StaticClass(),
								TEXT("RoomPowerUpSelection")));
					bool bSelectionShown = false;
					if (SelectionComponent)
					{
						SelectionComponent->ComponentTags.AddUnique(
							ProceduralDecorationComponentTag);
						WeakRoomActor->AddInstanceComponent(SelectionComponent);
						SelectionComponent->RegisterComponent();
						SelectionComponent->OnSelectionFinished.AddLambda(
							[WeakRoomActor]()
						{
							if (WeakRoomActor.IsValid())
							{
								UE_LOG(
									LogTemp,
									Display,
									TEXT("Room power-up selected; unlocking the portal."));
								ActivateRoomExit(WeakRoomActor.Get());
							}
						});

						const int32 SelectionSeed = FMath::RoundToInt(
							ReadNumber(
								WeakRoomActor.Get(),
								TEXT("RoomSeed"),
								1.0));
						bSelectionShown =
							SelectionComponent->ShowSelection(SelectionSeed, NpcIndex);
						if (!bSelectionShown)
						{
							SelectionComponent->DestroyComponent();
						}
					}

					if (!bSelectionShown)
					{
						UE_LOG(
							LogTemp,
							Warning,
							TEXT("Power-up selection could not be shown; opening the portal to avoid a soft lock."));
						ActivateRoomExit(WeakRoomActor.Get());
					}
				}
				}
			}),
			0.1f,
			true);
		}
		}
		}
		}
	}

	static void MonitorBaseEnemies(
		const TWeakObjectPtr<AActor> WeakRoomActor,
		const FTransform NpcWorldTransform)
	{
		if (!WeakRoomActor.IsValid() || !WeakRoomActor->GetWorld())
		{
			return;
		}

		const TWeakObjectPtr<UWorld> WeakWorld(WeakRoomActor->GetWorld());
		const TSharedRef<FTimerHandle> MonitorTimer = MakeShared<FTimerHandle>();
		WeakRoomActor->GetWorld()->GetTimerManager().SetTimer(
			*MonitorTimer,
			FTimerDelegate::CreateLambda(
				[WeakRoomActor, WeakWorld, MonitorTimer, NpcWorldTransform]()
		{
			if (!WeakWorld.IsValid() || !WeakRoomActor.IsValid())
			{
				if (WeakWorld.IsValid())
				{
					WeakWorld->GetTimerManager().ClearTimer(*MonitorTimer);
				}
				return;
			}

			ACharacter* PlayerCharacter =
				UGameplayStatics::GetPlayerCharacter(WeakWorld.Get(), 0);
			int32 LivingEnemies = 0;
			for (TActorIterator<AActor> ActorIt(WeakWorld.Get()); ActorIt; ++ActorIt)
			{
				AActor* Enemy = *ActorIt;
				if (IsValid(Enemy) && Enemy->ActorHasTag(ProceduralEnemyTag) &&
					!ReadBool(Enemy, TEXT("Muerto"), false))
				{
					++LivingEnemies;

					if (IsValid(PlayerCharacter))
					{
						APawn* EnemyPawn = Cast<APawn>(Enemy);
						AController* EnemyController = EnemyPawn
							? EnemyPawn->GetController()
							: nullptr;
						const double DistanceToPlayer = FVector::Dist2D(
							Enemy->GetActorLocation(),
							PlayerCharacter->GetActorLocation());

						InvokeNoParameterFunction(
							EnemyController,
							DistanceToPlayer <= 1200.0
								? TEXT("AgroOn")
								: TEXT("AgroOff"));
						InvokeNoParameterFunction(
							EnemyController,
							DistanceToPlayer <= 200.0
								? TEXT("EstoyARangoOn")
								: TEXT("EstoyARangoOff"));
					}
				}
			}

			// Both enemies were confirmed spawned before this monitor started. Treat
			// destroyed enemies as defeated too, so the room cannot remain locked if
			// both death actors disappear between two monitor ticks.
			if (LivingEnemies == 0 &&
				!WeakRoomActor->ActorHasTag(ProceduralRoomClearedTag))
			{
				WeakRoomActor->Tags.AddUnique(ProceduralRoomClearedTag);
				SpawnDialogueNpcAndWaitForExit(
					WeakRoomActor.Get(),
					NpcWorldTransform);
				WeakWorld->GetTimerManager().ClearTimer(*MonitorTimer);
			}
		}),
			0.2f,
			true);
	}

	static bool WriteTransform(UObject* Object, const FName Name, const FTransform& Value)
	{
		FStructProperty* Property = CastField<FStructProperty>(FindProperty(Object, Name));
		const bool bCanWrite = Property &&
			Property->Struct == TBaseStructure<FTransform>::Get();
		if (bCanWrite)
		{
			*Property->ContainerPtrToValuePtr<FTransform>(Object) = Value;
		}
		return bCanWrite;
	}

	static FRoomConfig ReadConfig(const AActor* Actor, const UPCGRandomRoomSettings* Settings = nullptr)
	{
		FRoomConfig Config;
		if (Settings)
		{
			Config.TileSize = Settings->DefaultTileSize;
			Config.MinTilesX = Settings->DefaultMinRoomTilesX;
			Config.MinTilesY = Settings->DefaultMinRoomTilesY;
			Config.IrregularChance = Settings->DefaultIrregularRoomChance;
			Config.IrregularFillRatio = Settings->DefaultIrregularFillRatio;
		}

		Config.HalfWidth = FMath::Max(100.0, FMath::Abs(ReadNumber(Actor, TEXT("RoomWidth"), Config.HalfWidth)));
		Config.HalfLength = FMath::Max(100.0, FMath::Abs(ReadNumber(Actor, TEXT("RoomLength"), Config.HalfLength)));
		Config.TileSize = FMath::Clamp(ReadNumber(Actor, TEXT("RoomTileSize"), Config.TileSize), 25.0, 2000.0);
		Config.MinTilesX = FMath::Clamp(FMath::RoundToInt(ReadNumber(Actor, TEXT("MinRoomTilesX"), Config.MinTilesX)), 2, 64);
		Config.MinTilesY = FMath::Clamp(FMath::RoundToInt(ReadNumber(Actor, TEXT("MinRoomTilesY"), Config.MinTilesY)), 2, 64);
		Config.IrregularChance = FMath::Clamp(ReadNumber(Actor, TEXT("IrregularRoomChance"), Config.IrregularChance), 0.0, 1.0);
		Config.IrregularFillRatio = FMath::Clamp(ReadNumber(Actor, TEXT("IrregularFillRatio"), Config.IrregularFillRatio), 0.35, 1.0);
		Config.bCreateExitOpening = ReadBool(Actor, TEXT("bCreateExitOpening"), false);
		return Config;
	}

	static bool IsInside(const FIntPoint& Point, const int32 Width, const int32 Height)
	{
		return Point.X >= 0 && Point.X < Width && Point.Y >= 0 && Point.Y < Height;
	}

	static int32 CountOccupiedNeighbours(const FIntPoint& Point, const TSet<FIntPoint>& Cells)
	{
		int32 Count = 0;
		for (const FIntPoint& Direction : Directions)
		{
			Count += Cells.Contains(Point + Direction) ? 1 : 0;
		}
		return Count;
	}

	static void AddConnectedCell(
		TSet<FIntPoint>& Cells,
		const int32 Width,
		const int32 Height,
		FRandomStream& Random)
	{
		TSet<FIntPoint> FrontierSet;
		for (const FIntPoint& Cell : Cells)
		{
			for (const FIntPoint& Direction : Directions)
			{
				const FIntPoint Candidate = Cell + Direction;
				if (IsInside(Candidate, Width, Height) && !Cells.Contains(Candidate))
				{
					FrontierSet.Add(Candidate);
				}
			}
		}

		TArray<FIntPoint> Frontier = FrontierSet.Array();
		Frontier.Sort([](const FIntPoint& A, const FIntPoint& B)
		{
			return A.X == B.X ? A.Y < B.Y : A.X < B.X;
		});

		int32 TotalWeight = 0;
		TArray<int32> Weights;
		Weights.Reserve(Frontier.Num());
		for (const FIntPoint& Candidate : Frontier)
		{
			const int32 Neighbours = CountOccupiedNeighbours(Candidate, Cells);
			const int32 Weight = 1 + Neighbours * Neighbours * 3;
			Weights.Add(Weight);
			TotalWeight += Weight;
		}

		int32 Choice = Frontier.IsEmpty() ? 0 : Random.RandRange(1, TotalWeight);
		int32 Index = 0;
		bool bCellAdded = false;
		while (Index < Frontier.Num() && !bCellAdded)
		{
			Choice -= Weights[Index];
			if (Choice <= 0)
			{
				Cells.Add(Frontier[Index]);
				bCellAdded = true;
			}
			++Index;
		}
	}

	static TSet<FIntPoint> FindExteriorEmpty(const TSet<FIntPoint>& Cells, const int32 Width, const int32 Height)
	{
		TSet<FIntPoint> Exterior;
		TArray<FIntPoint> Queue;
		Queue.Add(FIntPoint(-1, -1));
		Exterior.Add(Queue[0]);

		for (int32 QueueIndex = 0; QueueIndex < Queue.Num(); ++QueueIndex)
		{
			const FIntPoint Current = Queue[QueueIndex];
			for (const FIntPoint& Direction : Directions)
			{
				const FIntPoint Next = Current + Direction;
				const bool bCanVisit = Next.X >= -1 && Next.X <= Width &&
					Next.Y >= -1 && Next.Y <= Height && !Cells.Contains(Next) &&
					!Exterior.Contains(Next);
				if (bCanVisit)
				{
					Exterior.Add(Next);
					Queue.Add(Next);
				}
			}
		}
		return Exterior;
	}

	static void FillInteriorHoles(TSet<FIntPoint>& Cells, const int32 Width, const int32 Height)
	{
		const TSet<FIntPoint> Exterior = FindExteriorEmpty(Cells, Width, Height);
		for (int32 X = 0; X < Width; ++X)
		{
			for (int32 Y = 0; Y < Height; ++Y)
			{
				const FIntPoint Point(X, Y);
				if (!Cells.Contains(Point) && !Exterior.Contains(Point))
				{
					Cells.Add(Point);
				}
			}
		}
	}

	static FRoomLayout GenerateLayout(const FRoomConfig& Config, const int32 Seed)
	{
		FRoomLayout Layout;
		FRandomStream Random(Seed);

		const int32 MaxTilesX = FMath::Clamp(FMath::FloorToInt((Config.HalfWidth * 2.0) / Config.TileSize), 2, 64);
		const int32 MaxTilesY = FMath::Clamp(FMath::FloorToInt((Config.HalfLength * 2.0) / Config.TileSize), 2, 64);
		const int32 MinTilesX = FMath::Min(Config.MinTilesX, MaxTilesX);
		const int32 MinTilesY = FMath::Min(Config.MinTilesY, MaxTilesY);

		Layout.TilesX = Random.RandRange(MinTilesX, MaxTilesX);
		Layout.TilesY = Random.RandRange(MinTilesY, MaxTilesY);
		Layout.bIrregular = Random.FRand() < Config.IrregularChance && Layout.TilesX * Layout.TilesY > 8;

		if (!Layout.bIrregular)
		{
			for (int32 X = 0; X < Layout.TilesX; ++X)
			{
				for (int32 Y = 0; Y < Layout.TilesY; ++Y)
				{
					Layout.CellSet.Add(FIntPoint(X, Y));
				}
			}
		}
		else
		{
			const int32 Area = Layout.TilesX * Layout.TilesY;
			const double RatioVariation = Random.FRandRange(-0.08f, 0.08f);
			const int32 TargetCells = FMath::Clamp(
				FMath::RoundToInt(Area * FMath::Clamp(Config.IrregularFillRatio + RatioVariation, 0.35, 0.95)),
				FMath::Min(Area, 6),
				FMath::Max(1, Area - 1));

			const int32 CenterX = (Layout.TilesX - 1) / 2;
			const int32 CenterY = (Layout.TilesY - 1) / 2;
			Layout.CellSet.Add(FIntPoint(CenterX, CenterY));
			if (Layout.TilesX > 1)
			{
				Layout.CellSet.Add(FIntPoint(FMath::Min(CenterX + 1, Layout.TilesX - 1), CenterY));
			}
			if (Layout.TilesY > 1)
			{
				Layout.CellSet.Add(FIntPoint(CenterX, FMath::Min(CenterY + 1, Layout.TilesY - 1)));
			}
			if (CenterX > 0)
			{
				Layout.CellSet.Add(FIntPoint(CenterX - 1, CenterY));
			}
			if (CenterY > 0)
			{
				Layout.CellSet.Add(FIntPoint(CenterX, CenterY - 1));
			}

			bool bCanAddCells = true;
			while (Layout.CellSet.Num() < TargetCells && bCanAddCells)
			{
				const int32 PreviousCount = Layout.CellSet.Num();
				AddConnectedCell(Layout.CellSet, Layout.TilesX, Layout.TilesY, Random);
				if (Layout.CellSet.Num() == PreviousCount)
				{
					bCanAddCells = false;
				}
			}

			FillInteriorHoles(Layout.CellSet, Layout.TilesX, Layout.TilesY);
		}

		Layout.Cells = Layout.CellSet.Array();
		Layout.Cells.Sort([](const FIntPoint& A, const FIntPoint& B)
		{
			return A.X == B.X ? A.Y < B.Y : A.X < B.X;
		});

		const TSet<FIntPoint> ExteriorEmpty = FindExteriorEmpty(Layout.CellSet, Layout.TilesX, Layout.TilesY);
		const double OriginX = -0.5 * static_cast<double>(Layout.TilesX - 1) * Config.TileSize;
		const double OriginY = -0.5 * static_cast<double>(Layout.TilesY - 1) * Config.TileSize;

		for (const FIntPoint& Cell : Layout.Cells)
		{
			const FVector CellPosition(OriginX + Cell.X * Config.TileSize, OriginY + Cell.Y * Config.TileSize, 0.0);
			for (const FIntPoint& Direction : Directions)
			{
				const FIntPoint Neighbour = Cell + Direction;
				if (!Layout.CellSet.Contains(Neighbour))
				{
					FBoundaryEdge& Edge = Layout.BoundaryEdges.Emplace_GetRef();
					Edge.Cell = Cell;
					Edge.Direction = Direction;
					Edge.LocalPosition = CellPosition + FVector(Direction.X, Direction.Y, 0.0) * (Config.TileSize * 0.5);
					Edge.WallYaw = Direction.X == 0 ? 0.0 : 90.0;
					Edge.ExitYaw = FMath::RadiansToDegrees(FMath::Atan2(static_cast<double>(Direction.Y), static_cast<double>(Direction.X)));
					Edge.bExterior = ExteriorEmpty.Contains(Neighbour);
				}
			}
		}

		double BestScore = -1.0;
		for (int32 Index = 0; Index < Layout.BoundaryEdges.Num(); ++Index)
		{
			const FBoundaryEdge& Edge = Layout.BoundaryEdges[Index];
			if (Edge.bExterior)
			{
				const double DistanceScore = Edge.LocalPosition.SizeSquared2D();
				const double Score = DistanceScore + Random.FRandRange(0.0f, static_cast<float>(Config.TileSize * Config.TileSize * 2.0));
				if (Score > BestScore)
				{
					BestScore = Score;
					Layout.ExitEdgeIndex = Index;
				}
			}
		}

		return Layout;
	}

	static FTransform ToWorldTransform(const FTransform& LocalTransform, const FTransform& ActorTransform)
	{
		FTransform Result = LocalTransform;
		Result *= ActorTransform;
		return Result;
	}

	static void AppendPoint(
		TArray<FPCGPoint>& Points,
		const FTransform& Transform,
		const FVector& BoundsExtent)
	{
		FPCGPoint& Point = Points.Emplace_GetRef(Transform, 1.0f, PCGHelpers::ComputeSeedFromPosition(Transform.GetLocation()));
		Point.SetExtents(BoundsExtent);
		Point.Steepness = 1.0f;
	}

	struct FObstaclePlacement
	{
		FIntPoint Cell = FIntPoint::ZeroValue;
		double Yaw = 0.0;
	};

	struct FGeneratedPropResult
	{
		int32 Obstacles = 0;
		int32 Decorations = 0;
		TSet<FIntPoint> ReservedFloorCells;
		TSet<FIntPoint> BlockingFloorCells;
	};

	static TArray<FIntPoint> ChooseEnemyCells(const FRoomLayout& Layout)
	{
		const FIntPoint PlayerArrivalCell(
			(Layout.TilesX - 1) / 2 + 1,
			(Layout.TilesY - 1) / 2);
		TArray<FIntPoint> PreferredCells = Layout.Cells;
		PreferredCells.Sort([PlayerArrivalCell](const FIntPoint& A, const FIntPoint& B)
		{
			const int32 AX = A.X - PlayerArrivalCell.X;
			const int32 AY = A.Y - PlayerArrivalCell.Y;
			const int32 BX = B.X - PlayerArrivalCell.X;
			const int32 BY = B.Y - PlayerArrivalCell.Y;
			return AX * AX + AY * AY > BX * BX + BY * BY;
		});

		TArray<FIntPoint> Result;
		Result.Reserve(2);
		int32 PreferredIndex = 0;
		while (PreferredIndex < PreferredCells.Num() && Result.Num() < 2)
		{
			const FIntPoint& Cell = PreferredCells[PreferredIndex];
			if (Result.IsEmpty() ||
				FMath::Abs(Cell.X - Result[0].X) + FMath::Abs(Cell.Y - Result[0].Y) >= 6)
			{
				Result.Add(Cell);
			}
			++PreferredIndex;
		}

		if (Result.Num() < 2)
		{
			PreferredIndex = 0;
			while (PreferredIndex < PreferredCells.Num() && Result.Num() < 2)
			{
				const FIntPoint& Cell = PreferredCells[PreferredIndex];
				if (!Result.Contains(Cell))
				{
					Result.Add(Cell);
				}
				++PreferredIndex;
			}
		}
		return Result;
	}

	static void ReserveCellAndNeighbours(
		const FIntPoint& Cell,
		const TSet<FIntPoint>& RoomCells,
		TSet<FIntPoint>& ReservedCells)
	{
		if (RoomCells.Contains(Cell))
		{
			ReservedCells.Add(Cell);
		}
		for (const FIntPoint& Direction : Directions)
		{
			if (RoomCells.Contains(Cell + Direction))
			{
				ReservedCells.Add(Cell + Direction);
			}
		}
	}

	static bool LeavesRoomConnected(
		const TSet<FIntPoint>& RoomCells,
		const TSet<FIntPoint>& BlockedCells)
	{
		FIntPoint Start = FIntPoint::ZeroValue;
		bool bFoundStart = false;
		TArray<FIntPoint> RoomCellArray = RoomCells.Array();
		int32 StartIndex = 0;
		while (StartIndex < RoomCellArray.Num() && !bFoundStart)
		{
			const FIntPoint& Cell = RoomCellArray[StartIndex];
			if (!BlockedCells.Contains(Cell))
			{
				Start = Cell;
				bFoundStart = true;
			}
			++StartIndex;
		}
		TSet<FIntPoint> Visited;
		TArray<FIntPoint> Queue;
		if (bFoundStart)
		{
			Visited.Add(Start);
			Queue.Add(Start);
		}
		for (int32 QueueIndex = 0; QueueIndex < Queue.Num(); ++QueueIndex)
		{
			for (const FIntPoint& Direction : Directions)
			{
				const FIntPoint Next = Queue[QueueIndex] + Direction;
				if (RoomCells.Contains(Next) &&
					!BlockedCells.Contains(Next) &&
					!Visited.Contains(Next))
				{
					Visited.Add(Next);
					Queue.Add(Next);
				}
			}
		}
		const bool bConnected = bFoundStart &&
			Visited.Num() == RoomCells.Num() - BlockedCells.Num();
		return bConnected;
	}

	struct FRoomAccessibilityResult
	{
		bool bPlayable = false;
		int32 ReachableTiles = 0;
		int32 WalkableTiles = 0;
		TArray<FIntPoint> UnreachableTiles;
		FString Message;
	};

	static FRoomAccessibilityResult ValidateRoomAccessibility(
		const FRoomLayout& Layout,
		const TSet<FIntPoint>& BlockingCells)
	{
		FRoomAccessibilityResult Result;
		Result.WalkableTiles = Layout.CellSet.Num();
		for (const FIntPoint& BlockedCell : BlockingCells)
		{
			if (Layout.CellSet.Contains(BlockedCell))
			{
				--Result.WalkableTiles;
			}
		}

		bool bCanValidate = Result.WalkableTiles > 0;
		if (!bCanValidate)
		{
			Result.Message = TEXT("La sala no contiene ninguna baldosa transitable.");
		}

		const FIntPoint CentreCell(
			(Layout.TilesX - 1) / 2,
			(Layout.TilesY - 1) / 2);
		FIntPoint ArrivalCell(CentreCell.X + 1, CentreCell.Y);
		if (bCanValidate && (!Layout.CellSet.Contains(ArrivalCell) ||
			BlockingCells.Contains(ArrivalCell)))
		{
			ArrivalCell = CentreCell;
		}
		if (bCanValidate && (!Layout.CellSet.Contains(ArrivalCell) ||
			BlockingCells.Contains(ArrivalCell)))
		{
			Result.Message =
				TEXT("La baldosa de llegada del jugador esta bloqueada.");
			bCanValidate = false;
		}

		TSet<FIntPoint> Visited;
		TArray<FIntPoint> Queue;
		if (bCanValidate)
		{
			Visited.Add(ArrivalCell);
			Queue.Add(ArrivalCell);
		}
		for (int32 QueueIndex = 0; QueueIndex < Queue.Num(); ++QueueIndex)
		{
			for (const FIntPoint& Direction : Directions)
			{
				const FIntPoint Next = Queue[QueueIndex] + Direction;
				if (Layout.CellSet.Contains(Next) &&
					!BlockingCells.Contains(Next) &&
					!Visited.Contains(Next))
				{
					Visited.Add(Next);
					Queue.Add(Next);
				}
			}
		}

		if (bCanValidate)
		{
			Result.ReachableTiles = Visited.Num();
			for (const FIntPoint& Cell : Layout.Cells)
			{
				if (!BlockingCells.Contains(Cell) && !Visited.Contains(Cell))
				{
					Result.UnreachableTiles.Add(Cell);
				}
			}

			Result.bPlayable =
				Result.ReachableTiles == Result.WalkableTiles &&
				Result.UnreachableTiles.IsEmpty();
			Result.Message = Result.bPlayable
				? FString::Printf(
					TEXT("Sala jugable: las %d baldosas transitables estan conectadas."),
					Result.WalkableTiles)
				: FString::Printf(
					TEXT("Sala no jugable: %d de %d baldosas son alcanzables; %d han quedado aisladas."),
					Result.ReachableTiles,
					Result.WalkableTiles,
					Result.UnreachableTiles.Num());
		}
		return Result;
	}

	static TArray<FObstaclePlacement> ChooseObstaclePlacements(
		const FRoomLayout& Layout,
		const TSet<FIntPoint>& ReservedCells,
		FRandomStream& Random)
	{
		TArray<FIntPoint> Candidates;
		for (const FIntPoint& Cell : Layout.Cells)
		{
			if (!ReservedCells.Contains(Cell) &&
				CountOccupiedNeighbours(Cell, Layout.CellSet) >= 3)
			{
				Candidates.Add(Cell);
			}
		}
		if (Candidates.Num() < 3)
		{
			for (const FIntPoint& Cell : Layout.Cells)
			{
				if (!ReservedCells.Contains(Cell) &&
					!Candidates.Contains(Cell) &&
					CountOccupiedNeighbours(Cell, Layout.CellSet) >= 2)
				{
					Candidates.Add(Cell);
				}
			}
		}

		const int32 TargetCount = FMath::Clamp(Layout.Cells.Num() / 14 + 2, 3, 9);
		TSet<FIntPoint> BlockedCells;
		TArray<FObstaclePlacement> Result;
		for (int32 Attempt = 0;
			Attempt < 96 && Result.Num() < TargetCount && !Candidates.IsEmpty();
			++Attempt)
		{
			const FIntPoint SeedCell = Candidates[Random.RandRange(0, Candidates.Num() - 1)];
			const FIntPoint Axis =
				Random.RandRange(0, 1) == 0 ? FIntPoint(1, 0) : FIntPoint(0, 1);
			const int32 Offsets[] = {0, 1, -1};
			const int32 GroupLength =
				FMath::Min(Random.RandRange(1, 3), TargetCount - Result.Num());

			for (int32 GroupIndex = 0; GroupIndex < GroupLength; ++GroupIndex)
			{
				const FIntPoint Candidate = SeedCell + Axis * Offsets[GroupIndex];
				const bool bCandidateAvailable = Candidates.Contains(Candidate) &&
					!BlockedCells.Contains(Candidate);
				if (bCandidateAvailable)
				{
					TSet<FIntPoint> TrialBlocked = BlockedCells;
					TrialBlocked.Add(Candidate);
					if (LeavesRoomConnected(Layout.CellSet, TrialBlocked))
					{
						BlockedCells.Add(Candidate);
						FObstaclePlacement& Placement = Result.Emplace_GetRef();
						Placement.Cell = Candidate;
						Placement.Yaw = Axis.X != 0 ? 0.0 : 90.0;
					}
				}
			}
		}
		return Result;
	}

	static AStaticMeshActor* SpawnRoomProp(
		AActor* RoomActor,
		const TCHAR* AssetPath,
		const FVector& LocalPosition2D,
		const FRotator& Rotation,
		const FVector& Scale,
		const double TileSize,
		const bool bObstacle,
		const bool bBlocksMovement)
	{
		UWorld* World = IsValid(RoomActor) ? RoomActor->GetWorld() : nullptr;
		UStaticMesh* Mesh = World ? LoadObject<UStaticMesh>(nullptr, AssetPath) : nullptr;
		if (!Mesh)
		{
			if (World)
			{
				UE_LOG(LogTemp, Warning, TEXT("Procedural room prop could not load: %s"), AssetPath);
			}
		}
		AStaticMeshActor* Prop = nullptr;
		if (Mesh)
		{
		const FBoxSphereBounds MeshBounds = Mesh->GetBounds();
		FVector LocalPosition = LocalPosition2D;
		LocalPosition.Z = -(MeshBounds.Origin.Z - MeshBounds.BoxExtent.Z) * Scale.Z + 2.0;
		const FTransform LocalTransform(Rotation, LocalPosition, Scale);
		const FTransform WorldTransform =
			ToWorldTransform(LocalTransform, RoomActor->GetActorTransform());

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = RoomActor;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Prop = World->SpawnActor<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(),
			WorldTransform,
			SpawnParameters);
		if (Prop)
		{
		Prop->Tags.AddUnique(ProceduralPropTag);
		Prop->Tags.AddUnique(
			bObstacle ? ProceduralObstacleTag : ProceduralLegacyDecorationTag);
		UStaticMeshComponent* MeshComponent = Prop->GetStaticMeshComponent();
		MeshComponent->SetMobility(EComponentMobility::Movable);
		MeshComponent->SetStaticMesh(Mesh);
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComponent->SetCanEverAffectNavigation(false);

		if (bBlocksMovement)
		{
			const FName CollisionName = bObstacle
				? TEXT("ProceduralObstacleCollision")
				: TEXT("ProceduralVaseCollision");
			UBoxComponent* BlockingBox =
				NewObject<UBoxComponent>(Prop, CollisionName);
			Prop->AddInstanceComponent(BlockingBox);
			BlockingBox->SetupAttachment(MeshComponent);
			const FVector SafeScale(
				FMath::Max(0.01, Scale.X),
				FMath::Max(0.01, Scale.Y),
				FMath::Max(0.01, Scale.Z));
			const double MaxWorldHalfWidth = TileSize * 0.34;
			BlockingBox->SetRelativeLocation(MeshBounds.Origin);
			BlockingBox->SetBoxExtent(FVector(
				FMath::Min(MeshBounds.BoxExtent.X, MaxWorldHalfWidth / SafeScale.X),
				FMath::Min(MeshBounds.BoxExtent.Y, MaxWorldHalfWidth / SafeScale.Y),
				FMath::Max(MeshBounds.BoxExtent.Z, 50.0 / SafeScale.Z)));
			BlockingBox->SetCollisionObjectType(ECC_WorldStatic);
			BlockingBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			BlockingBox->SetCollisionResponseToAllChannels(ECR_Block);
			BlockingBox->SetGenerateOverlapEvents(false);
			BlockingBox->SetCanEverAffectNavigation(true);
			BlockingBox->RegisterComponent();

			if (!bObstacle)
			{
				UE_LOG(
					LogTemp,
					Display,
					TEXT("Procedural decorative vase spawned with blocking collision: %s."),
					*Prop->GetName());
			}
		}
		}
		}
		return Prop;
	}

	static FGeneratedPropResult SpawnRoomProps(
		AActor* RoomActor,
		const FRoomConfig& Config,
		const FRoomLayout& Layout,
		const int32 RoomSeed,
		const TArray<FIntPoint>& EnemyCells)
	{
		FGeneratedPropResult Result;
		FRandomStream Random(RoomSeed ^ 0x51A7C3D);
		const FIntPoint CentreCell((Layout.TilesX - 1) / 2, (Layout.TilesY - 1) / 2);
		const FIntPoint PlayerArrivalCell(CentreCell.X + 1, CentreCell.Y);
		ReserveCellAndNeighbours(CentreCell, Layout.CellSet, Result.ReservedFloorCells);
		ReserveCellAndNeighbours(
			PlayerArrivalCell,
			Layout.CellSet,
			Result.ReservedFloorCells);
		for (const FIntPoint& EnemyCell : EnemyCells)
		{
			ReserveCellAndNeighbours(
				EnemyCell,
				Layout.CellSet,
				Result.ReservedFloorCells);
		}

		const double OriginX =
			-0.5 * static_cast<double>(Layout.TilesX - 1) * Config.TileSize;
		const double OriginY =
			-0.5 * static_cast<double>(Layout.TilesY - 1) * Config.TileSize;
		const TArray<FObstaclePlacement> Obstacles =
			ChooseObstaclePlacements(Layout, Result.ReservedFloorCells, Random);
		const TCHAR* ObstacleAssets[] =
		{
			TEXT("/Game/SoulCave/Environment/Meshes/Building/SM_Cave_Rock_Pillar_Broken01a.SM_Cave_Rock_Pillar_Broken01a"),
			TEXT("/Game/OldBrokenPillar/Meshes/Pillar_001/SM_Pillar_001.SM_Pillar_001"),
			TEXT("/Game/ObsidianGoldOrnates_YV/StaticMeshes/SM_BlackDecor_16.SM_BlackDecor_16")
		};
		const double ObstacleBaseScales[] = {0.78, 2.25, 0.82};
		const int32 ObstacleAssetOffset =
			Random.RandRange(0, UE_ARRAY_COUNT(ObstacleAssets) - 1);
		TSet<FIntPoint> ObstacleCells;
		for (int32 Index = 0; Index < Obstacles.Num(); ++Index)
		{
			const FObstaclePlacement& Placement = Obstacles[Index];
			ObstacleCells.Add(Placement.Cell);
			Result.ReservedFloorCells.Add(Placement.Cell);
			const int32 AssetIndex =
				(Index + ObstacleAssetOffset) % UE_ARRAY_COUNT(ObstacleAssets);
			const double UniformScale =
				ObstacleBaseScales[AssetIndex] * Random.FRandRange(0.88f, 1.12f);
			const FVector LocalPosition(
				OriginX + Placement.Cell.X * Config.TileSize,
				OriginY + Placement.Cell.Y * Config.TileSize,
				0.0);
			if (SpawnRoomProp(
				RoomActor,
				ObstacleAssets[AssetIndex],
				LocalPosition,
				FRotator(
					Random.FRandRange(-3.0f, 3.0f),
					Placement.Yaw + Random.FRandRange(-10.0f, 10.0f),
					Random.FRandRange(-3.0f, 3.0f)),
				FVector(UniformScale),
				Config.TileSize,
				true,
				true))
			{
				Result.BlockingFloorCells.Add(Placement.Cell);
				++Result.Obstacles;
			}
		}

		TArray<FIntPoint> DecorationCells;
		for (const FIntPoint& Cell : Layout.Cells)
		{
			if (!Result.ReservedFloorCells.Contains(Cell) &&
				!ObstacleCells.Contains(Cell))
			{
				DecorationCells.Add(Cell);
			}
		}
		const TCHAR* DecorationAssets[] =
		{
			TEXT("/Game/SoulCave/Environment/Meshes/Bricks/SM_Cave_Brick_Pile02.SM_Cave_Brick_Pile02"),
			TEXT("/Game/SoulCave/Environment/Meshes/Bricks/SM_Cave_Brick_Pile02.SM_Cave_Brick_Pile02"),
			TEXT("/Game/SoulCave/Environment/Meshes/Bricks/SM_Cave_Brick_Pile02.SM_Cave_Brick_Pile02"),
			TEXT("/Game/ObsidianGoldOrnates_YV/StaticMeshes/SM_IbsidianGoldOrnates_18.SM_IbsidianGoldOrnates_18")
		};
		const double DecorationBaseScales[] = {0.82, 0.92, 1.04, 1.15};
		const int32 DecorationCount = FMath::Min(
			FMath::Clamp(Layout.Cells.Num() / 7, 6, 18),
			DecorationCells.Num());
		const int32 DecorationAssetOffset =
			Random.RandRange(0, UE_ARRAY_COUNT(DecorationAssets) - 1);
		for (int32 Index = 0; Index < DecorationCount; ++Index)
		{
			const int32 CellIndex = Random.RandRange(0, DecorationCells.Num() - 1);
			const FIntPoint Cell = DecorationCells[CellIndex];
			DecorationCells.RemoveAtSwap(CellIndex);
			Result.ReservedFloorCells.Add(Cell);
			const int32 AssetIndex =
				(Index + DecorationAssetOffset) % UE_ARRAY_COUNT(DecorationAssets);
			const bool bBlocksMovement = AssetIndex == 3;
			bool bCanPlaceDecoration = true;
			if (bBlocksMovement)
			{
				TSet<FIntPoint> TrialBlockingCells =
					Result.BlockingFloorCells;
				TrialBlockingCells.Add(Cell);
				if (!LeavesRoomConnected(
					Layout.CellSet,
					TrialBlockingCells))
				{
					UE_LOG(
						LogTemp,
						Display,
						TEXT("[VALIDACION SALA] Jarron descartado en (%d,%d): bloquearia el unico paso disponible."),
						Cell.X,
						Cell.Y);
					bCanPlaceDecoration = false;
				}
			}
			if (bCanPlaceDecoration)
			{
			const double UniformScale =
				DecorationBaseScales[AssetIndex] * Random.FRandRange(0.78f, 1.25f);
			const FVector LocalPosition(
				OriginX + Cell.X * Config.TileSize +
					Random.FRandRange(-0.22f, 0.22f) * Config.TileSize,
				OriginY + Cell.Y * Config.TileSize +
					Random.FRandRange(-0.22f, 0.22f) * Config.TileSize,
				0.0);
			if (SpawnRoomProp(
				RoomActor,
				DecorationAssets[AssetIndex],
				LocalPosition,
				FRotator(
					Random.FRandRange(-8.0f, 8.0f),
					90.0 * Random.RandRange(0, 3) +
						Random.FRandRange(-12.0f, 12.0f),
					Random.FRandRange(-8.0f, 8.0f)),
				FVector(UniformScale),
				Config.TileSize,
				false,
				bBlocksMovement))
			{
				if (bBlocksMovement)
				{
					Result.BlockingFloorCells.Add(Cell);
				}
				++Result.Decorations;
			}
			}
		}

		UE_LOG(
			LogTemp,
			Display,
			TEXT("Procedural room previous decoration preserved: %d obstacles, %d floor decorations."),
			Result.Obstacles,
			Result.Decorations);
		return Result;
	}

	static FGeneratedDecorationCounts CreateGeneratedDecorations(
		AActor* RoomActor,
		const FRoomConfig& Config,
		const FRoomLayout& Layout,
		const int32 RoomSeed,
		const TSet<FIntPoint>& ReservedFloorCells)
	{
		FGeneratedDecorationCounts Counts;
		if (IsValid(RoomActor) && !Layout.Cells.IsEmpty())
		{

		UStaticMesh* CeilingFlat = LoadObject<UStaticMesh>(
			nullptr,
			TEXT("/Game/MedievalDungeon/Meshes/Architecture/Dungeon/SM_Ceiling_Flat.SM_Ceiling_Flat"));
		UStaticMesh* OpaqueSealCube = LoadObject<UStaticMesh>(
			nullptr,
			TEXT("/Engine/BasicShapes/Cube.Cube"));
		UStaticMesh* TorchMesh = LoadObject<UStaticMesh>(
			nullptr,
			TEXT("/Game/MedievalDungeon/Meshes/Props/SM_Torch.SM_Torch"));
		UParticleSystem* TorchFire = LoadObject<UParticleSystem>(
			nullptr,
			TEXT("/Game/MedievalDungeon/Particles/P_Torch_Fire.P_Torch_Fire"));
		UParticleSystem* PitFire = LoadObject<UParticleSystem>(
			nullptr,
			TEXT("/Game/MedievalDungeon/Particles/P_Pit_Fire.P_Pit_Fire"));

		TArray<UStaticMesh*> CeilingMeshes;
		if (CeilingFlat)
		{
			CeilingMeshes.Add(CeilingFlat);
		}

		FRandomStream Random(RoomSeed ^ 0x5A17C3D);
		const double OriginX = -0.5 * static_cast<double>(Layout.TilesX - 1) * Config.TileSize;
		const double OriginY = -0.5 * static_cast<double>(Layout.TilesY - 1) * Config.TileSize;
		const double CeilingModuleScale = Config.TileSize / 600.0;

		// A complete ceiling follows the same occupied cells as the floor. An opaque,
		// thick, overlapping slab is generated above every visible module. Unlike a
		// second zero-thickness ceiling plane, these slabs also close the perimeter
		// and diagonal light paths that Lumen/skylight can otherwise see through.
		if (!CeilingMeshes.IsEmpty())
		{
			TArray<UHierarchicalInstancedStaticMeshComponent*> CeilingInstancers;
			CeilingInstancers.Reserve(CeilingMeshes.Num());
			for (int32 MeshIndex = 0; MeshIndex < CeilingMeshes.Num(); ++MeshIndex)
			{
				if (UHierarchicalInstancedStaticMeshComponent* Instancer =
					CreateDecorationInstancer(
					RoomActor,
					CeilingMeshes[MeshIndex],
					TEXT("ProceduralCeilingInstances")))
				{
					CeilingInstancers.Add(Instancer);
				}
			}

			UHierarchicalInstancedStaticMeshComponent* CeilingLightSeal =
				OpaqueSealCube
					? CreateDecorationInstancer(
						RoomActor,
						OpaqueSealCube,
						TEXT("ProceduralCeilingOpaqueSeal"),
						false)
					: nullptr;
			if (CeilingLightSeal && CeilingFlat && CeilingFlat->GetMaterial(0))
			{
				CeilingLightSeal->SetMaterial(0, CeilingFlat->GetMaterial(0));
			}

			if (CeilingInstancers.IsEmpty())
			{
				UE_LOG(
					LogTemp,
					Error,
					TEXT("Procedural room ceiling could not create any instancing component."));
			}
			else
			{
				const FBoxSphereBounds SealBounds = OpaqueSealCube
					? OpaqueSealCube->GetBounds()
					: FBoxSphereBounds(EForceInit::ForceInit);
				constexpr double CeilingSealOverlapMultiplier = 1.12;
				constexpr double CeilingSealThickness = 40.0;
				constexpr double CeilingSealBottomZ = GeneratedWallTopZ + 1.0;
				const FVector CeilingSealScale(
					Config.TileSize * CeilingSealOverlapMultiplier /
						FMath::Max(1.0, SealBounds.BoxExtent.X * 2.0),
					Config.TileSize * CeilingSealOverlapMultiplier /
						FMath::Max(1.0, SealBounds.BoxExtent.Y * 2.0),
					CeilingSealThickness /
						FMath::Max(1.0, SealBounds.BoxExtent.Z * 2.0));
				const double CeilingSealCenterZ =
					CeilingSealBottomZ + CeilingSealThickness * 0.5 -
					SealBounds.Origin.Z * CeilingSealScale.Z;

				for (const FIntPoint& Cell : Layout.Cells)
				{
					const int32 MeshIndex =
						Random.RandRange(0, CeilingInstancers.Num() - 1);
					UHierarchicalInstancedStaticMeshComponent* Instancer =
						CeilingInstancers[MeshIndex];
					UStaticMesh* CeilingMesh = Instancer->GetStaticMesh();
					const FBoxSphereBounds CeilingBounds = CeilingMesh->GetBounds();
					const double CeilingBottomZ =
						CeilingBounds.Origin.Z - CeilingBounds.BoxExtent.Z;
					const FVector LocalPosition(
						OriginX + Cell.X * Config.TileSize,
						OriginY + Cell.Y * Config.TileSize,
						GeneratedWallTopZ - CeilingBottomZ);
					const FRotator LocalRotation(
						0.0,
						90.0f * Random.RandRange(0, 3),
						0.0);
					Instancer->AddInstance(FTransform(
						LocalRotation,
						LocalPosition,
						FVector(CeilingModuleScale, CeilingModuleScale, 1.0)));
					if (CeilingLightSeal)
					{
						CeilingLightSeal->AddInstance(FTransform(
							FRotator::ZeroRotator,
							FVector(LocalPosition.X, LocalPosition.Y, CeilingSealCenterZ),
							CeilingSealScale));
					}
					++Counts.Ceilings;
				}

				if (!CeilingLightSeal)
				{
					UE_LOG(
						LogTemp,
						Warning,
						TEXT("Procedural room ceiling created without its opaque light-seal slabs; some seams may remain exposed."));
				}
			}

			if (Counts.Ceilings != Layout.Cells.Num())
			{
				UE_LOG(
					LogTemp,
					Error,
					TEXT("Procedural room ceiling incomplete: generated %d of %d required pieces."),
					Counts.Ceilings,
					Layout.Cells.Num());
			}
		}
		else
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("Procedural room ceiling assets could not be loaded."));
		}

		// Randomized wall torches point inward. Their mesh, fire particle and warm
		// movable point light reproduce the existing BP_Torch setup.
		TArray<int32> TorchEdges;
		for (int32 EdgeIndex = 0; EdgeIndex < Layout.BoundaryEdges.Num(); ++EdgeIndex)
		{
			const FBoundaryEdge& Edge = Layout.BoundaryEdges[EdgeIndex];
			if (Edge.bExterior && EdgeIndex != Layout.ExitEdgeIndex)
			{
				TorchEdges.Add(EdgeIndex);
			}
		}
		for (int32 Index = TorchEdges.Num() - 1; Index > 0; --Index)
		{
			TorchEdges.Swap(Index, Random.RandRange(0, Index));
		}

		const int32 DesiredTorches = FMath::Min(
			TorchEdges.Num(),
			FMath::Clamp(FMath::RoundToInt(TorchEdges.Num() * 0.20), 2, 10));
		for (int32 TorchIndex = 0; TorchIndex < DesiredTorches; ++TorchIndex)
		{
			const FBoundaryEdge& Edge = Layout.BoundaryEdges[TorchEdges[TorchIndex]];
			const FVector Inward(-Edge.Direction.X, -Edge.Direction.Y, 0.0);
			FVector LocalPosition = Edge.LocalPosition + Inward * 8.0;
			LocalPosition.Z = GeneratedTorchZ + Random.FRandRange(-35.0f, 35.0f);
			const FRotator LocalRotation(
				Random.FRandRange(-6.0f, 6.0f),
				Edge.ExitYaw + 90.0 + Random.FRandRange(-4.0f, 4.0f),
				Random.FRandRange(-5.0f, 5.0f));
			USceneComponent* Anchor = CreateDecorationAnchor(
				RoomActor,
				TEXT("ProceduralTorchAnchor"),
				FTransform(LocalRotation, LocalPosition));
			if (Anchor)
			{
				CreateDecorationMesh(
					RoomActor,
					Anchor,
					TorchMesh,
					TEXT("ProceduralTorchMesh"),
					FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(0.75)),
					false);
				CreateDecorationFire(
					RoomActor,
					Anchor,
					TorchFire,
					TEXT("ProceduralTorchFire"),
					FTransform(FRotator::ZeroRotator, FVector(0.0, 39.0, 33.0), FVector(0.75)));
				CreateDecorationLight(
					RoomActor,
					Anchor,
					TEXT("ProceduralTorchLight"),
					FVector(0.0, 31.0, 38.0),
					1400.0f,
					650.0f);
				++Counts.Torches;
			}
		}

		// Floor flames stay away from the portal, player, enemies and the restored
		// obstacle/decoration layer. Each one consists only of particles and its
		// own warm light source.
		const FIntPoint CenterCell((Layout.TilesX - 1) / 2, (Layout.TilesY - 1) / 2);
		TArray<FIntPoint> FireCells;
		TArray<FIntPoint> CoverageFireCells;
		for (const FIntPoint& Cell : Layout.Cells)
		{
			const int32 CenterDistance = FMath::Abs(Cell.X - CenterCell.X) + FMath::Abs(Cell.Y - CenterCell.Y);
			if (!ReservedFloorCells.Contains(Cell))
			{
				CoverageFireCells.Add(Cell);
				if (CenterDistance >= 3)
				{
					FireCells.Add(Cell);
				}
			}
		}
		for (int32 Index = FireCells.Num() - 1; Index > 0; --Index)
		{
			FireCells.Swap(Index, Random.RandRange(0, Index));
		}

		// Start with more, softer lights than before. Coverage is then measured at
		// the centre and four near-corners of every occupied floor tile. Extra fires
		// are added greedily around the darkest sample until the minimum is met.
		const int32 DesiredFires = FMath::Min(
			FireCells.Num(),
			FMath::Clamp(FMath::RoundToInt(Layout.Cells.Num() * 0.10), 4, 12));
		// Visible flames remain capped to avoid filling large irregular rooms with
		// particles. Any residual dark samples are handled by subtle fill lights.
		const int32 MaximumFiresForCoverage = FMath::Min(
			CoverageFireCells.Num(),
			FMath::Clamp(FMath::RoundToInt(Layout.Cells.Num() * 0.32), 10, 36));
		const double FireParticleScale = Config.TileSize / 600.0 * 1.6;
		constexpr float FloorFireIntensity = 480.0f;
		constexpr float FloorFireRadius = 550.0f;
		constexpr double MinimumFloorIlluminationScore = 12.0;
		TSet<FIntPoint> SelectedFireCells;
		struct FFloorLightSample
		{
			FVector Position = FVector::ZeroVector;
			float Intensity = 0.0f;
			float Radius = 0.0f;
		};
		TArray<FFloorLightSample> FloorLightSamples;

		auto SpawnFloorFire = [&](const FIntPoint& Cell, const bool bSupplemental)
		{
			bool bFireSpawned = false;
			if (!SelectedFireCells.Contains(Cell))
			{
			const FVector LocalPosition(
				OriginX + Cell.X * Config.TileSize,
				OriginY + Cell.Y * Config.TileSize,
				3.0);
			const FRotator LocalRotation(
				0.0,
				Random.FRandRange(0.0f, 360.0f),
				0.0);
			USceneComponent* Anchor = CreateDecorationAnchor(
				RoomActor,
				TEXT("ProceduralFloorFireAnchor"),
				FTransform(LocalRotation, LocalPosition));
			if (Anchor)
			{
			// Floor fires intentionally have no prop mesh: only the flame particles
			// and their warm point light are generated directly over the floor.
			CreateDecorationFire(
				RoomActor,
				Anchor,
				PitFire,
				TEXT("ProceduralPitFire"),
				FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(FireParticleScale)));
			UPointLightComponent* FloorLight = CreateDecorationLight(
				RoomActor,
				Anchor,
				TEXT("ProceduralPitLight"),
				FVector(0.0, 0.0, 90.0),
				FloorFireIntensity,
				FloorFireRadius);
			if (FloorLight)
			{
			SelectedFireCells.Add(Cell);
			FloorLightSamples.Add({
				LocalPosition + FVector(0.0, 0.0, 90.0),
				FloorFireIntensity,
				FloorFireRadius});
			++Counts.FloorFires;
			if (bSupplemental)
			{
				UE_LOG(
					LogTemp,
					Verbose,
					TEXT("Procedural illumination added a supplemental floor fire at cell (%d,%d)."),
					Cell.X,
					Cell.Y);
			}
			bFireSpawned = true;
			}
			}
			}
			return bFireSpawned;
		};

		for (int32 FireIndex = 0; FireIndex < DesiredFires; ++FireIndex)
		{
			SpawnFloorFire(FireCells[FireIndex], false);
		}

		auto EstimateFloorIllumination = [&](const FVector& Sample)
		{
			double Score = 0.0;
			for (const FFloorLightSample& Light : FloorLightSamples)
			{
				const double Distance = FVector::Dist2D(Sample, Light.Position);
				if (Distance < Light.Radius)
				{
					const double NormalizedFalloff =
						1.0 - Distance / Light.Radius;
					Score += Light.Intensity * FMath::Square(NormalizedFalloff);
				}
			}
			return Score;
		};

		TArray<FVector> FloorSamples;
		FloorSamples.Reserve(Layout.Cells.Num() * 5);
		const double SampleOffset = Config.TileSize * 0.42;
		for (const FIntPoint& Cell : Layout.Cells)
		{
			const FVector Center(
				OriginX + Cell.X * Config.TileSize,
				OriginY + Cell.Y * Config.TileSize,
				0.0);
			FloorSamples.Add(Center);
			FloorSamples.Add(Center + FVector(SampleOffset, SampleOffset, 0.0));
			FloorSamples.Add(Center + FVector(SampleOffset, -SampleOffset, 0.0));
			FloorSamples.Add(Center + FVector(-SampleOffset, SampleOffset, 0.0));
			FloorSamples.Add(Center + FVector(-SampleOffset, -SampleOffset, 0.0));
		}

		bool bCanAddCoverageFire = true;
		while (Counts.FloorFires < MaximumFiresForCoverage && bCanAddCoverageFire)
		{
			double DarkestScore = TNumericLimits<double>::Max();
			FVector DarkestSample = FVector::ZeroVector;
			for (const FVector& Sample : FloorSamples)
			{
				const double Score = EstimateFloorIllumination(Sample);
				if (Score < DarkestScore)
				{
					DarkestScore = Score;
					DarkestSample = Sample;
				}
			}

			if (DarkestScore >= MinimumFloorIlluminationScore)
			{
				bCanAddCoverageFire = false;
			}
			else
			{
			int32 NearestCandidateIndex = INDEX_NONE;
			double NearestDistanceSquared = TNumericLimits<double>::Max();
			for (int32 CandidateIndex = 0; CandidateIndex < CoverageFireCells.Num(); ++CandidateIndex)
			{
				const FIntPoint& Candidate = CoverageFireCells[CandidateIndex];
				if (!SelectedFireCells.Contains(Candidate))
				{
					const FVector CandidatePosition(
						OriginX + Candidate.X * Config.TileSize,
						OriginY + Candidate.Y * Config.TileSize,
						0.0);
					const double DistanceSquared =
						FVector::DistSquared2D(DarkestSample, CandidatePosition);
					if (DistanceSquared < NearestDistanceSquared)
					{
						NearestDistanceSquared = DistanceSquared;
						NearestCandidateIndex = CandidateIndex;
					}
				}
			}

			if (NearestCandidateIndex == INDEX_NONE ||
				!SpawnFloorFire(CoverageFireCells[NearestCandidateIndex], true))
			{
				bCanAddCoverageFire = false;
			}
			}
		}

		// Reserved gameplay cells may not accept visible fire particles. Place a
		// weak, warm, non-shadow-casting fill light above the darkest remaining
		// sample, then re-evaluate. This guarantees the minimum without visual clutter.
		constexpr float CoverageFillIntensity = 220.0f;
		constexpr float CoverageFillRadius = 500.0f;
		constexpr double CoverageFillHeight = 240.0;
		int32 CoverageFillLights = 0;
		bool bCanAddFillLight = true;
		while (CoverageFillLights < FloorSamples.Num() && bCanAddFillLight)
		{
			double DarkestScore = TNumericLimits<double>::Max();
			FVector DarkestSample = FVector::ZeroVector;
			for (const FVector& Sample : FloorSamples)
			{
				const double Score = EstimateFloorIllumination(Sample);
				if (Score < DarkestScore)
				{
					DarkestScore = Score;
					DarkestSample = Sample;
				}
			}
			if (DarkestScore >= MinimumFloorIlluminationScore)
			{
				bCanAddFillLight = false;
			}
			else
			{
			const FVector FillPosition(
				DarkestSample.X,
				DarkestSample.Y,
				CoverageFillHeight);
			USceneComponent* FillAnchor = CreateDecorationAnchor(
				RoomActor,
				TEXT("ProceduralIlluminationFillAnchor"),
				FTransform(FRotator::ZeroRotator, FillPosition));
			UPointLightComponent* FillLight = FillAnchor
				? CreateDecorationLight(
					RoomActor,
					FillAnchor,
					TEXT("ProceduralIlluminationFillLight"),
					FVector::ZeroVector,
					CoverageFillIntensity,
					CoverageFillRadius)
				: nullptr;
			if (!FillLight)
			{
				bCanAddFillLight = false;
			}
			else
			{
				FillLight->SetCastShadows(false);
				FloorLightSamples.Add({
					FillPosition,
					CoverageFillIntensity,
					CoverageFillRadius});
				++CoverageFillLights;
			}
			}
		}

		double MinimumMeasuredScore = TNumericLimits<double>::Max();
		int32 SamplesBelowMinimum = 0;
		for (const FVector& Sample : FloorSamples)
		{
			const double Score = EstimateFloorIllumination(Sample);
			MinimumMeasuredScore = FMath::Min(MinimumMeasuredScore, Score);
			SamplesBelowMinimum += Score < MinimumFloorIlluminationScore ? 1 : 0;
		}
		if (SamplesBelowMinimum == 0)
		{
			UE_LOG(
				LogTemp,
				Display,
				TEXT("Procedural floor illumination verification: %d fires, %d fill lights, %d samples, minimum score %.1f (target %.1f), all samples covered."),
				Counts.FloorFires,
				CoverageFillLights,
				FloorSamples.Num(),
				MinimumMeasuredScore,
				MinimumFloorIlluminationScore);
		}
		else
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("Procedural floor illumination verification: %d fires, %d fill lights, %d samples, minimum score %.1f (target %.1f), %d below target."),
				Counts.FloorFires,
				CoverageFillLights,
				FloorSamples.Num(),
				MinimumMeasuredScore,
				MinimumFloorIlluminationScore,
				SamplesBelowMinimum);
		}
		}
		return Counts;
	}
}

#if WITH_EDITOR
FText UPCGRandomRoomSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Random Room Shape");
}

FText UPCGRandomRoomSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Generates connected regular or irregular room footprints, tall perimeter walls and a deterministic exit opening at runtime. Runtime preparation adds ceilings, torches and floor fires.");
}
#endif

TArray<FPCGPinProperties> UPCGRandomRoomSettings::InputPinProperties() const
{
	return {};
}

TArray<FPCGPinProperties> UPCGRandomRoomSettings::OutputPinProperties() const
{
	return
	{
		FPCGPinProperties(FloorPinLabel, FPCGDataTypeIdentifier{EPCGDataType::Point}),
		FPCGPinProperties(WallsPinLabel, FPCGDataTypeIdentifier{EPCGDataType::Point})
	};
}

FPCGElementPtr UPCGRandomRoomSettings::CreateElement() const
{
	return MakeShared<FPCGRandomRoomElement>();
}

bool FPCGRandomRoomElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);

	const UPCGRandomRoomSettings* Settings = Context->GetInputSettings<UPCGRandomRoomSettings>();
	AActor* Actor = Settings && Context->ExecutionSource.IsValid()
		? Context->ExecutionSource->GetExecutionState().GetTypedTarget<AActor>()
		: nullptr;
	if (Actor)
	{
	const ProceduralRoom::FRoomConfig Config = ProceduralRoom::ReadConfig(Actor, Settings);
	int32 RoomSeed = FMath::RoundToInt(ProceduralRoom::ReadNumber(Actor, TEXT("RoomSeed"), 0.0));
	if (RoomSeed == 0)
	{
		RoomSeed = Context->GetSeed();
	}

	const ProceduralRoom::FRoomLayout Layout = ProceduralRoom::GenerateLayout(Config, RoomSeed);
	FTransform ActorTransform = Context->ExecutionSource->GetExecutionState().GetTransform();
	ActorTransform.SetScale3D(FVector::OneVector);

	TArray<FPCGPoint> FloorPoints;
	TArray<FPCGPoint> WallPoints;
	FloorPoints.Reserve(Layout.Cells.Num());
	WallPoints.Reserve(Layout.BoundaryEdges.Num());

	const double OriginX = -0.5 * static_cast<double>(Layout.TilesX - 1) * Config.TileSize;
	const double OriginY = -0.5 * static_cast<double>(Layout.TilesY - 1) * Config.TileSize;
	const double ModuleScale = Config.TileSize / FMath::Max(1.0, Settings->NativeModuleSize);

	for (const FIntPoint& Cell : Layout.Cells)
	{
		const FVector LocalPosition(OriginX + Cell.X * Config.TileSize, OriginY + Cell.Y * Config.TileSize, Settings->FloorZ);
		const FTransform LocalTransform(FRotator::ZeroRotator, LocalPosition, FVector(ModuleScale, ModuleScale, 1.0));
		ProceduralRoom::AppendPoint(
			FloorPoints,
			ProceduralRoom::ToWorldTransform(LocalTransform, ActorTransform),
			FVector(Settings->NativeModuleSize * 0.5, Settings->NativeModuleSize * 0.5, 5.0));
	}

	for (int32 EdgeIndex = 0; EdgeIndex < Layout.BoundaryEdges.Num(); ++EdgeIndex)
	{
		if (!Config.bCreateExitOpening || EdgeIndex != Layout.ExitEdgeIndex)
		{
		const ProceduralRoom::FBoundaryEdge& Edge = Layout.BoundaryEdges[EdgeIndex];
		FVector LocalPosition = Edge.LocalPosition;
		LocalPosition.Z = Settings->WallZ;
		const double EffectiveWallHeightScale = FMath::Max(
			Settings->WallHeightScale,
			ProceduralRoom::MinimumWallHeightScale);
		const double EffectiveWallThicknessScale = FMath::Max(
			Settings->WallThicknessScale,
			ProceduralRoom::MinimumWallThicknessScale);
		const double EffectiveWallLengthScale =
			ModuleScale * ProceduralRoom::WallLengthOverlapMultiplier;
		const FTransform LocalTransform(
			FRotator(0.0, Edge.WallYaw, 0.0),
			LocalPosition,
			FVector(
				EffectiveWallLengthScale,
				EffectiveWallThicknessScale,
				EffectiveWallHeightScale));
		ProceduralRoom::AppendPoint(
			WallPoints,
			ProceduralRoom::ToWorldTransform(LocalTransform, ActorTransform),
			FVector(
				Settings->NativeModuleSize *
					ProceduralRoom::WallLengthOverlapMultiplier * 0.5,
				25.0,
				Settings->NativeModuleSize * EffectiveWallHeightScale * 0.5));
		}
	}

	UPCGPointData* FloorData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
	UPCGPointData* WallsData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
	FloorData->GetMutablePoints() = MoveTemp(FloorPoints);
	WallsData->GetMutablePoints() = MoveTemp(WallPoints);

	FPCGTaggedData& FloorOutput = Context->OutputData.TaggedData.Emplace_GetRef();
	FloorOutput.Pin = UPCGRandomRoomSettings::FloorPinLabel;
	FloorOutput.Data = FloorData;

	FPCGTaggedData& WallsOutput = Context->OutputData.TaggedData.Emplace_GetRef();
	WallsOutput.Pin = UPCGRandomRoomSettings::WallsPinLabel;
	WallsOutput.Data = WallsData;
	}
	return true;
}

bool UProceduralRoomBlueprintLibrary::PrepareRandomRoom(AActor* RoomActor)
{
	bool bRoomPrepared = IsValid(RoomActor);
	if (bRoomPrepared)
	{
	int32 RoomSeed = FMath::RoundToInt(ProceduralRoom::ReadNumber(RoomActor, TEXT("RoomSeed"), 0.0));
	if (RoomSeed == 0)
	{
		RoomSeed = FMath::RandHelper(MAX_int32 - 1) + 1;
		ProceduralRoom::WriteInteger(RoomActor, TEXT("RoomSeed"), RoomSeed);
	}

	const ProceduralRoom::FRoomConfig Config = ProceduralRoom::ReadConfig(RoomActor);
	const ProceduralRoom::FRoomLayout Layout = ProceduralRoom::GenerateLayout(Config, RoomSeed);
	const TArray<FIntPoint> ChosenEnemyCells =
		ProceduralRoom::ChooseEnemyCells(Layout);
	RoomActor->Tags.AddUnique(ProceduralRoom::ProceduralRoomTag);
	RoomActor->Tags.Remove(ProceduralRoom::ProceduralRoomClearedTag);
	ProceduralRoom::DestroyGeneratedDecorations(RoomActor);

	// Refresh only actors created by this procedural system. Hand-placed
	// decoration has no ProceduralPropTag and is deliberately preserved.
	for (TActorIterator<AActor> ActorIt(RoomActor->GetWorld()); ActorIt; ++ActorIt)
	{
		AActor* ExistingActor = *ActorIt;
		if (IsValid(ExistingActor) &&
			(ExistingActor->ActorHasTag(ProceduralRoom::ProceduralEnemyTag) ||
			 ExistingActor->ActorHasTag(ProceduralRoom::ProceduralPropTag) ||
			 ExistingActor->ActorHasTag(ProceduralRoom::ProceduralDialogueNpcTag)))
		{
			ExistingActor->Destroy();
		}
	}

	ProceduralRoom::WriteInteger(RoomActor, TEXT("GeneratedRoomTilesX"), Layout.TilesX);
	ProceduralRoom::WriteInteger(RoomActor, TEXT("GeneratedRoomTilesY"), Layout.TilesY);
	ProceduralRoom::WriteBool(RoomActor, TEXT("bGeneratedIrregular"), Layout.bIrregular);

	ProceduralRoom::FGeneratedPropResult PropResult =
		ProceduralRoom::SpawnRoomProps(
			RoomActor,
			Config,
			Layout,
			RoomSeed,
			ChosenEnemyCells);

	ProceduralRoom::FRoomAccessibilityResult Accessibility =
		ProceduralRoom::ValidateRoomAccessibility(
			Layout,
			PropResult.BlockingFloorCells);
	if (!Accessibility.bPlayable)
	{
		int32 RemovedBlockers = 0;
		for (TActorIterator<AActor> ActorIt(RoomActor->GetWorld()); ActorIt; ++ActorIt)
		{
			AActor* GeneratedProp = *ActorIt;
			if (IsValid(GeneratedProp) &&
				GeneratedProp->ActorHasTag(ProceduralRoom::ProceduralPropTag))
			{
				TInlineComponentArray<UBoxComponent*> BlockingBoxes(GeneratedProp);
				const bool bBlocksPawn = BlockingBoxes.ContainsByPredicate(
					[](const UBoxComponent* Box)
					{
						return IsValid(Box) &&
							Box->GetCollisionEnabled() != ECollisionEnabled::NoCollision &&
							Box->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block;
					});
				if (bBlocksPawn)
				{
					GeneratedProp->Destroy();
					++RemovedBlockers;
				}
			}
		}

		PropResult.BlockingFloorCells.Reset();
		Accessibility = ProceduralRoom::ValidateRoomAccessibility(
			Layout,
			PropResult.BlockingFloorCells);
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[VALIDACION SALA] Se detecto una sala bloqueada y se reparo retirando %d obstaculos generados. %s"),
			RemovedBlockers,
			*Accessibility.Message);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[VALIDACION SALA] %s"),
			*Accessibility.Message);
	}

	const ProceduralRoom::FGeneratedDecorationCounts DecorationCounts =
		ProceduralRoom::CreateGeneratedDecorations(
			RoomActor,
			Config,
			Layout,
			RoomSeed,
			PropResult.ReservedFloorCells);
	UE_LOG(LogTemp, Display,
			TEXT("Procedural room complete: %d opaque flat ceiling pieces with two-sided shadows joined to walls at Z %.1f, %d previous obstacles, %d previous decorations, %d wall torches, %d illuminated floor fires."),
		DecorationCounts.Ceilings,
		ProceduralRoom::GeneratedWallTopZ,
		PropResult.Obstacles,
		PropResult.Decorations,
		DecorationCounts.Torches,
		DecorationCounts.FloorFires);

	if (UBoxComponent* Bounds = RoomActor->FindComponentByClass<UBoxComponent>())
	{
		Bounds->SetBoxExtent(FVector(
			Layout.TilesX * Config.TileSize * 0.5,
			Layout.TilesY * Config.TileSize * 0.5,
			FMath::Max(
				ProceduralRoom::GeneratedWallTopZ + 200.0,
				Bounds->GetUnscaledBoxExtent().Z)),
			true);
	}

	// GenerateLayout always seeds this cell, so it is a safe centre for the portal.
	const double CentreLocalX = Layout.TilesX % 2 == 0 ? -Config.TileSize * 0.5 : 0.0;
	const double CentreLocalY = Layout.TilesY % 2 == 0 ? -Config.TileSize * 0.5 : 0.0;
	const FTransform LocalExit(
		FRotator::ZeroRotator,
		FVector(CentreLocalX, CentreLocalY, 0.0),
		FVector::OneVector);
	const FTransform WorldExit = ProceduralRoom::ToWorldTransform(LocalExit, RoomActor->GetActorTransform());
	ProceduralRoom::WriteTransform(RoomActor, TEXT("ExitSpawnTransform"), WorldExit);

	// Choose a deterministic free floor cell for the dialogue NPC. Prefer cells
	// without restored props and keep it away from the central portal and enemies.
	const FIntPoint CentreCell(Layout.TilesX / 2, Layout.TilesY / 2);
	TArray<FIntPoint> NpcCandidates;
	for (const FIntPoint& Cell : Layout.Cells)
	{
		const int32 CentreDistance =
			FMath::Abs(Cell.X - CentreCell.X) +
			FMath::Abs(Cell.Y - CentreCell.Y);
		if (CentreDistance >= 2 &&
			!ChosenEnemyCells.Contains(Cell) &&
			!PropResult.ReservedFloorCells.Contains(Cell))
		{
			NpcCandidates.Add(Cell);
		}
	}

	FIntPoint ChosenNpcCell = CentreCell;
	if (!NpcCandidates.IsEmpty())
	{
		FRandomStream NpcRandom(RoomSeed ^ 0x71A10C);
		ChosenNpcCell =
			NpcCandidates[NpcRandom.RandRange(0, NpcCandidates.Num() - 1)];
	}
	else
	{
		// Small, crowded rooms still need a reachable NPC. Use the furthest
		// non-enemy floor cell and let collision handling make the final adjustment.
		int32 FurthestDistance = -1;
		for (const FIntPoint& Cell : Layout.Cells)
		{
			if (!ChosenEnemyCells.Contains(Cell))
			{
				const int32 CentreDistance =
					FMath::Abs(Cell.X - CentreCell.X) +
					FMath::Abs(Cell.Y - CentreCell.Y);
				if (CentreDistance > FurthestDistance)
				{
					FurthestDistance = CentreDistance;
					ChosenNpcCell = Cell;
				}
			}
		}
	}

	const double RoomOriginX =
		-0.5 * static_cast<double>(Layout.TilesX - 1) * Config.TileSize;
	const double RoomOriginY =
		-0.5 * static_cast<double>(Layout.TilesY - 1) * Config.TileSize;
	const FVector NpcLocalPosition(
		RoomOriginX + ChosenNpcCell.X * Config.TileSize,
		RoomOriginY + ChosenNpcCell.Y * Config.TileSize,
		300.0);
	const FVector NpcFacingDirection(
		CentreLocalX - NpcLocalPosition.X,
		CentreLocalY - NpcLocalPosition.Y,
		0.0);
	const FTransform LocalNpcTransform(
		NpcFacingDirection.IsNearlyZero()
			? FRotator::ZeroRotator
			: NpcFacingDirection.Rotation(),
		NpcLocalPosition,
		FVector::OneVector);
	const FTransform NpcWorldTransform = ProceduralRoom::ToWorldTransform(
		LocalNpcTransform,
		RoomActor->GetActorTransform());

	// The neighbouring +X seed cell is also guaranteed. It keeps the player clear
	// of the portal trigger while remaining inside every regular or irregular room.
	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(RoomActor, 0);
	if (IsValid(PlayerCharacter))
	{
		const FVector TargetLocation = RoomActor->GetActorTransform().TransformPosition(
			FVector(CentreLocalX + Config.TileSize, CentreLocalY, 300.0));

		if (UCharacterMovementComponent* Movement = PlayerCharacter->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}

		PlayerCharacter->SetActorLocation(
			TargetLocation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}

	// Spawn exactly two enemies after the room settles. A room can replace one
	// regular enemy with the much slower, high-health Greystone tank variant.
	TArray<FTransform> EnemyWorldTransforms;
	{
		EnemyWorldTransforms.Reserve(2);

		const double OriginX = -0.5 * static_cast<double>(Layout.TilesX - 1) * Config.TileSize;
		const double OriginY = -0.5 * static_cast<double>(Layout.TilesY - 1) * Config.TileSize;

		for (const FIntPoint& Cell : ChosenEnemyCells)
		{
			const FVector LocalPosition(
				OriginX + Cell.X * Config.TileSize,
				OriginY + Cell.Y * Config.TileSize,
				300.0);
			const FTransform LocalEnemyTransform(
				FRotator(0.0, EnemyWorldTransforms.IsEmpty() ? -90.0 : 0.0, 0.0),
				LocalPosition,
				FVector::OneVector);
			EnemyWorldTransforms.Add(ProceduralRoom::ToWorldTransform(
				LocalEnemyTransform,
				RoomActor->GetActorTransform()));
		}
	}

	if (EnemyWorldTransforms.Num() == 2)
	{
		const TWeakObjectPtr<AActor> WeakRoomActor(RoomActor);
		FTimerHandle SpawnEnemiesTimer;
		RoomActor->GetWorld()->GetTimerManager().SetTimer(
			SpawnEnemiesTimer,
			FTimerDelegate::CreateLambda(
				[WeakRoomActor, EnemyWorldTransforms, NpcWorldTransform]()
		{
			UWorld* World = WeakRoomActor.IsValid()
				? WeakRoomActor->GetWorld()
				: nullptr;
			UClass* EnemyClass = LoadClass<AActor>(
				nullptr,
				TEXT("/Game/MyContent/Enemigos/Enemigo_Base/BP_EnemigoBasse.BP_EnemigoBasse_C"));
			UClass* GreystoneTankClass = LoadClass<AActor>(
				nullptr,
				TEXT("/Game/MyContent/Enemigos/Greystone/BP_Enemigo_GreystoneTank.BP_Enemigo_GreystoneTank_C"));
			UClass* CentaurArcherClass = LoadClass<AActor>(
				nullptr,
				TEXT("/Game/MyContent/Enemigos/Arquero/BP_Enemigo_ArqueroCentauro.BP_Enemigo_ArqueroCentauro_C"));
			if (World && EnemyClass)
			{
			// At most one tank per room. Keeping the chance at 30% makes it a
			// noticeable encounter without replacing the regular enemy roster.
			const int32 ArcherSlot = CentaurArcherClass && FMath::FRand() <= 0.40f
				? FMath::RandRange(0, EnemyWorldTransforms.Num() - 1)
				: INDEX_NONE;
			int32 GreystoneSlot = INDEX_NONE;
			if (GreystoneTankClass && FMath::FRand() <= 0.30f)
			{
				GreystoneSlot = FMath::RandRange(0, EnemyWorldTransforms.Num() - 1);
				if (GreystoneSlot == ArcherSlot && EnemyWorldTransforms.Num() > 1)
				{
					GreystoneSlot = (GreystoneSlot + 1) % EnemyWorldTransforms.Num();
				}
			}
			int32 SpawnedEnemies = 0;
			for (int32 EnemyIndex = 0; EnemyIndex < EnemyWorldTransforms.Num(); ++EnemyIndex)
			{
				const FTransform& EnemyTransform = EnemyWorldTransforms[EnemyIndex];
				UClass* ClassToSpawn = EnemyIndex == ArcherSlot
					? CentaurArcherClass
					: (EnemyIndex == GreystoneSlot ? GreystoneTankClass : EnemyClass);
				FActorSpawnParameters SpawnParameters;
				SpawnParameters.SpawnCollisionHandlingOverride =
					ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

				if (AActor* Enemy = World->SpawnActor<AActor>(
					ClassToSpawn,
					EnemyTransform,
					SpawnParameters))
				{
					Enemy->Tags.AddUnique(ProceduralRoom::ProceduralEnemyTag);
					ProceduralRoom::ConfigureEnemySensing(Enemy);
					if (APawn* EnemyPawn = Cast<APawn>(Enemy))
					{
						EnemyPawn->SpawnDefaultController();
					}
					if (EnemyIndex == GreystoneSlot)
					{
						UE_LOG(LogTemp, Display,
							TEXT("[SALA] Greystone tanque generado: velocidad 230, vida 350."));
					}
					else if (EnemyIndex == ArcherSlot)
					{
						UE_LOG(LogTemp, Display,
							TEXT("[SALA] Arquero centauro generado: alcance 2200, dano de flecha 16."));
					}
					++SpawnedEnemies;
				}
			}

			if (SpawnedEnemies == 2)
			{
				ProceduralRoom::MonitorBaseEnemies(
					WeakRoomActor,
					NpcWorldTransform);
			}
			}
		}),
			1.0f,
			false);
	}

	// The PCG graph creates its wall ISM immediately after this preparation
	// function returns. Inspect the final instances shortly afterwards so the
	// seals use the mesh's real, post-PCG endpoints instead of assumptions
	// about its off-centre pivot.
	{
		const TWeakObjectPtr<AActor> WeakRoomActor(RoomActor);
		constexpr float WallSealRetryDelays[] = {0.5f, 1.5f, 3.0f};
		for (const float RetryDelay : WallSealRetryDelays)
		{
			FTimerHandle WallJointSealTimer;
			RoomActor->GetWorld()->GetTimerManager().SetTimer(
				WallJointSealTimer,
				FTimerDelegate::CreateLambda([WeakRoomActor]()
				{
					if (WeakRoomActor.IsValid())
					{
						ProceduralRoom::CreateGeneratedWallJointSeals(
							WeakRoomActor.Get());
					}
				}),
				RetryDelay,
				false);
		}
	}
	}
	return bRoomPrepared;
}

bool UProceduralRoomBlueprintLibrary::ValidateGeneratedRoom(
	AActor* RoomActor,
	int32& ReachableTiles,
	int32& WalkableTiles,
	FString& ValidationMessage)
{
	ReachableTiles = 0;
	WalkableTiles = 0;
	ValidationMessage = TEXT("No se ha proporcionado una sala valida.");
	bool bRoomPlayable = false;
	UWorld* World = IsValid(RoomActor) ? RoomActor->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[VALIDACION SALA] %s"),
			*ValidationMessage);
	}
	else
	{
	int32 RoomSeed = FMath::RoundToInt(
		ProceduralRoom::ReadNumber(RoomActor, TEXT("RoomSeed"), 0.0));
	if (RoomSeed == 0)
	{
		ValidationMessage =
			TEXT("La sala aun no tiene una semilla de generacion.");
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[VALIDACION SALA] %s"),
			*ValidationMessage);
	}
	else
	{
	const ProceduralRoom::FRoomConfig Config =
		ProceduralRoom::ReadConfig(RoomActor);
	const ProceduralRoom::FRoomLayout Layout =
		ProceduralRoom::GenerateLayout(Config, RoomSeed);
	const double OriginX =
		-0.5 * static_cast<double>(Layout.TilesX - 1) * Config.TileSize;
	const double OriginY =
		-0.5 * static_cast<double>(Layout.TilesY - 1) * Config.TileSize;

	TSet<FIntPoint> BlockingCells;
	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		AActor* GeneratedProp = *ActorIt;
		if (IsValid(GeneratedProp) &&
			GeneratedProp->ActorHasTag(ProceduralRoom::ProceduralPropTag))
		{
			TInlineComponentArray<UBoxComponent*> BlockingBoxes(GeneratedProp);
			const bool bBlocksPawn = BlockingBoxes.ContainsByPredicate(
				[](const UBoxComponent* Box)
				{
					return IsValid(Box) &&
						Box->GetCollisionEnabled() != ECollisionEnabled::NoCollision &&
						Box->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block;
				});
			if (bBlocksPawn)
			{
				const FVector LocalPosition =
					RoomActor->GetActorTransform().InverseTransformPosition(
						GeneratedProp->GetActorLocation());
				const FIntPoint Cell(
					FMath::RoundToInt((LocalPosition.X - OriginX) / Config.TileSize),
					FMath::RoundToInt((LocalPosition.Y - OriginY) / Config.TileSize));
				if (Layout.CellSet.Contains(Cell))
				{
					BlockingCells.Add(Cell);
				}
			}
		}
	}

	const ProceduralRoom::FRoomAccessibilityResult Result =
		ProceduralRoom::ValidateRoomAccessibility(Layout, BlockingCells);
	ReachableTiles = Result.ReachableTiles;
	WalkableTiles = Result.WalkableTiles;
	ValidationMessage = Result.Message;
	if (Result.bPlayable)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[VALIDACION SALA] Verificacion solicitada: %s"),
			*ValidationMessage);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[VALIDACION SALA] Verificacion solicitada: %s"),
			*ValidationMessage);
	}
	bRoomPlayable = Result.bPlayable;
	}
	}
	return bRoomPlayable;
}

#undef LOCTEXT_NAMESPACE
