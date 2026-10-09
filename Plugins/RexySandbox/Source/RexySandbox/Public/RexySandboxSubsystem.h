// Rexy Sandbox — per-world set-up for a practice level running under Rexy Bridge:
//  * visible rig models (camera bodies, crane base / arm / mount / counterweight)
//    so the blocking viewport shows where the equipment is — the engine's own
//    rig meshes are editor-only;
//  * the blocking viewport: an ARexyBlockingPawn takes over the main window;
//  * the mouse is never trapped (the cameras are driven by Bridge, not the mouse).
// Who sees which rig: the blocking viewport shows every rig (Rigs toggle); a
// camera never sees its own rig; a rig marked hidden disappears from every
// camera's view (monitors, and the main window when it shows a camera).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "RexySandboxSubsystem.generated.h"

class ACameraRig_Crane;
class ACineCameraActor;
class ARexyBlockingPawn;
class UPrimitiveComponent;
class UStaticMeshComponent;
class URexyCraneSmoother;

USTRUCT()
struct FRexyCraneProxy
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<ACameraRig_Crane> Crane = nullptr;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Arm = nullptr;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Mount = nullptr;
	float LastArmLength = -1.f;
};

UCLASS()
class REXYSANDBOX_API URexySandboxSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(URexySandboxSubsystem, STATGROUP_Tickables); }

	/** Rig-model components — the monitor windows hide these. */
	const TArray<TObjectPtr<UPrimitiveComponent>>& GetProxies() const { return Proxies; }

	void SetRigsVisible(bool bVisible);
	/** Crane easing time constant in seconds (0 = off). */
	void SetRigSmoothing(float Seconds);
	/** Hide a camera's rig (the camera and, if mounted, its crane) from the camera views. */
	void SetRigHidden(ACineCameraActor* Camera, bool bHidden);
	/** Rig parts a view through this camera must not show. */
	TArray<UPrimitiveComponent*> HiddenForCamera(const ACineCameraActor* Camera) const;
	void FrameAll();
	void FrameRigIndex(int32 Index);
	void FrameActor(AActor* Actor);
	/** Show a camera in the main window, or nullptr for the blocking viewport. */
	void SetMainViewCamera(ACineCameraActor* Camera);
	ARexyBlockingPawn* GetBlockingPawn() const { return BlockingPawn; }
	/** The in-game action transport (main window only). */
	void SetActionOverlayVisible(bool bVisible);
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	void Setup();
	void CreateRigProxies();
	UStaticMeshComponent* AddProxy(AActor* Owner, USceneComponent* Parent, const TCHAR* MeshPath,
		const FVector& Location, const FRotator& Rotation, const FVector& Scale);
	void UpdateCraneProxy(FRexyCraneProxy& P);
	void TakeOverMainWindow();
	TArray<AActor*> Rigs() const;
	void ApplyVisibility();
	static TArray<const AActor*> RigActorsOf(const ACineCameraActor* Camera);

	UPROPERTY() TArray<TObjectPtr<UPrimitiveComponent>> Proxies;
	UPROPERTY() TArray<FRexyCraneProxy> Cranes;
	UPROPERTY() TObjectPtr<ARexyBlockingPawn> BlockingPawn = nullptr;
	UPROPERTY() TArray<TObjectPtr<URexyCraneSmoother>> Smoothers;
	bool bSetupDone = false;
	bool bRigsVisible = true;
	TWeakObjectPtr<ACineCameraActor> MainCamera;
	TSet<TWeakObjectPtr<const AActor>> HiddenRigs;
	TSharedPtr<class SRexyTransportOverlay> ActionOverlay;
};
