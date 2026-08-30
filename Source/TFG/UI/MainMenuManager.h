#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MainMenuManager.generated.h"

class UMainMenuWidget;
class UPauseMenuComponent;
class UTexture2D;

/** Place one instance in the dedicated main-menu map. */
UCLASS(Blueprintable)
class TFG_API AMainMenuManager : public AActor
{
	GENERATED_BODY()

public:
	AMainMenuManager();

	UFUNCTION(BlueprintCallable, Category="Menu principal")
	void StartSaveSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category="Menu principal")
	void QuitGame();

	UPauseMenuComponent* GetSettingsComponent() const { return SettingsComponent; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Menu principal|Contenido")
	TSoftObjectPtr<UTexture2D> BackgroundImage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Menu principal|Contenido", meta=(MultiLine=true))
	FText CreditsText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Menu principal|Navegacion")
	FName FirstGameplayMap = TEXT("/Game/MyContent/Maps/Lobby_MesaRedonda");

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void RestoreGameplayInput() const;

	UPROPERTY(VisibleAnywhere, Category="Menu principal")
	TObjectPtr<UPauseMenuComponent> SettingsComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMainMenuWidget> MenuWidget;
};
