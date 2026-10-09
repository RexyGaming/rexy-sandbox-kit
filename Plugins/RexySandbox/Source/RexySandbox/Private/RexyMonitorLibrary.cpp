#include "RexyMonitorLibrary.h"
#include "RexySandboxSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Scene.h"

// Draws the capture ignoring its alpha: a scene capture's final-colour alpha is
// not opacity (it comes out empty), so a plain SImage renders it transparent.
class SRexyMonitorImage : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SRexyMonitorImage) {}
		SLATE_ARGUMENT(const FSlateBrush*, Brush)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs) { Brush = InArgs._Brush; }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		if (Brush)
		{
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry.ToPaintGeometry(), Brush,
				ESlateDrawEffect::IgnoreTextureAlpha | ESlateDrawEffect::NoGamma, FLinearColor::White);
		}
		return LayerId;
	}

	virtual FVector2D ComputeDesiredSize(float) const override { return Brush ? FVector2D(Brush->ImageSize) : FVector2D::ZeroVector; }

private:
	const FSlateBrush* Brush = nullptr;
};

// ── subsystem ──────────────────────────────────────────────────────────────

bool URexyMonitorSubsystem::Open(ACineCameraActor* Camera, int32 Width, int32 Height, int32 X, int32 Y, const FString& Title)
{
	if (!Camera || !FSlateApplication::IsInitialized())
	{
		return false;
	}
	for (FRexyMonitor& M : Monitors)
	{
		if (M.Camera == Camera && M.Window.IsValid())
		{
			M.Window->BringToFront();
			return true;
		}
	}

	UCineCameraComponent* Cine = Camera->GetCineCameraComponent();
	const float Aspect = (Cine && Cine->Filmback.SensorAspectRatio > 0.f) ? Cine->Filmback.SensorAspectRatio : 16.f / 9.f;
	Width = FMath::Clamp(Width, 160, 3840);
	// Height follows the camera's filmback so the monitor frames exactly what the camera frames.
	Height = Height > 0 ? FMath::Clamp(Height, 90, 2160) : FMath::RoundToInt(Width / Aspect);
	const int32 RtHeight = FMath::RoundToInt(Width / Aspect);

	FRexyMonitor M;
	M.Camera = Camera;

	M.Target = NewObject<UTextureRenderTarget2D>(this);
	M.Target->RenderTargetFormat = RTF_RGBA8;
	M.Target->ClearColor = FLinearColor::Black;
	const int32 RtWidth = MaxRenderWidth > 0 ? FMath::Min(Width, MaxRenderWidth) : Width;
	M.Target->InitAutoFormat(RtWidth, FMath::Max(90, FMath::RoundToInt(RtWidth / Aspect)));
	M.Target->UpdateResourceImmediate(true);

	M.Capture = NewObject<USceneCaptureComponent2D>(Camera, NAME_None, RF_Transient);
	M.Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	M.Capture->TextureTarget = M.Target;
	M.Capture->bCaptureEveryFrame = true;
	M.Capture->bCaptureOnMovement = false;
	M.Capture->bAlwaysPersistRenderingState = true;   // temporal AA / exposure history, like a real view
	// Scene captures switch motion blur off by default. On here, so a monitor shows
	// the same shutter as the main view — Bridge's Sandbox Views "Motion blur"
	// (r.MotionBlur.Amount) decides how much; 0 = none, as before.
	M.Capture->ShowFlags.SetMotionBlur(true);
	M.Capture->RegisterComponent();
	M.Capture->AttachToComponent(Cine ? static_cast<USceneComponent*>(Cine) : Camera->GetRootComponent(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	M.Brush = MakeShared<FSlateBrush>();
	M.Brush->SetResourceObject(M.Target);
	M.Brush->ImageSize = FVector2D(Width, RtHeight);       // layout aspect; the render may be smaller
	M.Brush->DrawAs = ESlateBrushDrawType::Image;

	const FString WindowTitle = Title.IsEmpty() ? Camera->GetName() : Title;
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(FText::FromString(WindowTitle))
		.ClientSize(FVector2D(Width, Height))
		.SizingRule(ESizingRule::UserSized)
		.AutoCenter(X < 0 || Y < 0 ? EAutoCenter::PreferredWorkArea : EAutoCenter::None)
		.ScreenPosition(FVector2D(FMath::Max(X, 0), FMath::Max(Y, 0)))
		// Sizes and positions are real screen pixels (Bridge works in physical
		// pixels); don't let Slate multiply them by the display scale.
		.AdjustInitialSizeAndPositionForDPIScale(false)
		.UseOSWindowBorder(true)
		// Don't take the keyboard from the game when a monitor opens: the blocking
		// viewport's WASD must keep working (Rob 2026-09-29).
		.FocusWhenFirstShown(false)
		.SupportsMaximize(true)
		.SupportsMinimize(true)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
			.BorderBackgroundColor(FLinearColor::Black)
			.Padding(0)
			[
				SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFit)
				[
					SNew(SRexyMonitorImage).Brush(M.Brush.Get())
				]
			]
		];

	// Closing the window from the OS (title-bar X) tears the capture down too.
	TWeakObjectPtr<ACineCameraActor> WeakCam(Camera);
	Window->SetOnWindowClosed(FOnWindowClosed::CreateWeakLambda(this, [this, WeakCam](const TSharedRef<SWindow>&)
	{
		const int32 Index = Monitors.IndexOfByPredicate([&](const FRexyMonitor& Mon) { return Mon.Camera == WeakCam.Get(); });
		if (Index != INDEX_NONE)
		{
			Remove(Index, /*bDestroyWindow*/ false);
		}
	}));

	M.Window = Window;
	Monitors.Add(M);
	RefreshHidden();                                   // never its own rig; not hidden rigs
	FSlateApplication::Get().AddWindow(Window, /*bShowImmediately*/ true);
	FSlateApplication::Get().SetAllUserFocusToGameViewport();
	UE_LOG(LogTemp, Display, TEXT("RexySandbox: monitor open for %s (%dx%d)"), *Camera->GetName(), Width, Height);
	return true;
}

