#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "MusicManagerSubsystem.generated.h"

class UAudioComponent;
class USoundBase;
class UWorld;

/**
 * Persistent music director. It selects music from the current map and
 * crossfades between exploration and combat while preserving both playback
 * positions by pausing the inaudible component at the end of each fade.
 */
UCLASS()
class TFG_API UMusicManagerSubsystem final
	: public UGameInstanceSubsystem
	, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

	/** Refreshes the normal-combat timer after an attack, hit or hostile shot. */
	UFUNCTION(BlueprintCallable, Category="Audio|Musica")
	void NotifyCombatActivity(float HoldSeconds = 5.0f);

	/** Mordred owns an explicit state because his intro must remain ambient. */
	UFUNCTION(BlueprintCallable, Category="Audio|Musica")
	void SetMordredCombatActive(bool bActive);

private:
	enum class EMapMusicContext : uint8
	{
		None,
		MainMenu,
		RoundTable,
		Run,
		Mordred
	};

	void LoadMusicAssets();
	void RefreshWorldAndContext();
	EMapMusicContext ResolveContext(const UWorld* World) const;
	void BuildComponentsForContext(EMapMusicContext NewContext);
	void CreateAndStartAmbient(USoundBase* Sound);
	UAudioComponent* CreateMusicComponent(USoundBase* Sound) const;
	void SetCombatMusicDesired(bool bDesired);
	void EnsureDesiredTrackPlaying(bool bWantsCombat);
	float GetTargetVolumeForComponent(const UAudioComponent* Component) const;
	void PauseAfterFade(UAudioComponent* ComponentToPause, bool bExpectedCombatState);
	void StopAndReleaseComponents();
	void MakeLooping(USoundBase* Sound) const;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> MainMenuMusic;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> RoundTableMusic;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> ExplorationMusic;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> NormalCombatMusic;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> MordredCombatMusic;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> AmbientComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> CombatComponent;

	TWeakObjectPtr<UWorld> ActiveWorld;
	EMapMusicContext ActiveContext = EMapMusicContext::None;
	FTimerHandle PauseAfterFadeTimer;
	double CombatActivityUntil = -1.0;
	float FadeDuration = 1.35f;
	float MusicVolume = 0.62f;
	/** Mordred is mastered louder than the other files, so play it at 80%. */
	float MordredVolumeMultiplier = 0.80f;
	bool bCombatMusicActive = false;
	bool bMordredCombatActive = false;
};
