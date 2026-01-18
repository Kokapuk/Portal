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

	FirstPersonMeshComponent->SetStaticMesh(Mesh);
	ThirdPersonMeshComponent->SetStaticMesh(Mesh);

	FirstPersonMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	ThirdPersonMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);

	FirstPersonMeshComponent->SetOnlyOwnerSee(true);
	ThirdPersonMeshComponent->SetOwnerNoSee(true);

	FirstPersonMeshComponent->AttachToComponent(
		ArmsMesh, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true),
		"GripPoint");
	ThirdPersonMeshComponent->AttachToComponent(ThirdPersonMesh,
	                                            FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true),
	                                            "GripPoint");
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


void UPPortalGunComponent::ServerFire_Implementation(const FHitResult& HitResult, const int32 PortalID)
{
	check(IsValid(PortalClass));
	check(IsValid(EmptyPortalMaterial));

	AActor* HitActor = HitResult.GetActor();

	if (IsValid(HitActor))
	{
		APPortal* Portal = Portals[PortalID];

		if (!IsValid(Portal))
		{
			APPortal* LinkedPortal = Portals[PortalID == 0 ? 1 : 0];

			Portal = GetWorld()->SpawnActor<APPortal>(PortalClass);
			Portal->AuthSetEmptyMaterial(EmptyPortalMaterial);
			Portal->AuthSetLinkedPortal(LinkedPortal);

			Portals[PortalID] = Portal;

			if (IsValid(LinkedPortal)) LinkedPortal->AuthSetLinkedPortal(Portal);
		}

		FTransform WorldGridTransform(HitResult.ImpactNormal.Rotation(), FVector::ZeroVector);
		FVector RelativeToWorldOrigin = WorldGridTransform.InverseTransformPosition(HitResult.Location);
		FVector SnappedLocal(
			RelativeToWorldOrigin.X,
			FMath::GridSnap(RelativeToWorldOrigin.Y, 100.f),
			FMath::GridSnap(RelativeToWorldOrigin.Z, 100.f)
		);
		FVector FinalWorldLocation = WorldGridTransform.TransformPosition(SnappedLocal);
		Portal->SetActorLocation(FinalWorldLocation + (HitResult.ImpactNormal * 0.1f));

		Portal->SetActorRotation(HitResult.ImpactNormal.Rotation());
		Portal->AuthSetSurface(HitActor);
	}

	MultiFire();
}

void UPPortalGunComponent::MultiFire_Implementation()
{
}
