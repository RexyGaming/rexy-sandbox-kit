// Rexy Sandbox — the blocking viewport: an editor-style free camera for the
// main game window, so the operator can see the whole set, the rigs and the
// action while each camera's own view lives in its monitor window.
//
// Controls (UE editor conventions):
//   RMB + mouse        look            RMB + W/A/S/D/Q/E  fly (wheel = fly speed)
//   MMB + mouse        pan             wheel              dolly in / out
//   Alt + LMB          orbit           Alt + RMB          zoom
//   Shift              faster          F                  frame the whole set
//   1 - 9              frame rig N

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RexyBlockingPawn.generated.h"

class UCameraComponent;

UCLASS(NotPlaceable)
class REXYSANDBOX_API ARexyBlockingPawn : public APawn
{
	GENERATED_BODY()

public:
	ARexyBlockingPawn();

	virtual void Tick(float DeltaSeconds) override;

	/** Look at Target from Distance along the current viewing direction. */
	void Frame(const FVector& Target, float Distance);
	/** Place the camera and look at a point (sets the orbit pivot). */
	void PlaceLookingAt(const FVector& From, const FVector& Target);

	UPROPERTY(VisibleAnywhere, Category = "Rexy")
	TObjectPtr<UCameraComponent> Camera;

	/** Fly speed in cm/s (the wheel adjusts it while RMB is held). */
	UPROPERTY(EditAnywhere, Category = "Rexy")
	float FlySpeed = 600.f;

private:
	float OrbitDistance = 1000.f;
};
