#include "EnemyArcherProjectile.h"

#include "TFGCharacter.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogEnemyArcherProjectile, Log, All);

AEnemyArcherProjectile::AEnemyArcherProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	InitialLifeSpan = 8.0f;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("ArrowCollision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(14.0f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->BodyInstance.bUseCCD = true;
	Collision->OnComponentHit.AddDynamic(this, &AEnemyArcherProjectile::HandleImpact);

	ArrowVisual = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ArrowVisual"));
	ArrowVisual->SetupAttachment(Collision);
	ArrowVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArrowVisual->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = Collision;
	ProjectileMovement->InitialSpeed = 2400.0f;
	ProjectileMovement->MaxSpeed = 2400.0f;
	ProjectileMovement->ProjectileGravityScale = 0.08f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bInitialVelocityInLocalSpace = true;
	ProjectileMovement->Velocity = FVector(2400.0f, 0.0f, 0.0f);
}

void AEnemyArcherProjectile::BeginPlay()
{
	Super::BeginPlay();
	OriginalShooter = GetOwner();
	if (!OriginalShooter.IsValid())
	{
		OriginalShooter = GetInstigator();
	}
	if (OriginalShooter.IsValid())
	{
		Collision->IgnoreActorWhenMoving(OriginalShooter.Get(), true);
	}

	if (USkeletalMesh* ArrowMesh = LoadObject<USkeletalMesh>(
		nullptr,
		TEXT("/Game/QuadrapedCreatures/Centaur/Meshes/SK_Arrow_Action.SK_Arrow_Action")))
	{
		ArrowVisual->SetSkeletalMeshAsset(ArrowMesh);
	}
}

void AEnemyArcherProjectile::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Deflection changes Owner to the player. Re-enable collision against the
	// original shooter so a returned arrow can damage the archer who fired it.
	if (OriginalShooter.IsValid() && GetOwner() != OriginalShooter.Get())
	{
		Collision->IgnoreActorWhenMoving(OriginalShooter.Get(), false);
		OriginalShooter.Reset();
	}
}

void AEnemyArcherProjectile::HandleImpact(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (!IsValid(OtherActor) || OtherActor == this || OtherActor == GetOwner() ||
		OtherActor == GetInstigator())
	{
		return;
	}

	bool bHandled = false;
	if (ATFGCharacter* Player = Cast<ATFGCharacter>(
		UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		bHandled = Player->ProcesarImpactoProyectil(this, OtherActor, Damage);
	}

	UE_LOG(LogEnemyArcherProjectile, Display,
		TEXT("[ARQUERO] Flecha de %s impacto a %s (%s)."),
		*GetNameSafe(GetOwner()), *GetNameSafe(OtherActor),
		bHandled ? TEXT("dano procesado") : TEXT("impacto de entorno"));
	Destroy();
}
