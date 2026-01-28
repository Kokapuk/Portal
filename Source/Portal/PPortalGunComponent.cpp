#include "PPortalGunComponent.h"

#include "PCharacter.h"
#include "PPortal.h"
#include "Types.h"

#include "Camera/CameraComponent.h"

#include "Engine/OverlapResult.h"

#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

#include "Sound/SoundCue.h"

UPPortalGunComponent::UPPortalGunComponent()
{
	SetIsReplicatedByDefault(true);
	SetIsReplicated(true);

	Colors = {FLinearColor::Red, FLinearColor::Blue};
	GridSnapFactor = 10.f;
	PlacementCorrectionIterations = 10;
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

	FirstPersonMeshComponent->SetCastShadow(false);

	ThirdPersonMeshComponent->SetOwnerNoSee(true);

	FirstPersonMeshComponent->AttachToComponent(
		ArmsMesh, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true),
		"GripPoint");
	ThirdPersonMeshComponent->AttachToComponent(ThirdPersonMesh,
	                                            FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true),
	                                            "GripPoint");

	OwnerCharacter->UpdateFirstPersonCapture({FirstPersonMeshComponent});
}

void UPPortalGunComponent::CosmeticFire(int32 PortalID)
{
	check(IsValid(FirstPersonFireMontage))

	const APCharacter* OwnerCharacter = GetOwner<APCharacter>();
	check(IsValid(OwnerCharacter));

	const USkeletalMeshComponent* ArmsMesh = OwnerCharacter->GetArmsMesh();
	check(IsValid(ArmsMesh));

	ArmsMesh->GetAnimInstance()->Montage_Play(FirstPersonFireMontage);

	const FHitResult HitResult = LineTrace();
	ServerFire(HitResult, PortalID);

	PlayShotEffects(PortalID);
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
	                                      UEngineTypes::ConvertToTraceType(ECC_PORTAL),
	                                      false,
	                                      {OwningCharacter}, EDrawDebugTrace::ForDuration, HitResult,
	                                      true,
	                                      FLinearColor::Red, FLinearColor::Green, 3.f);

	return HitResult;
}

