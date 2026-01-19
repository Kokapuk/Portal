#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PPortalGunComponent.generated.h"

class APPortal;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), DisplayName="Portal Gun Component", Blueprintable)
class PORTAL_API UPPortalGunComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPPortalGunComponent();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, DisplayName="Fire")
	void CosmeticFire(const int32 PortalID);

protected:
	UPROPERTY(EditDefaultsOnly)
	UStaticMesh* Mesh;

	UPROPERTY(EditDefaultsOnly)
	UAnimMontage* FirstPersonFireMontage;
	
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<APPortal> PortalClass;
	
	UPROPERTY(EditDefaultsOnly)
	UMaterial* EmptyPortalMaterial;

	FHitResult LineTrace() const;

	UFUNCTION(Server, Unreliable)
	void ServerFire(const FHitResult& HitResult, const int32 PortalID);
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void AuthSpawnOrUpdatePortal(const FHitResult& HitResult, const int32 PortalID);
	
	UFUNCTION(NetMulticast, Unreliable)
	void MultiFire();
	
private:
	UPROPERTY()
	TArray<APPortal*> Portals;
};
