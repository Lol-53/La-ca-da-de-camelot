// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "Systems/RunPowerPersistenceSubsystem.h"
#include "TFGCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;
class UInputAction;
class UActorComponent;
class UPauseMenuComponent;
class UPotionQuickbarWidget;
class ULobbyReturnDialogueWidget;
class URunStatusWidget;
class UTexture2D;
class UAnimSequenceBase;
class UCameraShakeBase;
class USoundBase;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogCombatPowers, Log, All);

/**
 * Player character used by the project.
 *
 * In addition to movement and camera controls, this class owns the temporary
	 * combat powers. The original powers remain Blueprint-callable, while the
	 * number-row and numeric-keypad keys activate the current prototype set.
 */
UCLASS(abstract)
class ATFGCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	/** Escape pause menu and persistent player settings. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interfaz", meta = (AllowPrivateAccess = "true"))
	UPauseMenuComponent* PauseMenuComponent;

	/** Dark Knight sword visual, attached independently so its grip can be corrected. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Apariencia", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* DarkKnightSwordVisual;

protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	UInputAction* MouseLookAction;

public:
	/** Ends a completed run without restoring its temporary powers on map change. */
	UFUNCTION(BlueprintCallable, Category="Poderes|Persistencia")
	void PrepararFinRunPorVictoria();

	/** Abandons the current run, clearing temporary powers before lobby travel. */
	UFUNCTION(BlueprintCallable, Category="Poderes|Persistencia")
	void PrepararSalidaVoluntariaAlLobby();

	/** Constructor */
	ATFGCharacter();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool MovimientoDesactivado = false;

