#include "PPortal.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Net/UnrealNetwork.h"

APPortal::APPortal()
{
	PrimaryActorTick.bCanEverTick = true;

	PostPostUpdateWorkTick.bCanEverTick = true;
	PostPostUpdateWorkTick.TickGroup = TG_PostUpdateWork;
	PostPostUpdateWorkTick.Target = this;

	SetReplicates(true);
	SetReplicateMovement(true);

	TeleportationThreshold = 0.002f; // NearClipPlane + 0.001 as safe buffer zone

	RootComponent = Mesh = CreateDefaultSubobject<UStaticMeshComponent>("Mesh");
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SceneCaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>("USceneCaptureComponent");
	SceneCaptureComponent->SetupAttachment(RootComponent);
	SceneCaptureComponent->FOVAngle = 103.f;
	SceneCaptureComponent->bEnableClipPlane = true;
	SceneCaptureComponent->bAlwaysPersistRenderingState = true;
	SceneCaptureComponent->bCaptureEveryFrame = false;
	SceneCaptureComponent->bCaptureOnMovement = false;

	Entrance = CreateDefaultSubobject<UArrowComponent>("Entrance");
	Entrance->SetupAttachment(RootComponent);
	Entrance->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	Entrance->SetHiddenInSceneCapture(true);

	Trigger = CreateDefaultSubobject<UBoxComponent>("Trigger");
	Trigger->SetupAttachment(RootComponent);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
}

void APPortal::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APPortal, Surface);
	DOREPLIFETIME(APPortal, LinkedPortal);
}


void APPortal::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		PostPostUpdateWorkTick.RegisterTickFunction(GetLevel());
	}

	Trigger->OnComponentBeginOverlap.AddDynamic(this, &APPortal::OnTriggerBeginOverlap);
	Trigger->OnComponentEndOverlap.AddDynamic(this, &APPortal::OnTriggerEndOverlap);
}

void APPortal::SetupDynamicTarget()
{
	if (GetWorld()->GetNetMode() == NM_DedicatedServer) return;

	RenderTarget = NewObject<UTextureRenderTarget2D>(this);

	FVector2D ViewportSize;
	GEngine->GameViewport->GetViewportSize(ViewportSize);

	RenderTarget->InitCustomFormat(ViewportSize.X, ViewportSize.Y, PF_FloatRGBA, true);
	RenderTarget->ClearColor = FLinearColor::Black;
}

void APPortal::UpdateMaterial() const
{
	if (GetWorld()->GetNetMode() == NM_DedicatedServer) return;

	check(IsValid(RenderTarget));

	if (IsValid(LinkedPortal))
	{
		UMaterialInstanceDynamic* DynMaterial = Mesh->CreateDynamicMaterialInstance(0);

		DynMaterial->SetTextureParameterValue("PortalTexture", RenderTarget);
		Mesh->SetMaterial(0, DynMaterial);
	}
	else Mesh->SetMaterial(0, EmptyMaterial);
}

void APPortal::UpdateCamera()
{
	const APlayerController* LocalController = GetWorld()->GetFirstPlayerController();
	if (!IsValid(LinkedPortal) || !IsValid(LocalController)) return;

	APlayerCameraManager* CameraManager = LocalController->PlayerCameraManager;
	UArrowComponent* LinkedPortalEntrance = LinkedPortal->GetEntrance();
	check((IsValid(CameraManager)))
	check((IsValid(LinkedPortalEntrance)));

	FVector CameraRelativePos = LinkedPortalEntrance->GetComponentTransform().
	                                                  InverseTransformPosition(CameraManager->GetCameraLocation());
	FRotator CameraRelativeRotation = LinkedPortalEntrance->GetComponentTransform().InverseTransformRotation(
		CameraManager->GetCameraRotation().Quaternion()).Rotator();

	SceneCaptureComponent->
		SetRelativeLocation(FVector(CameraRelativePos.X, -CameraRelativePos.Y, -CameraRelativePos.Z));
	SceneCaptureComponent->SetRelativeLocation(CameraRelativePos);
	SceneCaptureComponent->SetRelativeRotation(CameraRelativeRotation);

	FVector PlaneLocation = GetActorLocation();
	FVector PlaneNormal = GetActorForwardVector();

	SceneCaptureComponent->ClipPlaneNormal = PlaneNormal + (FVector::ForwardVector * PlaneNormal);
	SceneCaptureComponent->ClipPlaneBase = PlaneLocation;
}