bool URexyMonitorSubsystem::Close(ACineCameraActor* Camera)
{
	const int32 Index = Monitors.IndexOfByPredicate([&](const FRexyMonitor& M) { return M.Camera == Camera; });
	if (Index == INDEX_NONE)
	{
		return false;
	}
	Remove(Index, /*bDestroyWindow*/ true);
	return true;
}

void URexyMonitorSubsystem::CloseAll()
{
	for (int32 i = Monitors.Num() - 1; i >= 0; --i)
	{
		Remove(i, /*bDestroyWindow*/ true);
	}
}

TArray<FString> URexyMonitorSubsystem::OpenCameraNames() const
{
	TArray<FString> Names;
	for (const FRexyMonitor& M : Monitors)
	{
		if (M.Camera)
		{
			Names.Add(M.Camera->GetName());
		}
	}
	return Names;
}

TArray<FRexyMonitorRect> URexyMonitorSubsystem::Layout() const
{
	TArray<FRexyMonitorRect> Out;
	// The game's own window too ("__main__"), so Bridge can reopen it where it was left.
	if (GEngine && GEngine->GameViewport)
	{
		if (TSharedPtr<SWindow> Main = GEngine->GameViewport->GetWindow())
		{
			FRexyMonitorRect R;
			R.Camera = TEXT("__main__");
			const FVector2D Pos = Main->GetPositionInScreen();
			const FVector2D Size = Main->GetClientSizeInScreen();
			R.X = FMath::RoundToInt(Pos.X);
			R.Y = FMath::RoundToInt(Pos.Y);
			R.Width = FMath::RoundToInt(Size.X);
			R.Height = FMath::RoundToInt(Size.Y);
			Out.Add(R);
		}
	}
	for (const FRexyMonitor& M : Monitors)
	{
		if (M.Camera && M.Window.IsValid())
		{
			FRexyMonitorRect R;
			R.Camera = M.Camera->GetName();
			const FVector2D Pos = M.Window->GetPositionInScreen();
			const FVector2D Size = M.Window->GetClientSizeInScreen();
			R.X = FMath::RoundToInt(Pos.X);
			R.Y = FMath::RoundToInt(Pos.Y);
			R.Width = FMath::RoundToInt(Size.X);
			R.Height = FMath::RoundToInt(Size.Y);
			Out.Add(R);
		}
	}
	return Out;
}

