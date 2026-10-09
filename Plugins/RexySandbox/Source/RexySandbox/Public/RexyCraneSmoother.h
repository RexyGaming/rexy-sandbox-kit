// Rexy Sandbox — eases a crane's yaw / pitch / arm length toward the values Rexy
// Bridge writes over Remote Control. Writes arrive at an uneven ~60 Hz (network +
// JS timing) while the game renders every frame; without easing the crane holds,
// then jumps — visible as strobing/judder, worst when the camera is close to the
// subject. Ticks BEFORE the crane (tick prerequisite) so the crane always builds
// its components from the eased values. Bridge only ever writes these three as
// absolute values, so "newest write = target" is safe.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RexyCraneSmoother.generated.h"

UCLASS(ClassGroup = Rexy)
class REXYSANDBOX_API URexyCraneSmoother : public UActorComponent
{
	GENERATED_BODY()

public:
	URexyCraneSmoother();

	/** Time constant in seconds (0 = off). */
	float SmoothingSeconds = 0.05f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	struct FChannel
	{
		float Shown = 0.f;
		float Target = 0.f;
		float LastSet = 0.f;
	};
	FChannel Channels[3];
	bool bInitialised = false;
};
