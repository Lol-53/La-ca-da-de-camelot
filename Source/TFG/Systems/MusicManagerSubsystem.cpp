#include "Systems/MusicManagerSubsystem.h"

#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMusicManager, Log, All);

void UMusicManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadMusicAssets();
	UE_LOG(LogMusicManager, Display,
		TEXT("[MUSICA] Director inicializado con menu, lobby, ambiente, combate y Mordred."));
}

void UMusicManagerSubsystem::Deinitialize()
{
	StopAndReleaseComponents();
	Super::Deinitialize();
}

bool UMusicManagerSubsystem::IsTickable() const
{
	return !IsTemplate() && GetGameInstance() != nullptr;
}

TStatId UMusicManagerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMusicManagerSubsystem, STATGROUP_Tickables);
}

void UMusicManagerSubsystem::Tick(const float DeltaTime)
{
	RefreshWorldAndContext();
	UWorld* World = ActiveWorld.Get();
	if (!World)
	{
		return;
	}

	bool bWantsCombat = false;
	if (ActiveContext == EMapMusicContext::Run)
	{
		bWantsCombat = World->GetTimeSeconds() < CombatActivityUntil;
	}
	else if (ActiveContext == EMapMusicContext::Mordred)
	{
		bWantsCombat = bMordredCombatActive;
	}
	SetCombatMusicDesired(bWantsCombat);
	EnsureDesiredTrackPlaying(bWantsCombat);
}

void UMusicManagerSubsystem::NotifyCombatActivity(const float HoldSeconds)
{
	RefreshWorldAndContext();
	if (UWorld* World = ActiveWorld.Get())
	{
		CombatActivityUntil = FMath::Max(
			CombatActivityUntil,
			static_cast<double>(World->GetTimeSeconds() + FMath::Max(1.0f, HoldSeconds)));
		if (ActiveContext == EMapMusicContext::Run)
		{
			SetCombatMusicDesired(true);
		}
	}
}

void UMusicManagerSubsystem::SetMordredCombatActive(const bool bActive)
{
	bMordredCombatActive = bActive;
	RefreshWorldAndContext();
	if (ActiveContext == EMapMusicContext::Mordred)
	{
		SetCombatMusicDesired(bActive);
	}
}

void UMusicManagerSubsystem::LoadMusicAssets()
{
	MainMenuMusic = LoadObject<USoundBase>(nullptr,
		TEXT("/Game/MyContent/Audio/music/17406877-knights-of-camelot-8038_menuPrincipal_.17406877-knights-of-camelot-8038_menuPrincipal_"));
	RoundTableMusic = LoadObject<USoundBase>(nullptr,
		TEXT("/Game/MyContent/Audio/music/vifotofreesounds-ancient-486689_mesaRedonda1_.vifotofreesounds-ancient-486689_mesaRedonda1_"));
	ExplorationMusic = LoadObject<USoundBase>(nullptr,
		TEXT("/Game/MyContent/Audio/music/Steffen_Daum_-_Awakening_salaNoCombate_.Steffen_Daum_-_Awakening_salaNoCombate_"));
	NormalCombatMusic = LoadObject<USoundBase>(nullptr,
		TEXT("/Game/MyContent/Audio/music/Miguel_Johnson_-_Good_Day_To_Die_CombateNormal_.Miguel_Johnson_-_Good_Day_To_Die_CombateNormal_"));
	MordredCombatMusic = LoadObject<USoundBase>(nullptr,
		TEXT("/Game/MyContent/Audio/music/Makai_Symphony_-_The_Army_of_Minotaur_CombateMordred_.Makai_Symphony_-_The_Army_of_Minotaur_CombateMordred_"));

	MakeLooping(MainMenuMusic);
	MakeLooping(RoundTableMusic);
	MakeLooping(ExplorationMusic);
	MakeLooping(NormalCombatMusic);
	MakeLooping(MordredCombatMusic);
}

void UMusicManagerSubsystem::MakeLooping(USoundBase* Sound) const
{
	if (USoundWave* Wave = Cast<USoundWave>(Sound))
	{
		Wave->bLooping = true;
	}
}

