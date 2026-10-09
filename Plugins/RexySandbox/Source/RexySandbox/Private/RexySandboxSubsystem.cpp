#include "RexySandboxSubsystem.h"

#include "CameraRig_Crane.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "RexyBlockingPawn.h"
#include "RexyMonitorLibrary.h"
#include "RexyCraneSmoother.h"
#include "RexyTransportOverlay.h"
#include "Engine/GameViewportClient.h"
#include "LevelSequenceActor.h"
#include "TimerManager.h"

namespace
{
	// The engine's rig meshes (cooked via DirectoriesToAlwaysCook /Engine/EditorMeshes/Camera).
	const TCHAR* CineCamMesh   = TEXT("/Engine/EditorMeshes/Camera/SM_CineCam.SM_CineCam");
	const TCHAR* CraneBaseMesh = TEXT("/Engine/EditorMeshes/Camera/SM_CraneRig_Base.SM_CraneRig_Base");
	const TCHAR* CraneArmMesh  = TEXT("/Engine/EditorMeshes/Camera/SM_CraneRig_Arm.SM_CraneRig_Arm");
	const TCHAR* CraneMountMesh = TEXT("/Engine/EditorMeshes/Camera/SM_CraneRig_Mount.SM_CraneRig_Mount");
	const TCHAR* CraneBodyMesh = TEXT("/Engine/EditorMeshes/Camera/SM_CraneRig_Body.SM_CraneRig_Body");
	// Same numbers the engine uses for its editor preview (ACameraRig_Crane::UpdatePreviewMeshes).
	constexpr float CraneArmMeshSize = 29.f * 0.7f;
	constexpr float CraneMountZ = 40.f;

	USceneComponent* FindComponentNamed(AActor* Actor, const TCHAR* Name)
	{
		TArray<USceneComponent*> Comps;
		Actor->GetComponents<USceneComponent>(Comps);
		for (USceneComponent* C : Comps)
		{
			if (C->GetName() == Name)
			{
				return C;
			}
		}
		return nullptr;
	}
}

void URexySandboxSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// Next tick: the player controller and its default pawn exist by then.
	InWorld.GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &URexySandboxSubsystem::Setup));
}

void URexySandboxSubsystem::Setup()
{
	if (bSetupDone)
	{
		return;
	}
	bSetupDone = true;
	CreateRigProxies();
	TakeOverMainWindow();
	UE_LOG(LogTemp, Display, TEXT("RexySandbox: blocking viewport ready, %d rig model part(s)"), Proxies.Num());
}

UStaticMeshComponent* URexySandboxSubsystem::AddProxy(AActor* Owner, USceneComponent* Parent, const TCHAR* MeshPath,
	const FVector& Location, const FRotator& Rotation, const FVector& Scale)
{
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath);
	if (!Mesh || !Parent)
	{
		UE_LOG(LogTemp, Warning, TEXT("RexySandbox: rig mesh %s not available"), MeshPath);
		return nullptr;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner, NAME_None, RF_Transient);
	C->SetStaticMesh(Mesh);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCanEverAffectNavigation(false);
	C->CastShadow = false;
	C->SetupAttachment(Parent);
	C->SetRelativeLocation(Location);
	C->SetRelativeRotation(Rotation);
	C->SetRelativeScale3D(Scale);
	C->RegisterComponent();
	Proxies.Add(C);
	return C;
}