void URexyMonitorSubsystem::RefreshHidden()
{
	URexySandboxSubsystem* Sandbox = GetWorld() ? GetWorld()->GetSubsystem<URexySandboxSubsystem>() : nullptr;
	for (FRexyMonitor& M : Monitors)
	{
		if (!M.Capture)
		{
			continue;
		}
		M.Capture->ClearHiddenComponents();
		if (Sandbox)
		{
			for (UPrimitiveComponent* P : Sandbox->HiddenForCamera(M.Camera))
			{
				M.Capture->HideComponent(P);
			}
		}
	}
}

void URexyMonitorSubsystem::Remove(int32 Index, bool bDestroyWindow)
{
	FRexyMonitor M = Monitors[Index];
	Monitors.RemoveAt(Index);
	if (M.Capture)
	{
		M.Capture->TextureTarget = nullptr;
		M.Capture->DestroyComponent();
	}
	if (M.Target)
	{
		M.Target->ReleaseResource();
	}
	if (bDestroyWindow && M.Window.IsValid())
	{
		M.Window->SetOnWindowClosed(FOnWindowClosed());
		M.Window->RequestDestroyWindow();
	}
}

void URexyMonitorSubsystem::Tick(float DeltaTime)
{
	for (int32 i = Monitors.Num() - 1; i >= 0; --i)
	{
		FRexyMonitor& M = Monitors[i];
		if (!IsValid(M.Camera) || !IsValid(M.Capture))
		{
			Remove(i, /*bDestroyWindow*/ true);
			continue;
		}
		// Match the lens every frame: focal length (FOV) and the camera's post
		// process, including the depth of field its focus settings produce.
		if (UCineCameraComponent* Cine = M.Camera->GetCineCameraComponent())
		{
			FMinimalViewInfo View;
			Cine->GetCameraView(DeltaTime, View);
			M.Capture->FOVAngle = View.FOV;
			M.Capture->PostProcessSettings = View.PostProcessSettings;
			M.Capture->PostProcessBlendWeight = View.PostProcessBlendWeight;
		}
		if (bFastLighting)
		{
			// Fast: no Lumen in the monitors (the main window keeps it).
			FPostProcessSettings& PP = M.Capture->PostProcessSettings;
			PP.bOverride_DynamicGlobalIlluminationMethod = true;
			PP.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::None;
			PP.bOverride_ReflectionMethod = true;
			PP.ReflectionMethod = EReflectionMethod::ScreenSpace;
			M.Capture->PostProcessBlendWeight = 1.f;
		}
		FitTarget(M);
	}
}

