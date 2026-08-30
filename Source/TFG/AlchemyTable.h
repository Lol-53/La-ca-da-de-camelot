#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "AlchemyTable.generated.h"

class UAlchemyCraftingWidget;
class UPointLightComponent;
class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class TFG_API AAlchemyTable : public AActor
{
	GENERATED_BODY()

public:
	AAlchemyTable();
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category="Alquimia")
	void OpenCraftingMenu();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> TableMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> HealthPot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ManaPot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> EnergyPot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> HealthLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> ManaLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> EnergyLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> InteractionSphere;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> InteractionPrompt;

	UPROPERTY(Transient)
	TObjectPtr<UAlchemyCraftingWidget> CraftingWidget;

	bool bPlayerNearby = false;
	bool bMenuOpen = false;

	UFUNCTION()
	void CloseCraftingMenu();

	void SnapToFloor();
};
