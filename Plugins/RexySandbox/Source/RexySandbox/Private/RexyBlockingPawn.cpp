#include "RexyBlockingPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "RexySandboxSubsystem.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
	// PlayerInput's mouse deltas arrive pre-scaled (~0.07 per pixel); these give
	// roughly editor feel at 1080p-4K.
	constexpr float LookDegreesPerUnit = 3.0f;
	constexpr float PanPerUnitPerMetre = 0.02f;   // x distance to the pivot
	constexpr float MinOrbitDistance = 50.f;
}

ARexyBlockingPawn::ARexyBlockingPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);
	Camera->SetFieldOfView(70.f);
	Camera->bConstrainAspectRatio = false;
	bUseControllerRotationYaw = bUseControllerRotationPitch = bUseControllerRotationRoll = false;
	SetCanBeDamaged(false);
}

void ARexyBlockingPawn::PlaceLookingAt(const FVector& From, const FVector& Target)
{
	SetActorLocation(From);
	SetActorRotation((Target - From).Rotation());
	OrbitDistance = FMath::Max(MinOrbitDistance, FVector::Dist(From, Target));
}

void ARexyBlockingPawn::Frame(const FVector& Target, float Distance)
{
	OrbitDistance = FMath::Max(MinOrbitDistance, Distance);
	SetActorLocation(Target - GetActorForwardVector() * OrbitDistance);
}

void ARexyBlockingPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	float MouseX = 0.f, MouseY = 0.f;
	PC->GetInputMouseDelta(MouseX, MouseY);
	const bool bRMB = PC->IsInputKeyDown(EKeys::RightMouseButton);
	const bool bMMB = PC->IsInputKeyDown(EKeys::MiddleMouseButton);
	const bool bLMB = PC->IsInputKeyDown(EKeys::LeftMouseButton);
	const bool bAlt = PC->IsInputKeyDown(EKeys::LeftAlt) || PC->IsInputKeyDown(EKeys::RightAlt);
	const bool bShift = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift);
	const int32 Wheel = (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp) ? 1 : 0) - (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown) ? 1 : 0);
	const float Boost = bShift ? 3.f : 1.f;

	// A mouse press in the main view hands the keyboard back to the game view —
	// a monitor window (or Bridge) may have had it, and WASD would go nowhere.
	if ((PC->WasInputKeyJustPressed(EKeys::RightMouseButton) || PC->WasInputKeyJustPressed(EKeys::MiddleMouseButton)
		|| PC->WasInputKeyJustPressed(EKeys::LeftMouseButton)) && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}

	FRotator Rot = GetActorRotation();
	FVector Loc = GetActorLocation();

	if (bAlt && bLMB)
	{
		// Orbit around the pivot in front of the camera.
		const FVector Pivot = Loc + Rot.Vector() * OrbitDistance;
		Rot.Yaw += MouseX * LookDegreesPerUnit;
		Rot.Pitch = FMath::Clamp(Rot.Pitch + MouseY * LookDegreesPerUnit, -89.f, 89.f);
		Loc = Pivot - Rot.Vector() * OrbitDistance;
	}
	else if (bAlt && bRMB)
	{
		// Zoom toward / away from the pivot.
		const float Step = (MouseX + MouseY) * OrbitDistance * 0.02f * Boost;
		const float NewDistance = FMath::Max(MinOrbitDistance, OrbitDistance - Step);
		Loc += Rot.Vector() * (OrbitDistance - NewDistance);
		OrbitDistance = NewDistance;
	}
	else if (bRMB)
	{
		// Mouse look + fly.
		Rot.Yaw += MouseX * LookDegreesPerUnit;
		Rot.Pitch = FMath::Clamp(Rot.Pitch + MouseY * LookDegreesPerUnit, -89.f, 89.f);
		if (Wheel != 0)
		{
			FlySpeed = FMath::Clamp(FlySpeed * (Wheel > 0 ? 1.25f : 0.8f), 50.f, 20000.f);
		}
		const FRotationMatrix M(Rot);
		FVector Move = FVector::ZeroVector;
		if (PC->IsInputKeyDown(EKeys::W)) Move += M.GetUnitAxis(EAxis::X);
		if (PC->IsInputKeyDown(EKeys::S)) Move -= M.GetUnitAxis(EAxis::X);
		if (PC->IsInputKeyDown(EKeys::D)) Move += M.GetUnitAxis(EAxis::Y);
		if (PC->IsInputKeyDown(EKeys::A)) Move -= M.GetUnitAxis(EAxis::Y);
		if (PC->IsInputKeyDown(EKeys::E)) Move += FVector::UpVector;
		if (PC->IsInputKeyDown(EKeys::Q)) Move -= FVector::UpVector;
		Loc += Move.GetSafeNormal() * FlySpeed * Boost * DeltaSeconds;
	}
	else if (bMMB)
	{
		// Pan: the set follows the mouse.
		const FRotationMatrix M(Rot);
		const float Scale = FMath::Max(OrbitDistance, 100.f) * PanPerUnitPerMetre * Boost;
		Loc -= M.GetUnitAxis(EAxis::Y) * MouseX * Scale;
		Loc -= M.GetUnitAxis(EAxis::Z) * MouseY * Scale;
	}
	else if (Wheel != 0)
	{
		// Dolly in / out, proportional to how far away the pivot is.
		const float Step = FMath::Max(OrbitDistance * 0.15f, 20.f) * Wheel * Boost;
		const float NewDistance = FMath::Max(MinOrbitDistance, OrbitDistance - Step);
		Loc += Rot.Vector() * Step;
		OrbitDistance = NewDistance;
	}

	SetActorLocationAndRotation(Loc, Rot);

	// Framing shortcuts.
	if (URexySandboxSubsystem* Sandbox = GetWorld()->GetSubsystem<URexySandboxSubsystem>())
	{
		if (PC->WasInputKeyJustPressed(EKeys::F))
		{
			Sandbox->FrameAll();
		}
		static const FKey Digits[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
		for (int32 i = 0; i < UE_ARRAY_COUNT(Digits); ++i)
		{
			if (PC->WasInputKeyJustPressed(Digits[i]))
			{
				Sandbox->FrameRigIndex(i);
			}
		}
	}
}
