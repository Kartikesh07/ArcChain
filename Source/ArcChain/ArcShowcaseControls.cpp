#include "ArcShowcaseControls.h"

#include "ArcChainManager.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"

namespace
{
	const FLinearColor Cyan(0.1f, 0.85f, 1.0f);
	const FLinearColor Muted(0.45f, 0.6f, 0.68f);
	const FLinearColor ControlBackground(0.025f, 0.065f, 0.09f, 0.95f);
}

void UArcShowcaseControls::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;
	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>();
	Width->SetWidthOverride(250.0f);
	UCanvasPanelSlot* Placement = Canvas->AddChildToCanvas(Width);
	Placement->SetPosition(FVector2D(16.0f, 16.0f));
	Placement->SetAutoSize(true);
	Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(0.006f, 0.015f, 0.025f, 0.92f));
	Panel->SetPadding(FMargin(14.0f));
	Width->SetContent(Panel);
	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Content);

	auto MakeText = [this](const FString& Text, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(FText::FromString(Text));
		Label->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", Size));
		Label->SetColorAndOpacity(FSlateColor(Color));
		return Label;
	};
	auto AddLine = [this, Content]()
	{
		USizeBox* Height = WidgetTree->ConstructWidget<USizeBox>();
		Height->SetHeightOverride(2.0f);
		UBorder* Line = WidgetTree->ConstructWidget<UBorder>();
		Line->SetBrushColor(Cyan);
		Height->SetContent(Line);
		Content->AddChildToVerticalBox(Height)->SetPadding(FMargin(0, 8));
	};
	auto AddButton = [this, Content, &MakeText](const FString& Label)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>();
		FButtonStyle Style = Button->GetStyle();
		Style.SetNormal(FSlateColorBrush(ControlBackground));
		Style.SetHovered(FSlateColorBrush(FLinearColor(0.035f, 0.15f, 0.2f)));
		Style.SetPressed(FSlateColorBrush(FLinearColor(0.05f, 0.25f, 0.3f)));
		Button->SetStyle(Style);
		Button->SetContent(MakeText(Label, 12, Cyan));
		USizeBox* Height = WidgetTree->ConstructWidget<USizeBox>();
		Height->SetHeightOverride(32.0f);
		Height->SetContent(Button);
		Content->AddChildToVerticalBox(Height)->SetPadding(FMargin(0, 4));
		return Button;
	};
	auto AddNumber = [this, Content, &MakeText](const FString& Label, float Minimum,
		float Maximum, float Delta, int32 Digits)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(MakeText(Label, 11, Muted));
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		USpinBox* Number = WidgetTree->ConstructWidget<USpinBox>();
		Number->SetMinValue(Minimum);
		Number->SetMaxValue(Maximum);
		Number->SetMinSliderValue(Minimum);
		Number->SetMaxSliderValue(Maximum);
		Number->SetDelta(Delta);
		Number->SetMinFractionalDigits(Digits);
		Number->SetMaxFractionalDigits(Digits);
		Number->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 12));
		FSpinBoxStyle Style = Number->GetWidgetStyle();
		Style.SetBackgroundBrush(FSlateColorBrush(ControlBackground));
		Style.SetActiveBackgroundBrush(FSlateColorBrush(FLinearColor(0.035f, 0.12f, 0.17f)));
		Style.SetInactiveFillBrush(FSlateColorBrush(FLinearColor(0.025f, 0.085f, 0.11f)));
		Style.SetActiveFillBrush(FSlateColorBrush(FLinearColor(0.04f, 0.18f, 0.23f)));
		Number->SetWidgetStyle(Style);
		Number->SetForegroundColor(FSlateColor(Cyan));
		Number->OnValueChanged.AddDynamic(this, &UArcShowcaseControls::HandleSettingChanged);
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(85.0f);
		Size->SetHeightOverride(28.0f);
		Size->SetContent(Number);
		Row->AddChildToHorizontalBox(Size);
		Content->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 4));
		return Number;
	};

	Content->AddChildToVerticalBox(MakeText(TEXT("ARCCHAIN"), 19, Cyan));
	Content->AddChildToVerticalBox(MakeText(TEXT("SHOWCASE CONTROLS"), 9, Muted));
	AddLine();
	UButton* Mode = AddButton(TEXT("MODE  /  BRANCHING"));
	ModeText = Cast<UTextBlock>(Mode->GetContent());
	Mode->OnClicked.AddDynamic(this, &UArcShowcaseControls::HandleToggleMode);
	BranchesInput = AddNumber(TEXT("Branches / node"), 1, 4, 1, 0);
	RadiusInput = AddNumber(TEXT("Radius (cm)"), 50, 3000, 25, 0);
	IntervalInput = AddNumber(TEXT("Interval (s)"), 0.05f, 2, 0.05f, 2);
	AddLine();
	StatusText = MakeText(TEXT("STATUS  /  IDLE"), 12, Cyan);
	Content->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0, 0, 0, 5));
	StrikesText = MakeText(TEXT("Strikes completed: 0"), 11, Muted);
	Content->AddChildToVerticalBox(StrikesText)->SetPadding(FMargin(0, 0, 0, 7));
	AddButton(TEXT("RESET"))->OnClicked.AddDynamic(this, &UArcShowcaseControls::HandleReset);
	UTextBlock* Hint = MakeText(TEXT("Changes apply on the next cube click."), 9, Muted);
	Hint->SetAutoWrapText(true);
	Content->AddChildToVerticalBox(Hint)->SetPadding(FMargin(0, 6, 0, 0));
}

