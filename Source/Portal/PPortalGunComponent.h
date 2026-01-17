#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PPortalGunComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), DisplayName="Portal Gun Component", Blueprintable)
class PORTAL_API UPPortalGunComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPPortalGunComponent();

	virtual void BeginPlay() override;

protected:
	UPROPERTY(EditDefaultsOnly)
	UStaticMesh* Mesh;
};