void URexySandboxSubsystem::CreateRigProxies()
{
	UWorld* World = GetWorld();
	for (TActorIterator<ACineCameraActor> It(World); It; ++It)
	{
		// Same offset the engine gives its editor camera mesh (UCineCameraComponent::
		// ResetProxyMeshTransform): the model is built side-on, lens along +Y.
		AddProxy(*It, It->GetCineCameraComponent(), CineCamMesh, FVector(-46.f, 0.f, -24.f), FRotator(0.f, 90.f, 0.f), FVector::OneVector);
	}
	for (TActorIterator<ACameraRig_Crane> It(World); It; ++It)
	{
		ACameraRig_Crane* Crane = *It;
		USceneComponent* Root = Crane->GetRootComponent();
		USceneComponent* Pitch = FindComponentNamed(Crane, TEXT("CranePitchControl"));
		const FRotator Side(0.f, 90.f, 0.f);
		const FVector S(0.7f);
		AddProxy(Crane, Root, CraneBaseMesh, FVector::ZeroVector, FRotator::ZeroRotator, FVector::OneVector);
		AddProxy(Crane, Pitch, CraneBodyMesh, FVector::ZeroVector, Side, S);
		FRexyCraneProxy P;
		P.Crane = Crane;
		P.Arm = AddProxy(Crane, Pitch, CraneArmMesh, FVector(0.f, 0.f, 52.f), Side, S);
		P.Mount = AddProxy(Crane, Pitch, CraneMountMesh, FVector(Crane->CraneArmLength, 0.f, CraneMountZ), Side, S);
		UpdateCraneProxy(P);
		Cranes.Add(P);
		// Ease Bridge's crane writes; tick before the crane so it builds from eased values.
		URexyCraneSmoother* Smoother = NewObject<URexyCraneSmoother>(Crane, NAME_None, RF_Transient);
		Smoother->RegisterComponent();
		Crane->AddTickPrerequisiteComponent(Smoother);
		Smoothers.Add(Smoother);
	}
	ApplyVisibility();
}

void URexySandboxSubsystem::UpdateCraneProxy(FRexyCraneProxy& P)
{
	if (!P.Crane || P.Crane->CraneArmLength == P.LastArmLength)
	{
		return;
	}
	const float Len = P.Crane->CraneArmLength;
	P.LastArmLength = Len;
	if (P.Arm)
	{
		FVector Scale = P.Arm->GetRelativeScale3D();
		Scale.Y = Len / CraneArmMeshSize * 0.7f;
		P.Arm->SetRelativeScale3D(Scale);
	}
	if (P.Mount)
	{
		P.Mount->SetRelativeLocation(FVector(Len, 0.f, CraneMountZ));
	}
}

void URexySandboxSubsystem::Tick(float DeltaTime)
{
	for (FRexyCraneProxy& P : Cranes)
	{
		UpdateCraneProxy(P);   // Bridge changes the arm length live
	}
}

void URexySandboxSubsystem::TakeOverMainWindow()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	BlockingPawn = World->SpawnActor<ARexyBlockingPawn>(ARexyBlockingPawn::StaticClass(), FTransform::Identity, Params);
	if (!BlockingPawn)
	{
		return;
	}
	APawn* Old = PC->GetPawn();
	PC->Possess(BlockingPawn);
	if (Old && Old != BlockingPawn)
	{
		Old->Destroy();
	}
	PC->SetViewTarget(BlockingPawn);
	FrameAll();

	// Free cursor: captured only while a mouse button is held (for fly / orbit),
	// never locked to the window, so it moves freely to Bridge and the monitors.
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(true);
	PC->SetInputMode(Mode);
	PC->bShowMouseCursor = true;

	// The action's transport, inside the main window only (never in a monitor / take).
	bool bHasAction = false;
	for (TActorIterator<ALevelSequenceActor> It(World); It; ++It) { bHasAction = true; break; }
	if (bHasAction && GEngine && GEngine->GameViewport)
	{
		ActionOverlay = SNew(SRexyTransportOverlay).World(World);
		GEngine->GameViewport->AddViewportWidgetContent(ActionOverlay.ToSharedRef(), 50);
	}
}

TArray<AActor*> URexySandboxSubsystem::Rigs() const
{
	TArray<AActor*> Out;
	UWorld* World = GetWorld();
	// Cranes first, then free cameras (cameras mounted on a crane are covered by the crane).
	for (TActorIterator<ACameraRig_Crane> It(World); It; ++It)
	{
		Out.Add(*It);
	}
	for (TActorIterator<ACineCameraActor> It(World); It; ++It)
	{
		AActor* Parent = It->GetAttachParentActor();
		if (!Parent || !Parent->IsA<ACameraRig_Crane>())
		{
			Out.Add(*It);
		}
	}
	return Out;
}

void URexySandboxSubsystem::FrameAll()
{
	if (!BlockingPawn)
	{
		return;
	}
	FBox Box(ForceInit);
	for (AActor* A : Rigs())
	{
		Box += A->GetActorLocation();
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("RexyStandIn")))
		{
			Box += It->GetActorLocation();
		}
	}
	if (!Box.IsValid)
	{
		Box = FBox(FVector(-500.f), FVector(500.f));
	}
	const FVector Centre = Box.GetCenter();
	const float Radius = FMath::Max(Box.GetExtent().Size(), 500.f);
	// A high three-quarter view, like a set plan from the corner of the stage.
	const FVector From = Centre + FVector(-0.75f, -0.55f, 0.55f).GetSafeNormal() * Radius * 2.2f;
	BlockingPawn->PlaceLookingAt(From, Centre);
}

