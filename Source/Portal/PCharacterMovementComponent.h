#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PCharacterMovementComponent.generated.h"

UCLASS()
class PORTAL_API UPCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UPCharacterMovementComponent();

	virtual bool CanAttemptJump() const override;
};
