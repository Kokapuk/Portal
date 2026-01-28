#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"

#include "PPortalGunComponent.generated.h"

class APPortal;
class USoundCue;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), DisplayName="Portal Gun Component", Blueprintable)
class PORTAL_API UPPortalGunComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPPortalGunComponent();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, DisplayName="Fire")
	void CosmeticFire(int32 PortalID);

protected:
	UPROPERTY(EditDefaultsOnly)
	UStaticMesh* Mesh;

	UPROPERTY(EditDefaultsOnly)
	UAnimMontage* FirstPersonFireMontage;

	UPROPERTY(EditDefaultsOnly)
	TArray<USoundCue*> ShotSounds;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<APPortal> PortalClass;

	UPROPERTY(EditDefaultsOnly)
	TArray<FLinearColor> Colors;

	UPROPERTY(EditDefaultsOnly)
	float GridSnapFactor;
	
	UPROPERTY(EditDefaultsOnly)
	int32 PlacementCorrectionIterations;
	
	FHitResult LineTrace() const;

	UFUNCTION(Server, Unreliable)
	void ServerFire(const FHitResult& HitResult, int32 PortalID);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void AuthSpawnOrUpdatePortal(const FHitResult& HitResult, int32 PortalID);

	bool CorrectPortalLocation(FTransform& Transform, const AActor* Surface);

	UFUNCTION(NetMulticast, Unreliable)
	void MultiFire(int32 PortalID);

	void PlayShotEffects(int32 PortalID);

private:
	UPROPERTY()
	TArray<APPortal*> Portals;
};
