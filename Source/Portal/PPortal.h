#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PPortal.generated.h"

class APPortal;
class UBoxComponent;
class UArrowComponent;

struct FPostUpdateWorkTickFunction : FTickFunction
{
	APPortal* Target;
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread,
	                         const FGraphEventRef& MyCompletionGraphEvent) override;
};

UCLASS()
class PORTAL_API APPortal : public AActor
{
	GENERATED_BODY()

public:
	APPortal();
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(NetMulticast, Reliable)
	void MultiInitialize(UMaterial* DefaultEmptyMaterial);

	void AuthSetLinkedPortal(APPortal* NewLinkedPortal);
	void AuthSetSurface(AActor* NewSurface);

	USceneCaptureComponent2D* GetSceneCaptureComponent() const { return SceneCaptureComponent; }
	UArrowComponent* GetEntrance() const { return Entrance; }

	UFUNCTION(BlueprintPure)
	AActor* GetSurface() const { return Surface; }

	void UpdateCamera();

protected:
	FPostUpdateWorkTickFunction PostPostUpdateWorkTick;
	
	UPROPERTY(EditDefaultsOnly, meta=(ClampMin=0.f, ClampMax=1.f))
	float TeleportationThreshold;

	UPROPERTY(ReplicatedUsing=OnRep_Surface)
	AActor* Surface;

	UPROPERTY(ReplicatedUsing=OnRep_LinkedPortal)
	APPortal* LinkedPortal;

	UPROPERTY()
	UMaterial* EmptyMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UStaticMeshComponent* Mesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USceneCaptureComponent2D* SceneCaptureComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UArrowComponent* Entrance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UBoxComponent* Trigger;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UBoxComponent* FrameTop;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UBoxComponent* FrameRight;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UBoxComponent* FrameBottom;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UBoxComponent* FrameLeft;

	virtual void BeginPlay() override;

	void InitializeDynamicTarget();
	void UpdateMaterial() const;
	void CheckTransition();

private:
	UPROPERTY()
	UTextureRenderTarget2D* RenderTarget;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	                           int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	                         int32 OtherBodyIndex);

	UFUNCTION()
	void OnRep_Surface();

	UFUNCTION()
	void OnRep_LinkedPortal();
};