void UPPortalGunComponent::AuthSpawnOrUpdatePortal(const FHitResult& HitResult, int32 PortalID)
{
	if (!GetOwner()->HasAuthority()) return;

	AActor* Surface = HitResult.GetActor();
	if (!IsValid(Surface)) return;

	const FTransform SurfaceWorldOrigin(HitResult.ImpactNormal.Rotation(), FVector::ZeroVector);
	const FVector RelativeToSurface = SurfaceWorldOrigin.InverseTransformPosition(HitResult.Location);
	const FVector GriSnapped(
		RelativeToSurface.X,
		FMath::GridSnap(RelativeToSurface.Y, GridSnapFactor),
		FMath::GridSnap(RelativeToSurface.Z, GridSnapFactor)
	);
	const FVector PortalLocation = SurfaceWorldOrigin.TransformPosition(GriSnapped) + (HitResult.ImpactNormal *
		0.1f);

	FRotator PortalRotation = HitResult.ImpactNormal.Rotation();

	if (FMath::Abs(HitResult.ImpactNormal.Z) == 1.f)
	{
		PortalRotation.Yaw = (HitResult.TraceStart - HitResult.ImpactPoint).Rotation().Yaw;
	}

	FTransform PortalTransform(PortalRotation, PortalLocation);
	if (!CorrectPortalLocation(PortalTransform, Surface)) return;

	APPortal* Portal = Portals[PortalID];
	if (!IsValid(Portal))
	{
		APPortal* LinkedPortal = Portals[PortalID == 0 ? 1 : 0];

		Portal = GetWorld()->SpawnActorDeferred<APPortal>(PortalClass, PortalTransform, GetOwner(), GetOwner<APawn>());
		Portal->MultiInitialize(Colors[PortalID]);
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

bool UPPortalGunComponent::CorrectPortalLocation(FTransform& Transform, const AActor* Surface)
{
	const float HalfHeight = 215.f / 2.f; // TODO replace with real values
	const float HalfWidth = 115.f / 2.f; // TODO replace with real values
	FVector BoxExtent = FVector::OneVector;
	FCollisionShape BoxShape = FCollisionShape::MakeBox(BoxExtent);

	for (int32 i = 0; i < PlacementCorrectionIterations; ++i)
	{
		FVector Location = Transform.GetLocation();
		FQuat Rotation = Transform.GetRotation();
		FVector RightVector = Rotation.GetRightVector();
		FVector UpVector = Rotation.GetUpVector();

		FVector VerticalOffset = UpVector * (HalfHeight - GridSnapFactor);
		FVector HorizontalOffset = RightVector * (HalfWidth - GridSnapFactor);

		TArray<FOverlapResult> TopOverlaps;
		FVector TopLocation = Location + VerticalOffset;
		GetWorld()->OverlapMultiByChannel(TopOverlaps, TopLocation, Rotation, ECC_PORTAL, BoxShape);

		TArray<FOverlapResult> TopRightOverlaps;
		FVector TopRightLocation = Location + VerticalOffset - HorizontalOffset;
		GetWorld()->OverlapMultiByChannel(TopRightOverlaps, TopRightLocation, Rotation, ECC_PORTAL,
		                                  BoxShape);

		TArray<FOverlapResult> RightOverlaps;
		FVector RightLocation = Location - HorizontalOffset;
		GetWorld()->OverlapMultiByChannel(RightOverlaps, RightLocation, Rotation, ECC_PORTAL, BoxShape);

		TArray<FOverlapResult> BottomRightOverlaps;
		FVector BottomRightLocation = Location - VerticalOffset - HorizontalOffset;
		GetWorld()->OverlapMultiByChannel(BottomRightOverlaps, BottomRightLocation, Rotation, ECC_PORTAL,
		                                  BoxShape);

		TArray<FOverlapResult> BottomOverlaps;
		FVector BottomLocation = Location - VerticalOffset;
		GetWorld()->OverlapMultiByChannel(BottomOverlaps, BottomLocation, Rotation, ECC_PORTAL, BoxShape);

		TArray<FOverlapResult> BottomLeftOverlaps;
		FVector BottomLeftLocation = Location - VerticalOffset + HorizontalOffset;
		GetWorld()->OverlapMultiByChannel(BottomLeftOverlaps, BottomLeftLocation, Rotation, ECC_PORTAL,
		                                  BoxShape);

		TArray<FOverlapResult> LeftOverlaps;
		FVector LeftLocation = Location + HorizontalOffset;
		GetWorld()->OverlapMultiByChannel(LeftOverlaps, LeftLocation, Rotation, ECC_PORTAL, BoxShape);

		TArray<FOverlapResult> TopLeftOverlaps;
		FVector TopLeftLocation = Location + VerticalOffset + HorizontalOffset;
		GetWorld()->OverlapMultiByChannel(TopLeftOverlaps, TopLeftLocation, Rotation, ECC_PORTAL, BoxShape);

		DrawDebugBox(GetWorld(), TopLocation, BoxExtent, Rotation, FColor::Red, false, 3.f);
		DrawDebugBox(GetWorld(), TopRightLocation, BoxExtent, Rotation, FColor::Red, false, 3.f);
		DrawDebugBox(GetWorld(), RightLocation, BoxExtent, Rotation, FColor::Red, false, 3.f);
		DrawDebugBox(GetWorld(), BottomRightLocation, BoxExtent, Rotation, FColor::Red, false, 3.f);
		DrawDebugBox(GetWorld(), BottomLocation, BoxExtent, Rotation, FColor::Red, false, 3.f);
		DrawDebugBox(GetWorld(), BottomLeftLocation, BoxExtent, Rotation, FColor::Red, false, 3.f);
		DrawDebugBox(GetWorld(), LeftLocation, BoxExtent, Rotation, FColor::Red, false, 3.f);
		DrawDebugBox(GetWorld(), TopLeftLocation, BoxExtent, Rotation, FColor::Red, false, 3.f);

		bool ShouldNudgeTop = BottomOverlaps.Num() != 1 || BottomOverlaps[0].GetActor() != Surface;
		bool ShouldNudgeTopRight = BottomLeftOverlaps.Num() != 1 || BottomLeftOverlaps[0].GetActor() != Surface;
		bool ShouldNudgeRight = LeftOverlaps.Num() != 1 || LeftOverlaps[0].GetActor() != Surface;
		bool ShouldNudgeBottomRight = TopLeftOverlaps.Num() != 1 || TopLeftOverlaps[0].GetActor() != Surface;
		bool ShouldNudgeBottom = TopOverlaps.Num() != 1 || TopOverlaps[0].GetActor() != Surface;
		bool ShouldNudgeBottomLeft = TopRightOverlaps.Num() != 1 || TopRightOverlaps[0].GetActor() != Surface;
		bool ShouldNudgeLeft = RightOverlaps.Num() != 1 || RightOverlaps[0].GetActor() != Surface;
		bool ShouldNudgeTopLeft = BottomRightOverlaps.Num() != 1 || BottomRightOverlaps[0].GetActor() != Surface;

		int32 NudgeTopPriority = (ShouldNudgeTop ? 1 : 0) + (ShouldNudgeTopRight || ShouldNudgeTopLeft ? 1 : 0);
		int32 NudgeRightPriority = (ShouldNudgeRight ? 1 : 0) + (ShouldNudgeTopRight || ShouldNudgeBottomRight ? 1 : 0);
		int32 NudgeBottomPriority = (ShouldNudgeBottom ? 1 : 0) + (ShouldNudgeBottomRight || ShouldNudgeBottomLeft
			                                                           ? 1
			                                                           : 0);
		int32 NudgeLeftPriority = (ShouldNudgeLeft ? 1 : 0) + (ShouldNudgeTopLeft || ShouldNudgeBottomLeft ? 1 : 0);
		int32 MaxNudgePriority = FMath::Max(NudgeTopPriority, NudgeRightPriority, NudgeBottomPriority,
		                                    NudgeLeftPriority);

		if (MaxNudgePriority == 0) return true;

		if (MaxNudgePriority == NudgeTopPriority)
			Transform.SetLocation(
				Transform.GetLocation() + UpVector * GridSnapFactor);
		if (MaxNudgePriority == NudgeRightPriority)
			Transform.SetLocation(
				Transform.GetLocation() - RightVector * GridSnapFactor);
		if (MaxNudgePriority == NudgeBottomPriority)
			Transform.SetLocation(
				Transform.GetLocation() - UpVector * GridSnapFactor);
		if (MaxNudgePriority == NudgeLeftPriority)
			Transform.SetLocation(
				Transform.GetLocation() + RightVector * GridSnapFactor);
	}

	return false;
}

void UPPortalGunComponent::PlayShotEffects(int32 PortalID)
{
	check(ShotSounds.Num() == 2)

	USoundCue* ShotSound = ShotSounds[PortalID];

	UGameplayStatics::PlaySoundAtLocation(this, ShotSound, GetOwner()->GetActorLocation());
	UGameplayStatics::SpawnSoundAttached(ShotSound, GetOwner()->GetRootComponent());
}


void UPPortalGunComponent::ServerFire_Implementation(const FHitResult& HitResult, int32 PortalID)
{
	check(IsValid(PortalClass));
	check(Colors.Num() == 2)

	AuthSpawnOrUpdatePortal(HitResult, PortalID);
	MultiFire(PortalID);
}

void UPPortalGunComponent::MultiFire_Implementation(int32 PortalID)
{
	if (!Cast<APawn>(GetOwner())->IsLocallyControlled())
	{
		PlayShotEffects(PortalID);
	}
}
