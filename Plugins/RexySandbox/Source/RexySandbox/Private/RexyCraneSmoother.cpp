#include "RexyCraneSmoother.h"

#include "CameraRig_Crane.h"

URexyCraneSmoother::URexyCraneSmoother()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void URexyCraneSmoother::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ACameraRig_Crane* Crane = Cast<ACameraRig_Crane>(GetOwner());
	if (!Crane)
	{
		return;
	}
	float* Values[3] = { &Crane->CraneYaw, &Crane->CranePitch, &Crane->CraneArmLength };
	for (int32 i = 0; i < 3; ++i)
	{
		FChannel& C = Channels[i];
		const float V = *Values[i];
		if (!bInitialised)
		{
			C.Shown = C.Target = C.LastSet = V;
			continue;
		}
		if (V != C.LastSet)
		{
			C.Target = V;                                   // Bridge wrote a new value
		}
		C.Shown = (SmoothingSeconds > 0.f)
			? FMath::FInterpTo(C.Shown, C.Target, DeltaTime, 1.f / SmoothingSeconds)
			: C.Target;
		*Values[i] = C.Shown;
		C.LastSet = C.Shown;
	}
	bInitialised = true;
}