void UMusicManagerSubsystem::RefreshWorldAndContext()
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	const EMapMusicContext NewContext = ResolveContext(World);
	if (ActiveWorld.Get() != World || ActiveContext != NewContext)
	{
		StopAndReleaseComponents();
		ActiveWorld = World;
		ActiveContext = NewContext;
		CombatActivityUntil = -1.0;
		bCombatMusicActive = false;
		bMordredCombatActive = false;
		BuildComponentsForContext(NewContext);
		UE_LOG(LogMusicManager, Display,
			TEXT("[MUSICA] Contexto del mapa %s configurado (%d)."),
			*World->GetMapName(), static_cast<int32>(NewContext));
	}
}

UMusicManagerSubsystem::EMapMusicContext
UMusicManagerSubsystem::ResolveContext(const UWorld* World) const
{
	const FString MapName = World ? World->GetMapName() : FString();
	EMapMusicContext Context = EMapMusicContext::Run;
	if (MapName.Contains(TEXT("Menu_Principal"), ESearchCase::IgnoreCase))
	{
		Context = EMapMusicContext::MainMenu;
	}
	else if (MapName.Contains(TEXT("Lobby_MesaRedonda"), ESearchCase::IgnoreCase) ||
		MapName.Contains(TEXT("Lobby_Mesa_Redonda"), ESearchCase::IgnoreCase))
	{
		Context = EMapMusicContext::RoundTable;
	}
	else if (MapName.Contains(TEXT("Mordred"), ESearchCase::IgnoreCase))
	{
		Context = EMapMusicContext::Mordred;
	}
	return Context;
}

void UMusicManagerSubsystem::BuildComponentsForContext(const EMapMusicContext NewContext)
{
	switch (NewContext)
	{
	case EMapMusicContext::MainMenu:
		CreateAndStartAmbient(MainMenuMusic);
		break;
	case EMapMusicContext::RoundTable:
		CreateAndStartAmbient(RoundTableMusic);
		break;
	case EMapMusicContext::Run:
		CreateAndStartAmbient(ExplorationMusic);
		CombatComponent = CreateMusicComponent(NormalCombatMusic);
		break;
	case EMapMusicContext::Mordred:
		CreateAndStartAmbient(ExplorationMusic);
		CombatComponent = CreateMusicComponent(MordredCombatMusic);
		break;
	default:
		break;
	}
}

void UMusicManagerSubsystem::CreateAndStartAmbient(USoundBase* Sound)
{
	AmbientComponent = CreateMusicComponent(Sound);
	if (AmbientComponent)
	{
		AmbientComponent->FadeIn(
			FadeDuration,
			GetTargetVolumeForComponent(AmbientComponent.Get()),
			0.0f);
	}
}

UAudioComponent* UMusicManagerSubsystem::CreateMusicComponent(USoundBase* Sound) const
{
	UWorld* World = ActiveWorld.Get();
	UAudioComponent* Component = nullptr;
	if (!World || !Sound)
	{
		UE_LOG(LogMusicManager, Warning,
			TEXT("[MUSICA] No se pudo crear una pista: mundo o sonido invalido."));
	}
	else
	{
		Component = UGameplayStatics::CreateSound2D(
			World, Sound, 1.0f, 1.0f, 0.0f, nullptr, false, false);
		if (Component)
		{
			Component->bAutoDestroy = false;
			// FadeIn/AdjustVolume use the component's internal fader. Keeping the
			// base multiplier at zero would mute that fader at every value.
			Component->SetVolumeMultiplier(1.0f);
		}
	}
	return Component;
}

