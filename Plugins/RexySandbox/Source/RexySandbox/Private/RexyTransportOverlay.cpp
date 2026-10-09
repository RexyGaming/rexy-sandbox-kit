#include "RexyTransportOverlay.h"

#include "EngineUtils.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Misc/ConfigCacheIni.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const TCHAR* Section = TEXT("RexySandbox.ActionOverlay");
	const FVector2D MinSize(260.f, 60.f);

	FSlateFontInfo Font(int32 Size) { return FCoreStyle::GetDefaultFontStyle("Regular", Size); }
}

void SRexyTransportOverlay::Construct(const FArguments& InArgs)
{
	World = InArgs._World;
	LoadRect();
	// Full-window layer, but only the panel takes the mouse — the blocking
	// viewport underneath keeps every click and drag.
	SetVisibility(EVisibility::SelfHitTestInvisible);

	auto Btn = [this](const FText& Label, const FText& Tip, TFunction<void()> Do)
	{
		return SNew(SButton)
			.ToolTipText(Tip)
			.ContentPadding(FMargin(8.f, 2.f))
			.OnClicked_Lambda([Do]() { Do(); return FReply::Handled(); })
			[
				SNew(STextBlock).Text(Label).Font(Font(11))
			];
	};

	ChildSlot
	[
		SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)
		+ SOverlay::Slot()
		.HAlign(HAlign_Left).VAlign(VAlign_Top)
		.Padding(TAttribute<FMargin>::CreateSP(this, &SRexyTransportOverlay::PanelPadding))
		[
			SNew(SBox)
			.WidthOverride(TAttribute<FOptionalSize>::CreateSP(this, &SRexyTransportOverlay::PanelWidth))
			.HeightOverride(TAttribute<FOptionalSize>::CreateSP(this, &SRexyTransportOverlay::PanelHeight))
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
					.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.82f))
					.Padding(FMargin(8.f, 4.f, 8.f, 6.f))
					[
						SNew(SVerticalBox)
						// Title strip = drag handle
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SBorder)
							.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
							.Cursor(EMouseCursor::GrabHand)
							.OnMouseButtonDown_Lambda([this](const FGeometry&, const FPointerEvent& E)
							{
								if (E.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
								Drag = EDrag::Move; DragStartMouse = E.GetScreenSpacePosition(); DragStartPos = Pos;
								return FReply::Handled().CaptureMouse(SharedThis(this));
							})
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
								[
									SNew(STextBlock).Text(FText::FromString(TEXT(":: ACTION"))).Font(Font(9))
									.ColorAndOpacity(FLinearColor(0.64f, 0.9f, 0.21f))
								]
								+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right).VAlign(VAlign_Center)
								[
									SNew(STextBlock).Text(this, &SRexyTransportOverlay::TimeText).Font(Font(10))
									.ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.92f))
								]
							]
						]
						// Buttons + scrub
						+ SVerticalBox::Slot().FillHeight(1.f).VAlign(VAlign_Center).Padding(0.f, 4.f, 0.f, 0.f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
							[
								Btn(FText::FromString(TEXT("|<")), FText::FromString(TEXT("Back to the start of the loop")), [this]() { SeekFrame(0.0, false); })
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
							[
								SNew(SButton)
								.ToolTipText(FText::FromString(TEXT("Play / pause the level's action")))
								.ContentPadding(FMargin(10.f, 2.f))
								.OnClicked_Lambda([this]()
								{
									if (ULevelSequencePlayer* P = Player()) { if (P->IsPlaying()) P->Pause(); else P->Play(); }
									return FReply::Handled();
								})
								[
									SNew(STextBlock).Text(this, &SRexyTransportOverlay::PlayLabel).Font(Font(11))
								]
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
							[
								Btn(FText::FromString(TEXT(">|")), FText::FromString(TEXT("Pause on the last frame")), [this]()
								{
									if (ULevelSequencePlayer* P = Player()) P->Pause();
									SeekFrame(FMath::Max(0.0, DurationFrames() - 0.01), true);
								})
							]
							+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
							[
								SNew(SSlider)
								.Value_Lambda([this]() { return SliderValue; })
								.OnMouseCaptureBegin_Lambda([this]()
								{
									bScrubbing = true;
									ULevelSequencePlayer* P = Player();
									bWasPlaying = P && P->IsPlaying();
									if (bWasPlaying) P->Pause();
								})
								.OnMouseCaptureEnd_Lambda([this]()
								{
									bScrubbing = false;
									if (bWasPlaying) if (ULevelSequencePlayer* P = Player()) P->Play();
								})
								.OnValueChanged_Lambda([this](float V)
								{
									SliderValue = V;
									SeekFrame(FMath::Min(V * DurationFrames(), FMath::Max(0.0, DurationFrames() - 0.01)), true);
								})
							]
						]
					]
				]
				// Resize grip
				+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
					.Cursor(EMouseCursor::ResizeSouthEast)
					.Padding(FMargin(4.f, 0.f, 1.f, 0.f))
					.OnMouseButtonDown_Lambda([this](const FGeometry&, const FPointerEvent& E)
					{
						if (E.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
						Drag = EDrag::Resize; DragStartMouse = E.GetScreenSpacePosition(); DragStartSize = Size;
						return FReply::Handled().CaptureMouse(SharedThis(this));
					})
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("//"))).Font(Font(10))
						.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.65f))
					]
				]
			]
		]
	];
}