void URexySandboxSubsystem::FrameRigIndex(int32 Index)
{
	const TArray<AActor*> All = Rigs();
	if (All.IsValidIndex(Index))
	{
		FrameActor(All[Index]);
	}
}

void URexySandboxSubsystem::FrameActor(AActor* Actor)
{
	if (!BlockingPawn || !Actor)
	{
		return;
	}
	FVector Origin, Extent;
	Actor->GetActorBounds(false, Origin, Extent, true);
	const float Distance = FMath::Max(Extent.Size() * 3.f, 400.f);
	BlockingPawn->Frame(Origin, Distance);
}

TArray<const AActor*> URexySandboxSubsystem::RigActorsOf(const ACineCameraActor* Camera)
{
	TArray<const AActor*> Out;
	if (Camera)
	{
		Out.Add(Camera);
		if (const AActor* Parent = Camera->GetAttachParentActor(); Parent && Parent->IsA<ACameraRig_Crane>())
		{
			Out.Add(Parent);
		}
	}
	return Out;
}

TArray<UPrimitiveComponent*> URexySandboxSubsystem::HiddenForCamera(const ACineCameraActor* Camera) const
{
	const TArray<const AActor*> Own = RigActorsOf(Camera);
	TArray<UPrimitiveComponent*> Out;
	for (UPrimitiveComponent* C : Proxies)
	{
		const AActor* Owner = C ? C->GetOwner() : nullptr;
		if (Owner && (Own.Contains(Owner) || HiddenRigs.Contains(Owner)))
		{
			Out.Add(C);
		}
	}
	return Out;
}

void URexySandboxSubsystem::ApplyVisibility()
{
	UWorld* World = GetWorld();
	// Main window: the blocking viewport shows every rig (unless Rigs is off);
	// a camera in the main window sees what its monitor would.
	if (APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr)
	{
		PC->HiddenPrimitiveComponents.Reset();
		if (MainCamera.IsValid())
		{
			for (UPrimitiveComponent* C : HiddenForCamera(MainCamera.Get())) PC->HiddenPrimitiveComponents.Add(C);
		}
		else if (!bRigsVisible)
		{
			for (UPrimitiveComponent* C : Proxies) PC->HiddenPrimitiveComponents.Add(C);
		}
	}
	if (URexyMonitorSubsystem* Monitors = World ? World->GetSubsystem<URexyMonitorSubsystem>() : nullptr)
	{
		Monitors->RefreshHidden();
	}
}

void URexySandboxSubsystem::SetActionOverlayVisible(bool bVisible)
{
	if (ActionOverlay.IsValid()) ActionOverlay->SetVisibility(bVisible ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);
}

void URexySandboxSubsystem::Deinitialize()
{
	if (ActionOverlay.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(ActionOverlay.ToSharedRef());
	}
	ActionOverlay.Reset();
	Super::Deinitialize();
}

void URexySandboxSubsystem::SetRigSmoothing(float Seconds)
{
	for (URexyCraneSmoother* S : Smoothers)
	{
		if (S) S->SmoothingSeconds = FMath::Clamp(Seconds, 0.f, 1.f);
	}
}

void URexySandboxSubsystem::SetRigsVisible(bool bVisible)
{
	bRigsVisible = bVisible;
	ApplyVisibility();
}

void URexySandboxSubsystem::SetRigHidden(ACineCameraActor* Camera, bool bHidden)
{
	for (const AActor* A : RigActorsOf(Camera))
	{
		if (bHidden) HiddenRigs.Add(A);
		else HiddenRigs.Remove(A);
	}
	ApplyVisibility();
}

void URexySandboxSubsystem::SetMainViewCamera(ACineCameraActor* Camera)
{
	UWorld* World = GetWorld();
	if (APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr)
	{
		PC->SetViewTarget(Camera ? static_cast<AActor*>(Camera) : static_cast<AActor*>(BlockingPawn.Get()));
	}
	MainCamera = Camera;
	ApplyVisibility();
}