protected:

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Cache runtime values used by powers. */
	virtual void BeginPlay() override;

	/** Polls keys that must remain available even when another input mapping consumes them. */
	virtual void Tick(float DeltaSeconds) override;

	/** Clears active power timers and restores temporarily modified enemies. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Resolves the area impact when the offensive leap reaches the floor. */
	virtual void Landed(const FHitResult& Hit) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Opens or closes the pause screen. Executes while the world is paused. */
	void AlternarMenuPausa();

	/** Opens or closes the read-only run status overlay. Bound to Tab. */
	void AlternarInterfazEstadoRun();
	void CerrarInterfazEstadoRun();
	int32 ResolverNumeroSalaActual(bool bUpdatePersistence) const;
	TArray<FString> ObtenerNombresPoderesTemporales() const;

	/** Shipping fallback for Blueprint Debug Key events, which are editor-only. */
	void EjecutarAtaquePrincipalEmpaquetado();
	void EjecutarInteraccionEmpaquetada();
	void EjecutarEventoEntradaBlueprintEmpaquetado(const TCHAR* PrefijoEvento);

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	/** New 1: toggles directional guard (25% front damage, 200% back damage). */
	UFUNCTION(BlueprintCallable, Category="Poderes|Nuevos|Activacion")
	void AlternarPoderNuevo1GuardiaDireccional();

	/** New 2: suppresses enemy detection and current aggro for three seconds. */
	UFUNCTION(BlueprintCallable, Category="Poderes|Nuevos|Activacion")
	void ActivarPoderNuevo2Sigilo();

	/** New 3: toggles healing for 50% of damage dealt. */
	UFUNCTION(BlueprintCallable, Category="Poderes|Nuevos|Activacion")
	void AlternarPoderNuevo3RoboVida();

	/** New 4: toggles a 20% chance to avoid incoming hits. */
	UFUNCTION(BlueprintCallable, Category="Poderes|Nuevos|Activacion")
	void AlternarPoderNuevo4Esquiva();

	/** New 5: toggles the periodically recharging damage-to-healing conversion. */
	UFUNCTION(BlueprintCallable, Category="Poderes|Nuevos|Activacion")
	void AlternarPoderNuevo5InversionDanyo();

	/** New 6: toggles directional double-tap dashes that consume stamina. */
	UFUNCTION(BlueprintCallable, Category="Poderes|Nuevos|Activacion")
	void AlternarPoderNuevo6Dash();

	/** New 7: leaps forward and damages nearby enemies on landing. */
	UFUNCTION(BlueprintCallable, Category="Poderes|Nuevos|Activacion")
	void ActivarPoderNuevo7SaltoOfensivo();

	/** New 8: toggles faster attack montages and combo recovery. */
	UFUNCTION(BlueprintCallable, Category="Poderes|Nuevos|Activacion")
	void AlternarPoderNuevo8AtaqueRapido();

	/** Grants one persistent room-reward power without toggling an owned power off. */
	UFUNCTION(BlueprintCallable, Category="Poderes|Seleccion")
	bool OtorgarPowerUpDeSala(int32 PowerUpId);

	/** Returns whether the persistent room-reward power has already been obtained. */
	UFUNCTION(BlueprintPure, Category="Poderes|Seleccion")
	bool TienePowerUpDeSala(int32 PowerUpId) const;

	/** Returns whether this reward occupies the single active-power slot. */
	UFUNCTION(BlueprintPure, Category="Poderes|Seleccion")
	bool EsPowerUpActivoDeSala(int32 PowerUpId) const;

	/** Active reward equipped for the current run, or zero when the slot is empty. */
	UFUNCTION(BlueprintPure, Category="Poderes|Seleccion")
	int32 ObtenerPowerUpActivoDeSala() const { return PowerUpActivoDeSala; }

	/** Uses the single active room power equipped for this run. Bound to R. */
	UFUNCTION(BlueprintCallable, Category="Poderes|Activacion")
	void ActivarPowerUpDeSalaEquipado();

	/** Records the actor responsible for the next incoming damage calculation. */
	UFUNCTION(BlueprintCallable, Category="Poderes|Combate")
	void RegistrarFuenteDanyo(AActor* FuenteDanyo);

	/** Used by enemy sensing Blueprints to ignore the player during stealth. */
	UFUNCTION(BlueprintPure, Category="Poderes|Estado")
	bool EsIndetectable() const { return bIndetectable; }

	/** Multiplier consumed by BP_Combate when playing attacks and combo timers. */
	UFUNCTION(BlueprintPure, Category="Poderes|Combate")
	float ObtenerMultiplicadorVelocidadAtaque() const;

	/** Starts sprinting while either Shift key is held. */
	UFUNCTION(BlueprintCallable, Category="Movimiento|Sprint")
	void IniciarSprint();

	/** Restores normal movement speed when Shift is released. */
	UFUNCTION(BlueprintCallable, Category="Movimiento|Sprint")
	void DetenerSprint();

	/** Exposed for tests and UI: current reflected mana value, or zero if absent. */
	UFUNCTION(BlueprintPure, Category="Recursos|Mana")
	float ObtenerManaActual() const;

	/** Exposed for tests and UI: current reflected stamina value, or zero if absent. */
	UFUNCTION(BlueprintPure, Category="Recursos|Stamina")
	float ObtenerStaminaActual() const;

	/** Stores an enemy drop in the run inventory owned by this character. */
	UFUNCTION(BlueprintCallable, Category="Recolectables")
	void GuardarRecolectable(EEnemyCollectibleType Tipo, int32 Cantidad = 1);

	UFUNCTION(BlueprintPure, Category="Recolectables")
	int32 ObtenerCantidadRecolectable(EEnemyCollectibleType Tipo) const;

	UFUNCTION(BlueprintPure, Category="Recolectables")
	int32 ObtenerTotalRecolectables() const;

	/** Uses a health potion with Z and restores a percentage of maximum health. */
	UFUNCTION(BlueprintCallable, Category="Pociones")
	void UsarPocionVida();

	/** Uses an energy potion with X and restores a percentage of maximum stamina. */
	UFUNCTION(BlueprintCallable, Category="Pociones")
	void UsarPocionEnergia();

	/** Uses a mana potion with C and restores a percentage of maximum mana. */
	UFUNCTION(BlueprintCallable, Category="Pociones")
	void UsarPocionMana();

	/** Hides the potion quickbar while a dialogue is active and restores it afterwards. */
	UFUNCTION(BlueprintCallable, Category="Interfaz|Dialogo")
	void EstablecerDialogoActivo(bool bActivo);

	UFUNCTION(BlueprintPure, Category="Interfaz|Dialogo")
	bool EstaDialogoActivo() const { return bDialogoActivo; }

	/** Exposed for tests: whether power 5 is ready to convert the next hit. */
	UFUNCTION(BlueprintPure, Category="Poderes|Estado")
	bool EstaInversionDanyoPreparada() const { return bInversionDanyoPreparada; }

	/** Exposed for tests: whether an offensive landing is currently pending. */
	UFUNCTION(BlueprintPure, Category="Poderes|Estado")
	bool EstaSaltoOfensivoPendiente() const { return bSaltoOfensivoPendiente; }

	// Original powers are intentionally kept callable, but are no longer bound
	// to the numeric keys.

	/** Legacy callable: grants/prepares the passive double-damage power. */
	UFUNCTION(BlueprintCallable, Category="Poderes|Activacion")
	void ActivarPoder1DobleDanyo();

	/** 2: Toggles the burning damage-over-time effect on attacks. */
	UFUNCTION(BlueprintCallable, Category="Poderes|Activacion")
	void AlternarPoder2Quemadura();

	/** 3: Toggles strong knockback on attacks. */
	UFUNCTION(BlueprintCallable, Category="Poderes|Activacion")
	void AlternarPoder3Desplazamiento();

	/** 4: Toggles the chance for attacks to deal critical damage. */
	UFUNCTION(BlueprintCallable, Category="Poderes|Activacion")
	void AlternarPoder4Critico();

	/** 5: Toggles weakening enemies hit for five seconds. */
	UFUNCTION(BlueprintCallable, Category="Poderes|Activacion")
	void AlternarPoder5Debilitar();

	/** 6: Toggles movement at 200% of the normal speed. */
	UFUNCTION(BlueprintCallable, Category="Poderes|Activacion")
	void AlternarPoder6Velocidad();

	/** 7: Grants one non-stackable extra life. */
	UFUNCTION(BlueprintCallable, Category="Poderes|Activacion")
	void ActivarPoder7VidaExtra();

	/** 8: Toggles received-damage reduction. */
	UFUNCTION(BlueprintCallable, Category="Poderes|Activacion")
	void AlternarPoder8Resistencia();

	/** 9: Toggles projectile paralysis and temporary aggro loss. */
	UFUNCTION(BlueprintCallable, Category="Poderes|Activacion")
	void AlternarPoder9Paralisis();

	/**
	 * Deals one hit through the project's RecibirDanyo Blueprint interface and
	 * applies every currently active offensive power.
	 */
	UFUNCTION(BlueprintCallable, Category="Poderes|Combate")
	void AplicarGolpeConPoderes(AActor* Objetivo, float DanyoBase, bool bEsProyectil = false);

	/**
	 * Performs the authoritative melee sweep used by BP_Combate. Unlike the
	 * legacy Blueprint trace, this accepts any actor exposing RecibirDanyo.
	 */
	UFUNCTION(BlueprintCallable, Category="Poderes|Combate")
	void RealizarTrazaAtaqueJugador(bool bGolpeConCuerpo);

	/** Applies defensive powers before BP_BarraVida removes health. */
	UFUNCTION(BlueprintCallable, Category="Poderes|Combate")
	float ProcesarDanyoRecibido(float DanyoBase);

	/**
	 * Routes a projectile overlap to its correct damage receiver. Returns true
	 * when the projectile hit was handled and should be destroyed.
	 */
	UFUNCTION(BlueprintCallable, Category="Poderes|Combate")
	bool ProcesarImpactoProyectil(AActor* Proyectil, AActor* Objetivo, float DanyoBase = 10.0f);