ULevelSequencePlayer* SRexyTransportOverlay::Player() const
{
	if (!Action.IsValid() && World.IsValid())
	{
		for (TActorIterator<ALevelSequenceActor> It(World.Get()); It; ++It)
		{
			if (It->ActorHasTag(TEXT("RexyAction")) || !Action.IsValid())
			{
				Action = *It;
				if (It->ActorHasTag(TEXT("RexyAction"))) break;
			}
		}
	}
	return Action.IsValid() ? Action->GetSequencePlayer() : nullptr;
}

double SRexyTransportOverlay::DurationFrames() const
{
	const ULevelSequencePlayer* P = Player();
	return P ? P->GetDuration().Time.AsDecimal() : 0.0;
}

double SRexyTransportOverlay::CurrentFrame() const
{
	const ULevelSequencePlayer* P = Player();
	return P ? (P->GetCurrentTime().Time - P->GetStartTime().Time).AsDecimal() : 0.0;
}

void SRexyTransportOverlay::SeekFrame(double Frame, bool bScrub)
{
	if (ULevelSequencePlayer* P = Player())
	{
		FMovieSceneSequencePlaybackParams Params;
		Params.Frame = P->GetStartTime().Time + FFrameTime::FromDecimal(Frame);
		Params.PositionType = EMovieScenePositionType::Frame;
		Params.UpdateMethod = bScrub ? EUpdatePositionMethod::Scrub : EUpdatePositionMethod::Jump;
		P->SetPlaybackPosition(Params);
	}
}

FText SRexyTransportOverlay::TimeText() const
{
	const ULevelSequencePlayer* P = Player();
	if (!P) return FText::FromString(TEXT("no action"));
	const double Fps = P->GetFrameRate().AsDecimal();
	const double Now = CurrentFrame() / FMath::Max(1.0, Fps), Len = DurationFrames() / FMath::Max(1.0, Fps);
	return FText::FromString(FString::Printf(TEXT("%.1f / %.1f s"), Now, Len));
}

FText SRexyTransportOverlay::PlayLabel() const
{
	const ULevelSequencePlayer* P = Player();
	return FText::FromString(P && P->IsPlaying() ? TEXT("PAUSE") : TEXT("PLAY"));
}

void SRexyTransportOverlay::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	ViewSize = AllottedGeometry.GetLocalSize();
	if (!bScrubbing)
	{
		const double Len = DurationFrames();
		SliderValue = Len > 0.0 ? float(FMath::Clamp(CurrentFrame() / Len, 0.0, 1.0)) : 0.f;
	}
	if (Drag == EDrag::None) ClampToViewport();     // the window may have been resized
}

FReply SRexyTransportOverlay::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (Drag == EDrag::None || !HasMouseCapture()) return FReply::Unhandled();
	const float Scale = MyGeometry.Scale > 0.f ? MyGeometry.Scale : 1.f;           // screen px -> Slate units
	const FVector2D Delta = (MouseEvent.GetScreenSpacePosition() - DragStartMouse) / Scale;
	if (Drag == EDrag::Move) Pos = DragStartPos + Delta;
	else Size = FVector2D(FMath::Max(MinSize.X, DragStartSize.X + Delta.X), FMath::Max(MinSize.Y, DragStartSize.Y + Delta.Y));
	ClampToViewport();
	return FReply::Handled();
}

FReply SRexyTransportOverlay::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (Drag == EDrag::None) return FReply::Unhandled();
	Drag = EDrag::None;
	SaveRect();
	return FReply::Handled().ReleaseMouseCapture();
}

void SRexyTransportOverlay::ClampToViewport()
{
	Size.X = FMath::Clamp(Size.X, MinSize.X, FMath::Max(MinSize.X, ViewSize.X));
	Size.Y = FMath::Clamp(Size.Y, MinSize.Y, FMath::Max(MinSize.Y, ViewSize.Y));
	Pos.X = FMath::Clamp(Pos.X, 0.f, FMath::Max(0.f, float(ViewSize.X - Size.X)));
	Pos.Y = FMath::Clamp(Pos.Y, 0.f, FMath::Max(0.f, float(ViewSize.Y - Size.Y)));
}

void SRexyTransportOverlay::LoadRect()
{
	if (!GConfig) return;
	float V = 0.f;
	if (GConfig->GetFloat(Section, TEXT("X"), V, GGameUserSettingsIni)) Pos.X = V;
	if (GConfig->GetFloat(Section, TEXT("Y"), V, GGameUserSettingsIni)) Pos.Y = V;
	if (GConfig->GetFloat(Section, TEXT("W"), V, GGameUserSettingsIni)) Size.X = V;
	if (GConfig->GetFloat(Section, TEXT("H"), V, GGameUserSettingsIni)) Size.Y = V;
}

void SRexyTransportOverlay::SaveRect() const
{
	if (!GConfig) return;
	GConfig->SetFloat(Section, TEXT("X"), Pos.X, GGameUserSettingsIni);
	GConfig->SetFloat(Section, TEXT("Y"), Pos.Y, GGameUserSettingsIni);
	GConfig->SetFloat(Section, TEXT("W"), Size.X, GGameUserSettingsIni);
	GConfig->SetFloat(Section, TEXT("H"), Size.Y, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}
