// Rexy Sandbox — the action's transport inside the main game window (blocking
// viewport only: never in a camera monitor, never recorded). Play / pause,
// back to start, to the end, and a scrub slider, on a small panel you can drag
// by its title strip and resize from the corner; it stays inside the window and
// remembers where it was. It drives the level's LevelSequenceActor (tag
// RexyAction) directly; Rexy Bridge sees the change on its next poll and
// vMoCo follows.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class ALevelSequenceActor;
class ULevelSequencePlayer;

class SRexyTransportOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRexyTransportOverlay) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UWorld>, World)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	ULevelSequencePlayer* Player() const;
	void SeekFrame(double Frame, bool bScrub);
	double DurationFrames() const;
	double CurrentFrame() const;
	FText TimeText() const;
	FText PlayLabel() const;

	FMargin PanelPadding() const { return FMargin(Pos.X, Pos.Y, 0.f, 0.f); }
	FOptionalSize PanelWidth() const { return Size.X; }
	FOptionalSize PanelHeight() const { return Size.Y; }
	void ClampToViewport();
	void LoadRect();
	void SaveRect() const;

	TWeakObjectPtr<UWorld> World;
	mutable TWeakObjectPtr<ALevelSequenceActor> Action;

	FVector2D Pos = FVector2D(24.f, 24.f);
	FVector2D Size = FVector2D(460.f, 74.f);
	FVector2D ViewSize = FVector2D(1280.f, 720.f);

	enum class EDrag : uint8 { None, Move, Resize };
	EDrag Drag = EDrag::None;
	FVector2D DragStartMouse;
	FVector2D DragStartPos;
	FVector2D DragStartSize;

	bool bScrubbing = false;
	bool bWasPlaying = false;
	float SliderValue = 0.f;
};