void UMusicManagerSubsystem::SetCombatMusicDesired(const bool bDesired)
{
	const bool bCanChangeMusic = bCombatMusicActive != bDesired &&
		CombatComponent && AmbientComponent && ActiveWorld.IsValid();
	if (bCanChangeMusic)
	{
		bCombatMusicActive = bDesired;
		UWorld* World = ActiveWorld.Get();
		World->GetTimerManager().ClearTimer(PauseAfterFadeTimer);

	UAudioComponent* Incoming = bDesired ? CombatComponent.Get() : AmbientComponent.Get();
	UAudioComponent* Outgoing = bDesired ? AmbientComponent.Get() : CombatComponent.Get();
	const float IncomingVolume = GetTargetVolumeForComponent(Incoming);
	if (Incoming->GetPlayState() == EAudioComponentPlayState::Stopped)
	{
		Incoming->FadeIn(FadeDuration, IncomingVolume, 0.0f);
	}
	else
	{
		Incoming->SetPaused(false);
		Incoming->AdjustVolume(FadeDuration, IncomingVolume);
	}
	Outgoing->AdjustVolume(FadeDuration, 0.0f);

	FTimerDelegate PauseDelegate;
	PauseDelegate.BindUObject(
		this,
		&UMusicManagerSubsystem::PauseAfterFade,
		Outgoing,
		bDesired);
	World->GetTimerManager().SetTimer(
		PauseAfterFadeTimer, PauseDelegate, FadeDuration + 0.03f, false);

		UE_LOG(LogMusicManager, Display,
		TEXT("[MUSICA] Transicion %.2f s -> %s; la pista saliente conservara su posicion."),
			FadeDuration, bDesired ? TEXT("COMBATE") : TEXT("AMBIENTE"));
	}
}

void UMusicManagerSubsystem::EnsureDesiredTrackPlaying(const bool bWantsCombat)
{
	UAudioComponent* Desired =
		bWantsCombat && CombatComponent ? CombatComponent.Get() : AmbientComponent.Get();
	if (!Desired)
	{
		return;
	}

	const EAudioComponentPlayState PlayState = Desired->GetPlayState();
	if (PlayState == EAudioComponentPlayState::Stopped)
	{
		// SoundWave looping is enabled too, but this fallback also covers an
		// imported wave that reaches its end or is stopped during level travel.
		Desired->FadeIn(
			FadeDuration,
			GetTargetVolumeForComponent(Desired),
			0.0f);
		UE_LOG(LogMusicManager, Display,
			TEXT("[MUSICA] Pista activa terminada/detenida; ciclo reiniciado automaticamente."));
	}
	else if (PlayState == EAudioComponentPlayState::Paused)
	{
		// Only the outgoing track is allowed to stay paused. If the desired
		// track is paused, a rapid combat/ambient switch interrupted the fade.
		Desired->SetPaused(false);
		Desired->AdjustVolume(
			FadeDuration,
			GetTargetVolumeForComponent(Desired));
		UE_LOG(LogMusicManager, Display,
			TEXT("[MUSICA] Pista deseada reanudada desde su posicion guardada."));
	}
}

float UMusicManagerSubsystem::GetTargetVolumeForComponent(
	const UAudioComponent* Component) const
{
	const bool bEsCombateMordred = ActiveContext == EMapMusicContext::Mordred &&
		Component && Component == CombatComponent.Get();
	const float TargetVolume = bEsCombateMordred
		? MusicVolume * MordredVolumeMultiplier
		: MusicVolume;
	return TargetVolume;
}

void UMusicManagerSubsystem::PauseAfterFade(
	UAudioComponent* ComponentToPause,
	const bool bExpectedCombatState)
{
	if (IsValid(ComponentToPause) && bCombatMusicActive == bExpectedCombatState)
	{
		ComponentToPause->SetPaused(true);
	}
}

void UMusicManagerSubsystem::StopAndReleaseComponents()
{
	if (UWorld* World = ActiveWorld.Get())
	{
		World->GetTimerManager().ClearTimer(PauseAfterFadeTimer);
	}
	if (AmbientComponent)
	{
		AmbientComponent->Stop();
		AmbientComponent->DestroyComponent();
	}
	if (CombatComponent)
	{
		CombatComponent->Stop();
		CombatComponent->DestroyComponent();
	}
	AmbientComponent = nullptr;
	CombatComponent = nullptr;
}
