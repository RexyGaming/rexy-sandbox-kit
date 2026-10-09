// Rexy Sandbox — camera monitor windows.
//
// A monitor is a separate OS window showing a live view of a CineCamera: a
// SceneCaptureComponent2D on the camera renders into a render target that a
// Slate window displays. The window can be dragged to any screen and resized;
// closing it removes the capture. Rexy Bridge calls these over Remote Control
// (object /Script/RexySandbox.Default__RexyMonitorLibrary), passing the camera
// actor as the world context, exactly like GameplayStatics.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "RexyMonitorLibrary.generated.h"

class ACineCameraActor;
class UPrimitiveComponent;
class SWindow;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
struct FSlateBrush;

/** Where a monitor window sits, in screen pixels — Bridge saves these per level. */
USTRUCT(BlueprintType)
struct FRexyMonitorRect
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Rexy") FString Camera;
	UPROPERTY(BlueprintReadOnly, Category = "Rexy") int32 X = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rexy") int32 Y = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rexy") int32 Width = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Rexy") int32 Height = 0;
};

USTRUCT()
struct FRexyMonitor
{
	GENERATED_BODY()

	UPROPERTY() TObjectPtr<ACineCameraActor> Camera = nullptr;
	UPROPERTY() TObjectPtr<USceneCaptureComponent2D> Capture = nullptr;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> Target = nullptr;

	TSharedPtr<SWindow> Window;
	TSharedPtr<FSlateBrush> Brush;
	FVector2D LastClient = FVector2D::ZeroVector;   // window size last tick (resize settles first)
	double ClientSince = 0.0;
};

/** Owns the open monitors for one world and keeps each capture matched to its camera's lens. */
UCLASS()
class REXYSANDBOX_API URexyMonitorSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	bool Open(ACineCameraActor* Camera, int32 Width, int32 Height, int32 X, int32 Y, const FString& Title);
	bool Close(ACineCameraActor* Camera);
	void CloseAll();
	TArray<FString> OpenCameraNames() const;
	TArray<FRexyMonitorRect> Layout() const;
	/** Re-apply which rig models each monitor must not show (own rig + hidden rigs). */
	void RefreshHidden();
	/** How monitors render: at most MaxWidth pixels wide (0 = the window's size),
	 *  scaled up to fill the window; bFast = no Lumen in the monitors. */
	void SetQuality(int32 MaxWidth, bool bFast);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(URexyMonitorSubsystem, STATGROUP_Tickables); }
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	void Remove(int32 Index, bool bDestroyWindow);
	/** Size the monitor's render to its window, capped at MaxRenderWidth. */
	void FitTarget(FRexyMonitor& M);

	UPROPERTY() TArray<FRexyMonitor> Monitors;
	// A capture ignores screen percentage: a near-full-screen 4K monitor rendered
	// ~4.5 MP with Lumen every frame (~15 fps on a laptop RTX 2060, Rob 2026-10-01).
	int32 MaxRenderWidth = 1280;
	bool bFastLighting = false;
};

UCLASS()
class REXYSANDBOX_API URexyMonitorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Open (or bring to front) a monitor window for Camera. X/Y < 0 lets the OS place it. */
	UFUNCTION(BlueprintCallable, Category = "Rexy|Monitors")
	static bool OpenMonitor(ACineCameraActor* Camera, int32 Width = 960, int32 Height = 540, int32 X = -1, int32 Y = -1, const FString& Title = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "Rexy|Monitors")
	static bool CloseMonitor(ACineCameraActor* Camera);

	UFUNCTION(BlueprintCallable, Category = "Rexy|Monitors", meta = (WorldContext = "WorldContextObject"))
	static void CloseAllMonitors(UObject* WorldContextObject);

	/** Names of cameras with an open monitor window. */
	UFUNCTION(BlueprintCallable, Category = "Rexy|Monitors", meta = (WorldContext = "WorldContextObject"))
	static TArray<FString> GetOpenMonitors(UObject* WorldContextObject);

	/** Screen position + client size of every open monitor window. */
	UFUNCTION(BlueprintCallable, Category = "Rexy|Monitors", meta = (WorldContext = "WorldContextObject"))
	static TArray<FRexyMonitorRect> GetMonitorLayout(UObject* WorldContextObject);

	/** Monitor render quality: MaxWidth pixels at most (0 = full window size; the picture
	 *  is scaled up to fill the window), bFastLighting = no Lumen GI / reflections. */
	UFUNCTION(BlueprintCallable, Category = "Rexy|Monitors", meta = (WorldContext = "WorldContextObject"))
	static void SetMonitorQuality(UObject* WorldContextObject, int32 MaxWidth = 1280, bool bFastLighting = false);
};

/** The blocking viewport and rig models, callable from Rexy Bridge over Remote Control. */
UCLASS()
class REXYSANDBOX_API URexySandboxLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Bridge checks this to know the level runs on the Sandbox plugin. */
	UFUNCTION(BlueprintCallable, Category = "Rexy|Sandbox")
	static int32 GetSandboxVersion() { return 4; }   // 2: SetLook · 3: SetMonitorQuality · 4: SetFastLighting

	UFUNCTION(BlueprintCallable, Category = "Rexy|Sandbox", meta = (WorldContext = "WorldContextObject"))
	static void FrameAll(UObject* WorldContextObject);

	/** Point the blocking viewport at an actor (a camera or crane). */
	UFUNCTION(BlueprintCallable, Category = "Rexy|Sandbox")
	static void FrameActor(AActor* Actor);

	/** Rig models on/off in the blocking viewport. */
	UFUNCTION(BlueprintCallable, Category = "Rexy|Sandbox", meta = (WorldContext = "WorldContextObject"))
	static void SetRigsVisible(UObject* WorldContextObject, bool bVisible);

	/** Crane easing (seconds, 0 = off) — smooths Bridge's crane writes into continuous motion. */
	UFUNCTION(BlueprintCallable, Category = "Rexy|Sandbox", meta = (WorldContext = "WorldContextObject"))
	static void SetRigSmoothing(UObject* WorldContextObject, float Seconds);

	/** The look of every view (main window + monitors): motion blur as a camera shutter
	 *  angle (0 = off, 180 = film) and a frame-rate lock (0 = off; locked = VSync on).
	 *  With both, the blur is timed for the locked rate, like a real camera's shutter.
	 *  Only these settings — Remote Control's general console access stays off. */
	UFUNCTION(BlueprintCallable, Category = "Rexy|Sandbox")
	static void SetLook(float ShutterDegrees, int32 FrameLock);

	/** Whole-game fast lighting: no Lumen GI / reflections, regular shadow maps instead
	 *  of virtual ones. Off restores what the level started with. */
	UFUNCTION(BlueprintCallable, Category = "Rexy|Sandbox")
	static void SetFastLighting(bool bFast);

	/** Hide a camera's rig from every camera view (it stays in the blocking viewport). */
	UFUNCTION(BlueprintCallable, Category = "Rexy|Sandbox")
	static void SetRigHidden(ACineCameraActor* Camera, bool bHidden);

	/** The in-game action controls (main window only) on / off. */
	UFUNCTION(BlueprintCallable, Category = "Rexy|Sandbox", meta = (WorldContext = "WorldContextObject"))
	static void SetActionOverlayVisible(UObject* WorldContextObject, bool bVisible);

	/** Show a camera in the main window; pass no camera for the blocking viewport. */
	UFUNCTION(BlueprintCallable, Category = "Rexy|Sandbox", meta = (WorldContext = "WorldContextObject"))
	static void SetMainViewCamera(UObject* WorldContextObject, ACineCameraActor* Camera);
};
