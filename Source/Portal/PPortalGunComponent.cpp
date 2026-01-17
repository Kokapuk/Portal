#include "PPortalGunComponent.h"

#include "PCharacter.h"

UPPortalGunComponent::UPPortalGunComponent()
{
	SetIsReplicatedByDefault(true);
	SetIsReplicated(true);
}

void UPPortalGunComponent::BeginPlay()
{
	Super::BeginPlay();

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

	FirstPersonMeshComponent->SetOnlyOwnerSee(true);
	ThirdPersonMeshComponent->SetOwnerNoSee(true);

	FirstPersonMeshComponent->AttachToComponent(
		ArmsMesh, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true),
		"GripPoint");
	ThirdPersonMeshComponent->AttachToComponent(ThirdPersonMesh,
	                                            FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true),
	                                            "GripPoint");
}