protected:
	/** Played on the animation's melee trace frame. Defaults to Audio/Sounds/Hit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio|Combate")
	TObjectPtr<USoundBase> SonidoAtaque;

	/** Played only when damage was successfully delivered to an enemy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio|Combate")
	TObjectPtr<USoundBase> SonidoImpacto;

	/** Played only after defensive powers leave real incoming damage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio|Combate")
	TObjectPtr<USoundBase> SonidoRecibirDanyo;

	/** Played when the projectile leaves Arturo, alongside the launch pose. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio|Combate")
	TObjectPtr<USoundBase> SonidoLanzamientoProyectil;

	/** Played once when Arturo begins his definitive death animation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio|Muerte")
	TObjectPtr<USoundBase> SonidoDerrotaCaballeresca;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio|Combate", meta=(ClampMin="0.0"))
	float VolumenSonidosCombate = 0.72f;

	/** Short camera impulse played only after an attack has actually damaged an enemy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camara|Impacto")
	TSubclassOf<UCameraShakeBase> CameraShakeGolpeEnemigo;

	/** Melee attacks and the offensive landing use the full impact strength. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camara|Impacto", meta=(ClampMin="0.0"))
	float IntensidadCameraShakeCuerpoACuerpo = 0.18f;

	/** Projectiles are deliberately softer because several can hit in quick succession. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camara|Impacto", meta=(ClampMin="0.0"))
	float IntensidadCameraShakeProyectil = 0.07f;

	/** Prevents traces or simultaneous area hits from stacking an excessive shake. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camara|Impacto", meta=(ClampMin="0.0"))
	float IntervaloMinimoCameraShake = 0.06f;

	/** Dark Knight death animation played before returning to the lobby. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Muerte")
	TObjectPtr<UAnimSequenceBase> AnimacionMuerte;

	/** Used only if the configured animation cannot report a valid duration. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Muerte", meta=(ClampMin="0.25"))
	float DuracionMuerteFallback = 3.0f;

	/** Power tuning values, editable on BP_ThirdPersonCharacter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Ajustes", meta=(ClampMin="0.0"))
	float EnfriamientoDobleDanyo = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Ajustes", meta=(ClampMin="0.0"))
	float DanyoTotalQuemadura = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Ajustes", meta=(ClampMin="1"))
	int32 NumeroTicksQuemadura = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Ajustes", meta=(ClampMin="0.05"))
	float IntervaloQuemadura = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Ajustes", meta=(ClampMin="0.0"))
	float FuerzaDesplazamiento = 2200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Ajustes", meta=(ClampMin="0.0"))
	float FuerzaVerticalDesplazamiento = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Ajustes", meta=(ClampMin="0.0", ClampMax="1.0"))
	float ProbabilidadCritico = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Ajustes", meta=(ClampMin="1.0"))
	float MultiplicadorCritico = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Ajustes", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MultiplicadorDanyoEnemigoDebilitado = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Ajustes", meta=(ClampMin="0.0"))
	float DuracionDebilitado = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Ajustes", meta=(ClampMin="1.0"))
	float MultiplicadorVelocidad = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Ajustes", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MultiplicadorDanyoRecibido = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Ajustes", meta=(ClampMin="0.0"))
	float DuracionParalisis = 1.0f;

	/** New power and movement tuning values. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Nuevos|Ajustes", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MultiplicadorDanyoFrontal = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Nuevos|Ajustes", meta=(ClampMin="1.0"))
	float MultiplicadorDanyoEspalda = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Nuevos|Ajustes", meta=(ClampMin="0.0"))
	float DuracionSigilo = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Nuevos|Ajustes", meta=(ClampMin="0.0", ClampMax="1.0"))
	float PorcentajeRoboVida = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Nuevos|Ajustes", meta=(ClampMin="0.0", ClampMax="1.0"))
	float ProbabilidadEsquiva = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Nuevos|Ajustes", meta=(ClampMin="0.1"))
	float EnfriamientoInversionDanyo = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Nuevos|Ajustes", meta=(ClampMin="0.05"))
	float VentanaDoblePulsacion = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Nuevos|Ajustes", meta=(ClampMin="0.0"))
	float CosteStaminaDash = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Nuevos|Ajustes", meta=(ClampMin="0.0"))
	float FuerzaDash = 3600.0f;

	// Air movement has much less braking than ground movement. Applying only a
	// fraction of the force keeps an airborne dash close to the ground distance.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Nuevos|Ajustes", meta=(ClampMin="0.1", ClampMax="1.0"))
	float MultiplicadorDashAereo = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Nuevos|Ajustes", meta=(ClampMin="0.0"))
	float FuerzaSaltoOfensivoAdelante = 750.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Nuevos|Ajustes", meta=(ClampMin="0.0"))
	float FuerzaSaltoOfensivoVertical = 850.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Nuevos|Ajustes", meta=(ClampMin="0.0"))
	float RadioSaltoOfensivo = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Nuevos|Ajustes", meta=(ClampMin="0.0"))
	float DanyoSaltoOfensivo = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Nuevos|Ajustes", meta=(ClampMin="1.0"))
	float MultiplicadorVelocidadAtaqueNueva = 1.75f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Proyectiles", meta=(ClampMin="100.0"))
	float RadioAutoapuntadoProyectil = 2600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Proyectiles", meta=(ClampMin="0.0"))
	float AceleracionAutoapuntado = 4200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Proyectiles", meta=(ClampMin="100.0"))
	float RadioReboteProyectil = 2200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Proyectiles", meta=(ClampMin="1", ClampMax="20"))
	int32 MaximoRebotesProyectil = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Proyectiles", meta=(ClampMin="20.0"))
	float RadioDesvioProyectil = 105.0f;

	/** Minimum interval between projectiles successfully launched by Arturo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Poderes|Proyectiles", meta=(ClampMin="0.0"))
	float EnfriamientoProyectilArturo = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recursos|Ajustes", meta=(ClampMin="0.0"))
	float ManaRegeneradoPorSegundo = 5.0f;

	/** Percentage of the corresponding maximum resource restored by each potion. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD|Pociones", meta=(ClampMin="0.01", ClampMax="1.0"))
	float PorcentajeRestauradoPocion = 0.35f;

	/** Optional icon textures. Leave empty to use the colored placeholders. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD|Pociones")
	TObjectPtr<UTexture2D> IconoPocionVida;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD|Pociones")
	TObjectPtr<UTexture2D> IconoPocionEnergia;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD|Pociones")
	TObjectPtr<UTexture2D> IconoPocionMana;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movimiento|Sprint", meta=(ClampMin="1.0"))
	float MultiplicadorSprint = 1.6f;

private:

	struct FQuemaduraActiva
	{
		int32 TicksRestantes = 0;
		FTimerHandle Timer;
	};

	struct FDebilitadoActivo
	{
		double DanyoOriginal = 0.0;
		FTimerHandle Timer;
	};

	struct FParalisisActiva
	{
		float VelocidadOriginal = 0.0f;
		bool bTeniaAggro = false;
		FTimerHandle Timer;
	};

	bool bDobleDanyoPreparado = false;
	bool bAtaquesQueman = false;
	bool bAtaquesDesplazan = false;
	bool bAtaquesCriticos = false;
	bool bAtaquesDebilitan = false;
	bool bVelocidadAumentada = false;
	bool bTieneVidaExtra = false;
	bool bDanyoReducido = false;
	bool bProyectilesParalizan = false;

	bool bGuardiaDireccionalActiva = false;
	bool bIndetectable = false;
	bool bRoboVidaActivo = false;
	bool bEsquivaActiva = false;
	bool bInversionDanyoActiva = false;
	bool bInversionDanyoPreparada = false;
	bool bDashHabilitado = false;
	bool bSaltoOfensivoPendiente = false;
	bool bAtaqueRapidoActivo = false;
	bool bProyectilesAutoapuntado = false;
	bool bProyectilesRebotan = false;
	bool bRecursosAlquimiaObtenidos = false;
	bool bAtaquesDesvianProyectiles = false;
	bool bSprintActivo = false;
	bool bVidaExtraPermanenteAsignada = false;
	bool bDialogoActivo = false;
	bool bMuerteEnCurso = false;
	bool bAnimacionMuerteIniciada = false;
	bool bOmitirGuardadoPoderesEnEndPlay = false;
	float AcumuladorVisualProyectiles = 0.0f;
	float UltimaAnimacionLanzamiento = -100.0f;
	float UltimoProyectilArturoAceptado = -100.0f;
	float UltimoSonidoAtaque = -100.0f;
	float UltimoSonidoImpacto = -100.0f;
	int32 PowerUpActivoDeSala = 0;

	bool bAdelantePulsado = false;
	bool bAtrasPulsado = false;
	bool bDerechaPulsada = false;
	bool bIzquierdaPulsada = false;
	float UltimaPulsacionAdelante = -100.0f;
	float UltimaPulsacionAtras = -100.0f;
	float UltimaPulsacionDerecha = -100.0f;
	float UltimaPulsacionIzquierda = -100.0f;
	float UltimoDash = -100.0f;
	float UltimoCameraShakeGolpe = -100.0f;
	float SiguienteActualizacionNumeroSala = 0.0f;

	TWeakObjectPtr<AActor> FuenteDanyoPendiente;

	float VelocidadBase = 0.0f;
	float MultiplicadorDanyoPermanente = 1.0f;
	float MultiplicadorVelocidadPermanente = 1.0f;
	float RegeneracionVidaPermanente = 0.0f;
	float FinEnfriamientoDobleDanyo = 0.0f;
	float AcumuladorActualizacionProyectiles = 0.0f;
	FTimerHandle TimerEnfriamientoDobleDanyo;
	FTimerHandle TimerSigilo;
	FTimerHandle TimerInversionDanyo;
	FTimerHandle TimerRegeneracionMana;
	FTimerHandle TimerOcultarHUDLobby;
	FTimerHandle TimerDialogoPrimerRetorno;
	FTimerHandle TimerFinalizarMuerte;

	UPROPERTY(Transient)
	TObjectPtr<UPotionQuickbarWidget> PotionQuickbarWidget;

	UPROPERTY(Transient)
	TObjectPtr<ULobbyReturnDialogueWidget> LobbyReturnDialogueWidget;

	UPROPERTY(Transient)
	TObjectPtr<URunStatusWidget> RunStatusWidget;

	UPROPERTY(Transient)
	TObjectPtr<UInputComponent> LobbyReturnDialogueInputComponent;

	bool bLobbyReturnDialogueInputBound = false;

	TMap<TWeakObjectPtr<AActor>, FQuemaduraActiva> QuemadurasActivas;
	TMap<TWeakObjectPtr<AActor>, FDebilitadoActivo> EnemigosDebilitados;
	TMap<TWeakObjectPtr<AActor>, FParalisisActiva> EnemigosParalizados;
	TMap<TWeakObjectPtr<AActor>, float> UltimosImpactosAtaque;
	TSet<TWeakObjectPtr<AActor>> ProyectilesDelJugador;
	TSet<TWeakObjectPtr<AActor>> ProyectilesVisualesConfigurados;
	TMap<TWeakObjectPtr<AActor>, TSet<TWeakObjectPtr<AActor>>> ObjetivosPorProyectil;
	TMap<TWeakObjectPtr<AActor>, int32> RebotesPorProyectil;

	void DobleDanyoDisponible();
	void IniciarQuemadura(AActor* Objetivo);
	void TickQuemadura(TWeakObjectPtr<AActor> Objetivo);
	void AplicarDesplazamiento(AActor* Objetivo) const;
	void AplicarDebilitado(AActor* Objetivo);
	void TerminarDebilitado(TWeakObjectPtr<AActor> Objetivo);
	void AplicarParalisis(AActor* Objetivo);
	void TerminarParalisis(TWeakObjectPtr<AActor> Objetivo);
	void TerminarSigilo();
	void PrepararInversionDanyo();
	void RegenerarMana();
	void ProcesarEntradaDash(const FVector2D& MovementVector);
	void RegistrarPulsacionDash(const FVector2D& DireccionLocal, float& UltimaPulsacion);
	void EjecutarDash(const FVector2D& DireccionLocal);
	void ResolverSaltoOfensivo();
	void ActualizarProyectilesConPoderes(float DeltaSeconds);
	void ActualizarVisualesProyectiles(float DeltaSeconds);
	void ConfigurarDestelloProyectil(AActor* Proyectil, const FLinearColor& Color, const TCHAR* Lanzador);
	void ReproducirAnimacionLanzamiento();
	void ReproducirSonidoCombate(USoundBase* Sonido, float Tono, const TCHAR* Evento) const;
	void DesviarProyectilesEnAtaque(const FVector& Inicio, const FVector& Fin);
	void MarcarProyectilDelJugador(AActor* Proyectil);
	void RedirigirProyectil(AActor* Proyectil, AActor* Objetivo, const TCHAR* Motivo);
	bool EsActorProyectil(const AActor* Actor) const;
	bool EsProyectilDelJugador(const AActor* Proyectil) const;
	AActor* EncontrarObjetivoParaProyectil(
		const FVector& Origen,
		const AActor* ObjetivoExcluido,
		const TSet<TWeakObjectPtr<AActor>>* ObjetivosExcluidos,
		float Radio) const;
	void ActualizarVelocidadMovimiento();
	void AplicarMejorasPermanentes();
	void RestaurarVidaPersistente();
	void CrearHUDPociones();
	void OcultarEtiquetaVidaActualHUD();
	void ConfigurarHUDParaMapaActual();
	void MostrarDialogoPrimerRetornoSiPendiente();
	void CerrarDialogoPrimerRetorno();
	void RestaurarControlTrasDialogoPrimerRetorno();
	bool EsLobbyMesaRedonda() const;
	void UsarPocion(EPotionType Tipo);
	void GuardarEstadoPoderesPersistentes(const TCHAR* Motivo) const;
	void RestaurarEstadoPoderesPersistentes();
	void ReproducirCameraShakeGolpe(bool bEsProyectil);

	bool EnviarDanyoBlueprint(AActor* Objetivo, float Danyo) const;
	bool SoportaDanyoBlueprint(const AActor* Objetivo) const;
	UActorComponent* BuscarComponenteVida() const;
	UActorComponent* BuscarComponenteRecurso(FName PropiedadActual, FName PropiedadMaxima) const;
	AActor* BuscarFuenteDanyoCercana() const;
	float CurarPersonaje(float Cantidad, const TCHAR* Motivo);
	bool ConsumirStamina(float Cantidad);
	void ActualizarHUDRecurso(FName Funcion, float Porcentaje) const;
	bool ConsumirVidaExtraSiEsMortal(float DanyoProcesado);
	void IniciarMuerteDefinitiva();
	void FinalizarMuerteYVolverLobby();
	void DebugDarLuz();
	void DebugDarVida();
	void DebugDarEnergia();
	void DebugDarMana();
	void OtorgarRecursoDebug(EEnemyCollectibleType Tipo, const TCHAR* Nombre);

public:

	/** Returns CameraBoom subobject. */
	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject. */
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};
