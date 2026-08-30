#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PCGSettings.h"

#include "PCGRandomRoomSettings.generated.h"

/**
 * Creates the floor and perimeter-wall points for a connected room footprint.
 * Runtime values are read from the actor that owns the executing PCG component.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class TFG_API UPCGRandomRoomSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	static const FName FloorPinLabel;
	static const FName WallsPinLabel;

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("RandomRoomShape"); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	/** Fallback tile size when the owning Blueprint does not expose RoomTileSize. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Room|Fallbacks", meta = (ClampMin = "25.0", PCG_Overridable))
	double DefaultTileSize = 200.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Room|Fallbacks", meta = (ClampMin = "2", PCG_Overridable))
	int32 DefaultMinRoomTilesX = 5;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Room|Fallbacks", meta = (ClampMin = "2", PCG_Overridable))
	int32 DefaultMinRoomTilesY = 4;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Room|Fallbacks", meta = (ClampMin = "0.0", ClampMax = "1.0", PCG_Overridable))
	double DefaultIrregularRoomChance = 0.70;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Room|Fallbacks", meta = (ClampMin = "0.35", ClampMax = "1.0", PCG_Overridable))
	double DefaultIrregularFillRatio = 0.72;

	/** Native length of the modular floor and wall meshes before point scaling. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Room|Meshes", meta = (ClampMin = "1.0"))
	double NativeModuleSize = 100.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Room|Meshes")
	double FloorZ = 0.02;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Room|Meshes")
	double WallZ = 100.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Room|Meshes", meta = (ClampMin = "0.01"))
	double WallThicknessScale = 0.30;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Room|Meshes", meta = (ClampMin = "0.01"))
	double WallHeightScale = 3.0;

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	virtual bool UseSeed() const override { return false; }
};

class FPCGRandomRoomElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

/** Runtime preparation shared by Blueprint spawning and the PCG graph. */
UCLASS()
class TFG_API UProceduralRoomBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Resolves an automatic seed, updates the room bounds and places ExitSpawnTransform
	 * on a deterministic opening in the generated perimeter.
	 */
	UFUNCTION(BlueprintCallable, Category = "Procedural Room", meta = (DisplayName = "Prepare Random Room"))
	static bool PrepareRandomRoom(AActor* RoomActor);

	/**
	 * Checks the generated floor as a walkability graph. Every free tile must
	 * be reachable from the player's arrival tile without crossing a generated
	 * blocking prop. This can also be called from Blueprint for debug tools.
	 */
	UFUNCTION(BlueprintCallable, Category = "Procedural Room", meta = (DisplayName = "Validate Generated Room"))
	static bool ValidateGeneratedRoom(
		AActor* RoomActor,
		int32& ReachableTiles,
		int32& WalkableTiles,
		FString& ValidationMessage);
};