void APPortal::CheckTransition()
{
	if (!IsValid(LinkedPortal)) return;

	TArray<AActor*> ActorsInTrigger;
	Trigger->GetOverlappingActors(ActorsInTrigger);

	for (AActor* Actor : ActorsInTrigger)
	{
		APawn* Pawn = Cast<APawn>(Actor);

		if (!HasAuthority() && !Pawn->IsLocallyControlled()) continue;

		FVector RelativePosition = GetTransform().InverseTransformPosition(Pawn->GetActorLocation());

		if (RelativePosition.X < TeleportationThreshold)
		{
			FVector NewLocation(TeleportationThreshold + 0.001f, -RelativePosition.Y, RelativePosition.Z);
			// + 0.001 to eliminate poor floating point precision issues on check
			NewLocation = LinkedPortal->GetTransform().TransformPosition(NewLocation);

			FRotator RelativeRotation = Entrance->GetComponentTransform().InverseTransformRotation(
				Pawn->GetActorRotation().Quaternion()).Rotator();
			FRotator CurrentRotation = GetWorld()->GetFirstPlayerController()->GetControlRotation();
			FRotator NewRotation = FRotator(CurrentRotation.Pitch,
			                                LinkedPortal->GetActorRotation().Yaw + RelativeRotation.Yaw,
			                                CurrentRotation.Roll);

			FVector RelativeVelocity = Entrance->GetComponentTransform().InverseTransformVector(Pawn->GetVelocity());
			FVector NewVelocity = LinkedPortal->GetTransform().TransformVector(RelativeVelocity);

			if (Pawn->IsLocallyControlled())
			{
				Pawn->GetController()->SetControlRotation(NewRotation);
			}

			Pawn->SetActorLocation(NewLocation);
			Pawn->GetMovementComponent()->Velocity = NewVelocity;
		}
	}
}

void APPortal::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                     UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                                     const FHitResult& SweepResult)
{
	check(IsValid(Surface));
	Surface->SetActorEnableCollision(false);
}

void APPortal::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                   UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	check(IsValid(Surface));
	Surface->SetActorEnableCollision(true);
}

void APPortal::OnRep_Surface()
{
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void APPortal::OnRep_LinkedPortal()
{
	if (IsValid(LinkedPortal))
	{
		USceneCaptureComponent2D* LinkedPortalSceneCaptureComponent = LinkedPortal->GetSceneCaptureComponent();
		check(IsValid(LinkedPortalSceneCaptureComponent));

		LinkedPortalSceneCaptureComponent->TextureTarget = RenderTarget;
	}

	UpdateMaterial();
}

void APPortal::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	CheckTransition();
}

void APPortal::MultiInitialize_Implementation(UMaterial* DefaultEmptyMaterial)
{
	SetupDynamicTarget();
	EmptyMaterial = DefaultEmptyMaterial;
	UpdateMaterial();
}

void FPostUpdateWorkTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread,
                                              const FGraphEventRef& MyCompletionGraphEvent)
{
	Target->UpdateCamera();
	Target->GetSceneCaptureComponent()->CaptureScene();
}

void APPortal::AuthSetSurface(AActor* NewSurface)
{
	if (!HasAuthority()) return;

	Surface = NewSurface;
	OnRep_Surface();
}

void APPortal::AuthSetLinkedPortal(APPortal* NewLinkedPortal)
{
	if (!HasAuthority()) return;

	LinkedPortal = NewLinkedPortal;
	OnRep_LinkedPortal();
}