void UArcShowcaseControls::SetChainManager(AArcChainManager* Manager)
{
	if (AArcChainManager* Previous = ChainManager.Get())
	{
		Previous->OnStateChanged.RemoveAll(this);
	}
	ChainManager = Manager;
	if (IsValid(Manager) && !Manager->IsActorBeingDestroyed())
	{
		Manager->OnStateChanged.AddUObject(this, &UArcShowcaseControls::RefreshStatus);
	}
	RefreshPendingSettings();
	RefreshStatus();
}

void UArcShowcaseControls::RefreshPendingSettings()
{
	AArcChainManager* Manager = ChainManager.Get();
	if (!IsValid(Manager) || Manager->IsActorBeingDestroyed())
	{
		return;
	}
	TGuardValue<bool> Guard(bUpdatingControls, true);
	bNextBranching = Manager->IsBranchingEnabled();
	ModeText->SetText(FText::FromString(bNextBranching ? TEXT("MODE  /  BRANCHING") : TEXT("MODE  /  LINEAR")));
	BranchesInput->SetValue(Manager->GetMaxBranchesPerNode());
	BranchesInput->SetIsEnabled(bNextBranching);
	RadiusInput->SetValue(Manager->GetJumpRadius());
	IntervalInput->SetValue(Manager->GetJumpInterval());
}

void UArcShowcaseControls::RefreshStatus()
{
	AArcChainManager* Manager = ChainManager.Get();
	const bool bAvailable = IsValid(Manager) && !Manager->IsActorBeingDestroyed();
	Panel->SetIsEnabled(bAvailable);
	const EArcPropagationStatus Status = bAvailable ? Manager->GetPropagationStatus() : EArcPropagationStatus::Idle;
	const TCHAR* Name = Status == EArcPropagationStatus::Active ? TEXT("ACTIVE")
		: Status == EArcPropagationStatus::Complete ? TEXT("COMPLETE") : TEXT("IDLE");
	StatusText->SetText(FText::FromString(FString::Printf(TEXT("STATUS  /  %s"), Name)));
	StrikesText->SetText(FText::FromString(FString::Printf(TEXT("Strikes completed: %d"),
		bAvailable ? Manager->GetCompletedStrikes() : 0)));
}

void UArcShowcaseControls::HandleToggleMode()
{
	bNextBranching = !bNextBranching;
	ApplyPendingSettings();
}

void UArcShowcaseControls::HandleSettingChanged(float Value)
{
	if (!bUpdatingControls)
	{
		ApplyPendingSettings();
	}
}

void UArcShowcaseControls::ApplyPendingSettings()
{
	if (AArcChainManager* Manager = ChainManager.Get(); IsValid(Manager) && !Manager->IsActorBeingDestroyed())
	{
		Manager->ConfigureNextChain(bNextBranching, FMath::RoundToInt(BranchesInput->GetValue()),
			RadiusInput->GetValue(), IntervalInput->GetValue());
		RefreshPendingSettings();
	}
}

void UArcShowcaseControls::HandleReset()
{
	if (AArcChainManager* Manager = ChainManager.Get(); IsValid(Manager) && !Manager->IsActorBeingDestroyed())
	{
		Manager->ResetChain();
	}
}

bool UArcShowcaseControls::IsPointerOverPanel() const
{
	return Panel && FSlateApplication::IsInitialized()
		&& Panel->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos());
}

FReply UArcShowcaseControls::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	return Panel && Panel->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition())
		? FReply::Handled() : Super::NativeOnMouseButtonDown(Geometry, Event);
}

FReply UArcShowcaseControls::NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	return Panel && Panel->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition())
		? FReply::Handled() : Super::NativeOnMouseButtonUp(Geometry, Event);
}

void UArcShowcaseControls::NativeDestruct()
{
	if (AArcChainManager* Manager = ChainManager.Get())
	{
		Manager->OnStateChanged.RemoveAll(this);
	}
	Super::NativeDestruct();
}
