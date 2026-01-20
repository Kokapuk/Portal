#include "PPortal.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Net/UnrealNetwork.h"

APPortal::APPortal()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	PostPostUpdateWorkTick.bCanEverTick = true;
	PostPostUpdateWorkTick.TickGroup = TG_PostUpdateWork;
	PostPostUpdateWorkTick.Target = this;

	bReplicates = true;
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
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	FrameTop = CreateDefaultSubobject<UBoxComponent>("FrameTop");
	FrameRight = CreateDefaultSubobject<UBoxComponent>("FrameRight");
	FrameBottom = CreateDefaultSubobject<UBoxComponent>("FrameBottom");
	FrameLeft = CreateDefaultSubobject<UBoxComponent>("FrameLeft");

	for (TArray Frames = {FrameTop, FrameRight, FrameBottom, FrameLeft}; UBoxComponent* Frame : Frames)
	{
		Frame->SetupAttachment(RootComponent);
		Frame->SetCollisionProfileName("BlockAllDynamic");
		Frame->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
	}
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

void APPortal::InitializeDynamicTarget()
{
	if (GetWorld()->GetNetMode() == NM_DedicatedServer) return;

	FVector2D ViewportSize;
	GEngine->GameViewport->GetViewportSize(ViewportSize);
	RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
		this, ViewportSize.X, ViewportSize.Y, RTF_RGBA16f);
}

void APPortal::UpdateMaterial() const
{
	if (GetWorld()->GetNetMode() == NM_DedicatedServer) return;

	check(IsValid(RenderTarget));

	if (IsValid(LinkedPortal))
	{
		UMaterialInstanceDynamic* DynamicMaterial = Mesh->CreateDynamicMaterialInstance(0);

		DynamicMaterial->SetTextureParameterValue("PortalTexture", RenderTarget);
		Mesh->SetMaterial(0, DynamicMaterial);
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
		ACharacter* Character = Cast<ACharacter>(Actor);

		if (!HasAuthority() && !Character->IsLocallyControlled()) continue;

		FVector RelativePosition = GetTransform().InverseTransformPosition(Character->GetActorLocation());
		FVector RelativeVelocity = Entrance->GetComponentTransform().InverseTransformVector(
			Character->GetVelocity());
		FVector PotentialPositionNextFrame = RelativePosition + -RelativeVelocity * GetWorld()->GetDeltaSeconds();

		if (PotentialPositionNextFrame.X <= TeleportationThreshold)
		{
			FVector NewLocation(0.1f, -RelativePosition.Y, RelativePosition.Z);
			NewLocation = LinkedPortal->GetTransform().TransformPosition(NewLocation);

			FQuat RelativeControlRotation = Entrance->GetComponentTransform().InverseTransformRotation(
				Character->GetControlRotation().Quaternion());
			FRotator NewControlRotation = LinkedPortal->GetTransform().TransformRotation(RelativeControlRotation).
			                                            Rotator();
			NewControlRotation.Roll = 0.f;

			FVector NewVelocity = LinkedPortal->GetTransform().TransformVector(RelativeVelocity);

			if (Character->IsLocallyControlled())
			{
				Character->GetController()->SetControlRotation(NewControlRotation);
			}

			Character->GetMovementComponent()->Velocity = FVector::ZeroVector;
			Character->SetActorLocation(NewLocation);
			Character->GetMovementComponent()->Velocity = NewVelocity;
			Character->GetCharacterMovement()->UpdateComponentVelocity();
		}
	}
}

void APPortal::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                     UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                                     const FHitResult& SweepResult)
{
	check(IsValid(Surface));

	if (ACharacter* Character = Cast<ACharacter>(OtherActor))
	{
		Character->GetCapsuleComponent()->IgnoreActorWhenMoving(Surface, true);
	}
}

void APPortal::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                   UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	check(IsValid(Surface));

	if (ACharacter* Character = Cast<ACharacter>(OtherActor))
	{
		Character->GetCapsuleComponent()->IgnoreActorWhenMoving(Surface, false);
	}
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
	InitializeDynamicTarget();
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