void URexyMonitorSubsystem::FitTarget(FRexyMonitor& M)
{
	if (!M.Target || !M.Window.IsValid())
	{
		return;
	}
	const FVector2D Client = M.Window->GetClientSizeInScreen();
	if (Client.X < 16.f || Client.Y < 16.f)
	{
		return;                                                // minimised
	}
	// Wait for a drag-resize to settle (a new render every frame of a drag thrashes the GPU).
	const double Now = FPlatformTime::Seconds();
	if (!Client.Equals(M.LastClient, 1.f)) { M.LastClient = Client; M.ClientSince = Now; return; }
	if (Now - M.ClientSince < 0.3) return;
	// The camera's aspect (the brush keeps it; the window letterboxes around it).
	const float Aspect = M.Brush.IsValid() && M.Brush->ImageSize.Y > 0 ? M.Brush->ImageSize.X / M.Brush->ImageSize.Y : 16.f / 9.f;
	int32 W = FMath::RoundToInt(FMath::Min(Client.X, Client.Y * Aspect));
	if (MaxRenderWidth > 0) W = FMath::Min(W, MaxRenderWidth);
	W = FMath::Clamp(W, 160, 3840);
	const int32 H = FMath::Clamp(FMath::RoundToInt(W / Aspect), 90, 2160);
	if (FMath::Abs(static_cast<int32>(M.Target->SizeX) - W) <= 24)
	{
		return;
	}
	// A NEW render target, not ResizeTarget: Slate keeps drawing a resized target at
	// its old size — the picture cropped or sat in black margins (Rob 2026-10-02).
	UTextureRenderTarget2D* Fresh = NewObject<UTextureRenderTarget2D>(this);
	Fresh->RenderTargetFormat = RTF_RGBA8;
	Fresh->ClearColor = FLinearColor::Black;
	Fresh->InitAutoFormat(W, H);
	Fresh->UpdateResourceImmediate(true);
	M.Target = Fresh;
	if (M.Capture) M.Capture->TextureTarget = Fresh;
	if (M.Brush.IsValid()) M.Brush->SetResourceObject(Fresh);
}

void URexyMonitorSubsystem::SetQuality(int32 MaxWidth, bool bFast)
{
	MaxRenderWidth = MaxWidth > 0 ? FMath::Clamp(MaxWidth, 320, 3840) : 0;
	bFastLighting = bFast;
	for (FRexyMonitor& M : Monitors) FitTarget(M);
	UE_LOG(LogTemp, Display, TEXT("RexySandbox: monitors render %s wide, lighting %s"),
		MaxRenderWidth > 0 ? *FString::FromInt(MaxRenderWidth) : TEXT("full"), bFastLighting ? TEXT("fast") : TEXT("full"));
}

void URexyMonitorSubsystem::Deinitialize()
{
	CloseAll();
	Super::Deinitialize();
}

// ── Remote-Control-callable library ────────────────────────────────────────

static URexyMonitorSubsystem* MonitorsFor(const UObject* Context)
{
	UWorld* World = Context ? Context->GetWorld() : nullptr;
	return World ? World->GetSubsystem<URexyMonitorSubsystem>() : nullptr;
}

void URexyMonitorLibrary::SetMonitorQuality(UObject* WorldContextObject, int32 MaxWidth, bool bFastLighting)
{
	if (URexyMonitorSubsystem* Subsystem = MonitorsFor(WorldContextObject)) Subsystem->SetQuality(MaxWidth, bFastLighting);
}

bool URexyMonitorLibrary::OpenMonitor(ACineCameraActor* Camera, int32 Width, int32 Height, int32 X, int32 Y, const FString& Title)
{
	URexyMonitorSubsystem* Subsystem = MonitorsFor(Camera);
	return Subsystem && Subsystem->Open(Camera, Width, Height, X, Y, Title);
}

bool URexyMonitorLibrary::CloseMonitor(ACineCameraActor* Camera)
{
	URexyMonitorSubsystem* Subsystem = MonitorsFor(Camera);
	return Subsystem && Subsystem->Close(Camera);
}

void URexyMonitorLibrary::CloseAllMonitors(UObject* WorldContextObject)
{
	if (URexyMonitorSubsystem* Subsystem = MonitorsFor(WorldContextObject))
	{
		Subsystem->CloseAll();
	}
}

TArray<FString> URexyMonitorLibrary::GetOpenMonitors(UObject* WorldContextObject)
{
	URexyMonitorSubsystem* Subsystem = MonitorsFor(WorldContextObject);
	return Subsystem ? Subsystem->OpenCameraNames() : TArray<FString>();
}

TArray<FRexyMonitorRect> URexyMonitorLibrary::GetMonitorLayout(UObject* WorldContextObject)
{
	URexyMonitorSubsystem* Subsystem = MonitorsFor(WorldContextObject);
	return Subsystem ? Subsystem->Layout() : TArray<FRexyMonitorRect>();
}

