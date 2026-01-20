#include "PPortalGunComponent.h"

#include "PCharacter.h"
#include "PPortal.h"
#include "Camera/CameraComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

UPPortalGunComponent::UPPortalGunComponent()
{
	SetIsReplicatedByDefault(true);
	SetIsReplicated(true);
}

void UPPortalGunComponent::BeginPlay()
{
	Super::BeginPlay();

	Portals.SetNum(2);

	APCharacter* OwnerCharacter = GetOwner<APCharacter>();
	check(IsValid(OwnerCharacter));

	USkeletalMeshComponent* ArmsMesh = OwnerCharacter->GetArmsMesh();
	USkeletalMeshComponent* ThirdPersonMesh = OwnerCharacter->GetMesh();
	check(IsValid(ArmsMesh));
	check(IsValid(ThirdPersonMesh));

	UStaticMeshComponent* FirstPersonMeshComponent = NewObject<UStaticMeshComponent>(
		OwnerCharacter, UStaticMeshComponent::StaticClass());
	UStaticMeshComponent* ThirdPersonMeshComponent = NewObject<UStaticMeshComponent>(
		OwnerCharacter, UStaticMeshComponent::StaticClass());

	FirstPersonMeshComponent->RegisterComponent();
	ThirdPersonMeshComponent->RegisterComponent();

	FirstPersonMeshComponent->SetCastShadow(false);
	ThirdPersonMeshComponent->SetCastShadow(false);

	FirstPersonMeshComponent->SetStaticMesh(Mesh);
	ThirdPersonMeshComponent->SetStaticMesh(Mesh);

	FirstPersonMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	ThirdPersonMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);

	ThirdPersonMeshComponent->SetOwnerNoSee(true);

	FirstPersonMeshComponent->AttachToComponent(
		ArmsMesh, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true),
		"GripPoint");
	ThirdPersonMeshComponent->AttachToComponent(ThirdPersonMesh,
	                                            FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true),
	                                            "GripPoint");
	
	OwnerCharacter->UpdateFirstPersonCapture({FirstPersonMeshComponent});
}

void UPPortalGunComponent::CosmeticFire(const int32 PortalID)
{
	check(IsValid(FirstPersonFireMontage))

	const APCharacter* OwnerCharacter = GetOwner<APCharacter>();
	check(IsValid(OwnerCharacter));

	const USkeletalMeshComponent* ArmsMesh = OwnerCharacter->GetArmsMesh();
	check(IsValid(ArmsMesh));

	ArmsMesh->GetAnimInstance()->Montage_Play(FirstPersonFireMontage);

	const FHitResult HitResult = LineTrace();
	ServerFire(HitResult, PortalID);
}

FHitResult UPPortalGunComponent::LineTrace() const
{
	APCharacter* OwningCharacter = GetOwner<APCharacter>();
	check(IsValid(OwningCharacter))

	const UCameraComponent* Camera = OwningCharacter->GetCamera();
	check(IsValid(Camera))

	const FVector End = Camera->GetComponentLocation() + UKismetMathLibrary::GetForwardVector(
		OwningCharacter->GetBaseAimRotation()) * 35000.f;

	FHitResult HitResult;
	UKismetSystemLibrary::LineTraceSingle(this, Camera->GetComponentLocation(), End,
	                                      UEngineTypes::ConvertToTraceType(ECC_GameTraceChannel1),
	                                      false,
	                                      TArray<AActor*>{OwningCharacter}, EDrawDebugTrace::ForDuration, HitResult,
	                                      true,
	                                      FLinearColor::Red, FLinearColor::Green, 3.f);

	return HitResult;
}

void UPPortalGunComponent::AuthSpawnOrUpdatePortal(const FHitResult& HitResult, const int32 PortalID)
{
	if (!GetOwner()->HasAuthority()) return;

	AActor* Surface = HitResult.GetActor();
	if (!IsValid(Surface)) return;

	const FTransform SurfaceWorldOrigin(HitResult.ImpactNormal.Rotation(), FVector::ZeroVector);
	const FVector RelativeToSurface = SurfaceWorldOrigin.InverseTransformPosition(HitResult.Location);
	const FVector GriSnapped(
		RelativeToSurface.X,
		FMath::GridSnap(RelativeToSurface.Y, 100.f),
		FMath::GridSnap(RelativeToSurface.Z, 100.f)
	);
	const FVector PortalLocation = SurfaceWorldOrigin.TransformPosition(GriSnapped) + (HitResult.ImpactNormal *
		1.f);
	const FRotator PortalRotation = HitResult.ImpactNormal.Rotation();
	const FTransform PortalTransform(PortalRotation, PortalLocation);

	APPortal* Portal = Portals[PortalID];

	if (!IsValid(Portal))
	{
		APPortal* LinkedPortal = Portals[PortalID == 0 ? 1 : 0];

		Portal = GetWorld()->SpawnActorDeferred<APPortal>(PortalClass, PortalTransform, GetOwner(), GetOwner<APawn>());
		Portal->MultiInitialize(EmptyPortalMaterial);
		Portal->AuthSetLinkedPortal(LinkedPortal);
		Portal->AuthSetSurface(Surface);

		if (IsValid(LinkedPortal)) LinkedPortal->AuthSetLinkedPortal(Portal);

		Portal->FinishSpawning(PortalTransform);
		Portals[PortalID] = Portal;
	}
	else
	{
		Portal->SetActorTransform(PortalTransform);
		Portal->AuthSetSurface(Surface);
	}
}


void UPPortalGunComponent::ServerFire_Implementation(const FHitResult& HitResult, const int32 PortalID)
{
	check(IsValid(PortalClass));
	check(IsValid(EmptyPortalMaterial));

	AuthSpawnOrUpdatePortal(HitResult, PortalID);
	MultiFire();
}

void UPPortalGunComponent::MultiFire_Implementation()
{
}
