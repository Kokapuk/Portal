#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PInputActions.generated.h"

class UInputAction;

UCLASS()
class PORTAL_API UPInputActions : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	UInputAction* LookAction;

	UPROPERTY(EditDefaultsOnly)
	UInputAction* MoveAction;

	UPROPERTY(EditDefaultsOnly)
	UInputAction* JumpAction;

	UPROPERTY(EditDefaultsOnly)
	UInputAction* CrouchAction;
	
	UPROPERTY(EditDefaultsOnly)
	UInputAction* FireAction;
};