// ── URexySandboxLibrary ──────────────────────────────────────────────────────

static URexySandboxSubsystem* SandboxFor(const UObject* Context)
{
	UWorld* World = Context ? Context->GetWorld() : nullptr;
	return World ? World->GetSubsystem<URexySandboxSubsystem>() : nullptr;
}

void URexySandboxLibrary::FrameAll(UObject* WorldContextObject)
{
	if (URexySandboxSubsystem* S = SandboxFor(WorldContextObject)) S->FrameAll();
}

void URexySandboxLibrary::FrameActor(AActor* Actor)
{
	if (URexySandboxSubsystem* S = SandboxFor(Actor)) S->FrameActor(Actor);
}

void URexySandboxLibrary::SetRigsVisible(UObject* WorldContextObject, bool bVisible)
{
	if (URexySandboxSubsystem* S = SandboxFor(WorldContextObject)) S->SetRigsVisible(bVisible);
}

void URexySandboxLibrary::SetActionOverlayVisible(UObject* WorldContextObject, bool bVisible)
{
	if (URexySandboxSubsystem* S = SandboxFor(WorldContextObject)) S->SetActionOverlayVisible(bVisible);
}

void URexySandboxLibrary::SetRigSmoothing(UObject* WorldContextObject, float Seconds)
{
	if (URexySandboxSubsystem* S = SandboxFor(WorldContextObject)) S->SetRigSmoothing(Seconds);
}

void URexySandboxLibrary::SetLook(float ShutterDegrees, int32 FrameLock)
{
	const float Shutter = FMath::Clamp(ShutterDegrees, 0.f, 360.f);
	const int32 Fps = FMath::Clamp(FrameLock, 0, 240);
	auto Set = [](const TCHAR* Name, float Value)
	{
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name)) V->Set(Value, ECVF_SetByCode);
	};
	Set(TEXT("t.MaxFPS"), static_cast<float>(Fps));
	Set(TEXT("r.VSync"), Fps > 0 ? 1.f : 0.f);
	Set(TEXT("r.MotionBlur.Amount"), Shutter / 360.f);
	Set(TEXT("r.MotionBlur.TargetFPS"), (Shutter > 0.f && Fps > 0) ? static_cast<float>(Fps) : -1.f);
}

void URexySandboxLibrary::SetFastLighting(bool bFast)
{
	struct FSaved { const TCHAR* Name; float Fast; float Original; bool bHave; };
	static FSaved Vars[] = {
		{ TEXT("r.DynamicGlobalIlluminationMethod"), 0.f, 0.f, false },   // none (Lumen = 1)
		{ TEXT("r.ReflectionMethod"),                2.f, 0.f, false },   // screen space (Lumen = 1)
		{ TEXT("r.Shadow.Virtual.Enable"),           0.f, 0.f, false },   // regular shadow maps
	};
	for (FSaved& V : Vars)
	{
		IConsoleVariable* C = IConsoleManager::Get().FindConsoleVariable(V.Name);
		if (!C) continue;
		if (!V.bHave) { V.Original = C->GetFloat(); V.bHave = true; }   // what the level started with
		C->Set(bFast ? V.Fast : V.Original, ECVF_SetByCode);
	}
	UE_LOG(LogTemp, Display, TEXT("RexySandbox: fast lighting %s"), bFast ? TEXT("on") : TEXT("off"));
}

void URexySandboxLibrary::SetRigHidden(ACineCameraActor* Camera, bool bHidden)
{
	if (URexySandboxSubsystem* S = SandboxFor(Camera)) S->SetRigHidden(Camera, bHidden);
}

void URexySandboxLibrary::SetMainViewCamera(UObject* WorldContextObject, ACineCameraActor* Camera)
{
	if (URexySandboxSubsystem* S = SandboxFor(WorldContextObject)) S->SetMainViewCamera(Camera);
}
