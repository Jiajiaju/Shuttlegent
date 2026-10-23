#include "ShuttlegentStyle.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Mark/EShuttlegentMarkMotion.h"
#include "Mark/EShuttlegentMarkTier.h"
#include "Mark/FShuttlegentMarkSheet.h"
#include "ShuttlegentConstants.h"
#include "ShuttlegentEditorConstants.h"
#include "ShuttlegentLog.h"

namespace ShuttlegentStylePrivate
{
	// Number of scene slots per tier in MarkSheets
	inline constexpr int32 MotionSlots = static_cast<int32>(EShuttlegentMarkMotion::Done) + 1;

	// Indexed by tier * MotionSlots + motion; FrameCount 0 marks a scene the tier does not carry
	TArray<FShuttlegentMarkSheet> MarkSheets;

	int32 SheetIndex(EShuttlegentMarkTier Tier, EShuttlegentMarkMotion Motion)
	{
		return static_cast<int32>(Tier) * MotionSlots + static_cast<int32>(Motion);
	}

	bool ParseTier(const FString& Value, EShuttlegentMarkTier& OutTier)
	{
		if (Value == ShuttlegentEditorMarkAnim::TierLarge) OutTier = EShuttlegentMarkTier::Large;
		else if (Value == ShuttlegentEditorMarkAnim::TierSmall) OutTier = EShuttlegentMarkTier::Small;
		else return false;
		return true;
	}

	bool ParseMotion(const FString& Value, EShuttlegentMarkMotion& OutMotion)
	{
		if (Value == ShuttlegentEditorMarkAnim::MotionIntro) OutMotion = EShuttlegentMarkMotion::Intro;
		else if (Value == ShuttlegentEditorMarkAnim::MotionWorking) OutMotion = EShuttlegentMarkMotion::Working;
		else if (Value == ShuttlegentEditorMarkAnim::MotionHover) OutMotion = EShuttlegentMarkMotion::Hover;
		else if (Value == ShuttlegentEditorMarkAnim::MotionDone) OutMotion = EShuttlegentMarkMotion::Done;
		else return false;
		return true;
	}

	/** Reads Resources/anim/<AnimPalette>/mark-anim.json and registers one brush per sheet in the style set. Missing files leave the mark static. */
	void LoadMarkSheets(const FString& ResourcesDir, FSlateStyleSet& Style)
	{
		const int32 TierCount = static_cast<int32>(EShuttlegentMarkTier::Small) + 1;
		MarkSheets.Reset();
		MarkSheets.SetNum(TierCount * MotionSlots);

		const FString AnimDir = FPaths::Combine(ResourcesDir, ShuttlegentEditorResource::AnimDir, ShuttlegentEditorResource::AnimPalette);
		const FString ManifestPath = FPaths::Combine(AnimDir, ShuttlegentEditorResource::AnimManifest);
		FString ManifestText;
		if (!FFileHelper::LoadFileToString(ManifestText, *ManifestPath))
		{
			UE_LOG(LogShuttlegent, Warning, TEXT("Mark animation manifest not found at %s; the mark stays static."), *ManifestPath);
			return;
		}

		TSharedPtr<FJsonObject> Manifest;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ManifestText);
		if (!FJsonSerializer::Deserialize(Reader, Manifest) || !Manifest.IsValid())
		{
			UE_LOG(LogShuttlegent, Warning, TEXT("Mark animation manifest %s is not valid JSON; the mark stays static."), *ManifestPath);
			return;
		}

		const int32 Fps = Manifest->GetIntegerField(ShuttlegentEditorMarkAnim::FieldFps);
		const float Padding = static_cast<float>(Manifest->GetNumberField(ShuttlegentEditorMarkAnim::FieldPadding));
		const TArray<TSharedPtr<FJsonValue>>* Sheets = nullptr;
		if (Fps <= 0 || !Manifest->TryGetArrayField(ShuttlegentEditorMarkAnim::FieldSheets, Sheets) || Sheets == nullptr)
		{
			UE_LOG(LogShuttlegent, Warning, TEXT("Mark animation manifest %s has no fps or sheets; the mark stays static."), *ManifestPath);
			return;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Sheets)
		{
			const TSharedPtr<FJsonObject> Item = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Item.IsValid()) continue;

			EShuttlegentMarkTier Tier = EShuttlegentMarkTier::Large;
			EShuttlegentMarkMotion Motion = EShuttlegentMarkMotion::None;
			if (!ParseTier(Item->GetStringField(ShuttlegentEditorMarkAnim::FieldTier), Tier) || !ParseMotion(Item->GetStringField(ShuttlegentEditorMarkAnim::FieldMotion), Motion))
			{
				UE_LOG(LogShuttlegent, Warning, TEXT("Skipping mark sheet with unknown tier or motion in %s."), *ManifestPath);
				continue;
			}

			const FString SheetFile = Item->GetStringField(ShuttlegentEditorMarkAnim::FieldFile);
			const FString SheetPath = FPaths::Combine(AnimDir, SheetFile);
			if (!FPaths::FileExists(SheetPath))
			{
				UE_LOG(LogShuttlegent, Warning, TEXT("Mark sheet %s is missing."), *SheetPath);
				continue;
			}

			FShuttlegentMarkSheet& Sheet = MarkSheets[SheetIndex(Tier, Motion)];
			Sheet.FrameSize = FVector2f(static_cast<float>(Item->GetNumberField(ShuttlegentEditorMarkAnim::FieldFrameWidth)), static_cast<float>(Item->GetNumberField(ShuttlegentEditorMarkAnim::FieldFrameHeight)));
			Sheet.SheetSize = FVector2f(static_cast<float>(Item->GetNumberField(ShuttlegentEditorMarkAnim::FieldWidth)), static_cast<float>(Item->GetNumberField(ShuttlegentEditorMarkAnim::FieldHeight)));
			Sheet.FrameCount = Item->GetIntegerField(ShuttlegentEditorMarkAnim::FieldFrameCount);
			Sheet.Columns = Item->GetIntegerField(ShuttlegentEditorMarkAnim::FieldColumns);
			Sheet.bLoop = Item->GetBoolField(ShuttlegentEditorMarkAnim::FieldLoop);
			Sheet.Padding = Padding;
			Sheet.Fps = Fps;
			Sheet.Brush = FSlateImageBrush(SheetPath, FVector2D(Sheet.SheetSize) * ShuttlegentEditorMarkAnim::FrameScale);

			if (Sheet.FrameCount <= 0 || Sheet.Columns <= 0 || Sheet.SheetSize.X <= 0.0f || Sheet.SheetSize.Y <= 0.0f)
			{
				UE_LOG(LogShuttlegent, Warning, TEXT("Mark sheet %s has an invalid frame grid."), *SheetPath);
				Sheet = FShuttlegentMarkSheet();
				continue;
			}

			// The per-frame brush copies share this resource name, so registering here is what loads the texture
			Style.Set(FName(FString(ShuttlegentEditorStyleKey::MarkAnimPrefix) + SheetFile), new FSlateImageBrush(SheetPath, FVector2D(Sheet.SheetSize) * ShuttlegentEditorMarkAnim::FrameScale));
		}
	}

	// Typeface names of the editor's default font
	inline const TCHAR* TypefaceRegular = TEXT("Regular");
	inline const TCHAR* TypefaceBold = TEXT("Bold");
	inline const TCHAR* TypefaceItalic = TEXT("Italic");
	inline const TCHAR* TypefaceMono = TEXT("Mono");

	// Core styles that Markdown text and links start from
	inline const FName CoreHyperlinkStyle = TEXT("Hyperlink");
	inline const FName CoreNormalTextStyle = TEXT("NormalText");

	FLinearColor Linear(const FColor& Token)
	{
		return FLinearColor(Token);
	}

	FSlateRoundedBoxBrush RoundedFill(const FColor& Fill, float Radius)
	{
		return FSlateRoundedBoxBrush(Linear(Fill), Radius);
	}

	FSlateRoundedBoxBrush RoundedOutlined(const FColor& Fill, const FColor& Outline, float Radius)
	{
		return FSlateRoundedBoxBrush(Linear(Fill), Radius, Linear(Outline), ShuttlegentEditorMetric::BorderWidth);
	}

	FButtonStyle MakeButtonStyle(const FColor& Normal, const FColor& Hovered, const FColor& Outline, bool bOutlined)
	{
		const float Radius = ShuttlegentEditorMetric::RadiusControl;
		const FMargin Padding(ShuttlegentEditorMetric::Space3, ShuttlegentEditorMetric::Space1);

		FButtonStyle Style = FAppStyle::Get().GetWidgetStyle<FButtonStyle>(TEXT("Button"));
		Style.SetNormal(bOutlined ? RoundedOutlined(Normal, Outline, Radius) : RoundedFill(Normal, Radius));
		Style.SetHovered(bOutlined ? RoundedOutlined(Hovered, Outline, Radius) : RoundedFill(Hovered, Radius));
		Style.SetPressed(bOutlined ? RoundedOutlined(Normal, Outline, Radius) : RoundedFill(Normal, Radius));
		Style.SetDisabled(RoundedFill(ShuttlegentEditorColor::Raised, Radius));
		Style.SetNormalPadding(Padding);
		Style.SetPressedPadding(Padding);
		return Style;
	}

	FTextBlockStyle MakeTextStyle(const FSlateFontInfo& Font, const FColor& Color)
	{
		return FTextBlockStyle(FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>(CoreNormalTextStyle)).SetFont(Font).SetColorAndOpacity(FSlateColor(Linear(Color)));
	}

	/** Text styles of FShuttlegentMarkdown markup and of SShuttlegentMarkdownView blocks. */
	void RegisterMarkdownStyles(FSlateStyleSet& Style)
	{
		const int32 Body = ShuttlegentEditorMetric::FontBody;
		Style.Set(ShuttlegentEditorStyleKey::MarkdownBody, MakeTextStyle(FCoreStyle::GetDefaultFontStyle(TypefaceRegular, Body), ShuttlegentEditorColor::Text));
		Style.Set(ShuttlegentEditorStyleKey::MarkdownBold, MakeTextStyle(FCoreStyle::GetDefaultFontStyle(TypefaceBold, Body), ShuttlegentEditorColor::Text));
		Style.Set(ShuttlegentEditorStyleKey::MarkdownItalic, MakeTextStyle(FCoreStyle::GetDefaultFontStyle(TypefaceItalic, Body), ShuttlegentEditorColor::Text));
		Style.Set(ShuttlegentEditorStyleKey::MarkdownCode, MakeTextStyle(FCoreStyle::GetDefaultFontStyle(TypefaceMono, Body), ShuttlegentEditorColor::Accent));
		Style.Set(ShuttlegentEditorStyleKey::MarkdownHeading1, MakeTextStyle(FCoreStyle::GetDefaultFontStyle(TypefaceBold, ShuttlegentEditorMetric::FontHeading1), ShuttlegentEditorColor::Text));
		Style.Set(ShuttlegentEditorStyleKey::MarkdownHeading2, MakeTextStyle(FCoreStyle::GetDefaultFontStyle(TypefaceBold, ShuttlegentEditorMetric::FontHeading2), ShuttlegentEditorColor::Text));
		Style.Set(ShuttlegentEditorStyleKey::MarkdownHeading3, MakeTextStyle(FCoreStyle::GetDefaultFontStyle(TypefaceBold, ShuttlegentEditorMetric::FontHeading3), ShuttlegentEditorColor::Text));
		Style.Set(ShuttlegentEditorStyleKey::MarkdownQuote, MakeTextStyle(FCoreStyle::GetDefaultFontStyle(TypefaceRegular, Body), ShuttlegentEditorColor::Muted));
		Style.Set(ShuttlegentEditorStyleKey::MarkdownCodeBlock, MakeTextStyle(FCoreStyle::GetDefaultFontStyle(TypefaceMono, Body), ShuttlegentEditorColor::Text));

		FHyperlinkStyle Link = FCoreStyle::Get().GetWidgetStyle<FHyperlinkStyle>(CoreHyperlinkStyle);
		Link.SetTextStyle(MakeTextStyle(FCoreStyle::GetDefaultFontStyle(TypefaceRegular, Body), ShuttlegentEditorColor::Accent));
		Style.Set(ShuttlegentEditorStyleKey::MarkdownLink, Link);
	}
}

TSharedPtr<FSlateStyleSet> FShuttlegentStyle::StyleSet = nullptr;

void FShuttlegentStyle::Register()
{
	if (StyleSet.IsValid()) return;

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(ShuttlegentPath::PluginName);
	const FString ResourcesDir = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), ShuttlegentEditorResource::ResourcesDir) : FString();
	const FString PngDir = FPaths::Combine(ResourcesDir, ShuttlegentEditorResource::PngDir);

	StyleSet = MakeShared<FSlateStyleSet>(ShuttlegentEditorName::StyleSetName);
	StyleSet->SetContentRoot(ResourcesDir);

	const FVector2D IconSize(ShuttlegentEditorMetric::IconSize, ShuttlegentEditorMetric::IconSize);
	for (const TCHAR* Name : ShuttlegentEditorResource::Icons)
	{
		const FString Path = FPaths::Combine(PngDir, FString(Name) + ShuttlegentEditorResource::PngExtension);
		StyleSet->Set(FName(FString(ShuttlegentEditorStyleKey::IconPrefix) + Name), new FSlateImageBrush(Path, IconSize));
	}

	const FString MarkPath = FPaths::Combine(PngDir, FString(ShuttlegentEditorResource::MarkRedWhite) + ShuttlegentEditorResource::PngExtension);
	const FString TabIconPath = FPaths::Combine(PngDir, FString(ShuttlegentEditorResource::MarkReverse) + ShuttlegentEditorResource::PngExtension);
	const FVector2D MarkSize(ShuttlegentEditorMetric::MarkSizeHeader, ShuttlegentEditorMetric::MarkSizeHeader);
	const FVector2D TabIconSize(ShuttlegentEditorMetric::TabIconSize, ShuttlegentEditorMetric::TabIconSize);
	StyleSet->Set(ShuttlegentEditorStyleKey::Mark, new FSlateImageBrush(MarkPath, MarkSize));
	StyleSet->Set(ShuttlegentEditorStyleKey::TabIcon, new FSlateImageBrush(TabIconPath, TabIconSize));
	ShuttlegentStylePrivate::LoadMarkSheets(ResourcesDir, *StyleSet);

	StyleSet->Set(ShuttlegentEditorStyleKey::Background, new FSlateColorBrush(ShuttlegentStylePrivate::Linear(ShuttlegentEditorColor::Background)));
	StyleSet->Set(ShuttlegentEditorStyleKey::Divider, new FSlateColorBrush(ShuttlegentStylePrivate::Linear(ShuttlegentEditorColor::Border)));
	StyleSet->Set(ShuttlegentEditorStyleKey::Surface, new FSlateRoundedBoxBrush(ShuttlegentStylePrivate::RoundedFill(ShuttlegentEditorColor::Surface, ShuttlegentEditorMetric::RadiusPanel)));
	StyleSet->Set(ShuttlegentEditorStyleKey::Raised, new FSlateRoundedBoxBrush(ShuttlegentStylePrivate::RoundedOutlined(ShuttlegentEditorColor::Raised, ShuttlegentEditorColor::Border, ShuttlegentEditorMetric::RadiusPanel)));
	StyleSet->Set(ShuttlegentEditorStyleKey::Input, new FSlateRoundedBoxBrush(ShuttlegentStylePrivate::RoundedOutlined(ShuttlegentEditorColor::Surface, ShuttlegentEditorColor::Border, ShuttlegentEditorMetric::RadiusPanel)));
	StyleSet->Set(ShuttlegentEditorStyleKey::StatusDot, new FSlateRoundedBoxBrush(FLinearColor::White, ShuttlegentEditorMetric::StatusDotSize * 0.5f, FVector2D(ShuttlegentEditorMetric::StatusDotSize, ShuttlegentEditorMetric::StatusDotSize)));

	StyleSet->Set(ShuttlegentEditorStyleKey::PrimaryButton, ShuttlegentStylePrivate::MakeButtonStyle(ShuttlegentEditorColor::Primary, ShuttlegentEditorColor::PrimaryHovered, ShuttlegentEditorColor::Primary, false));
	StyleSet->Set(ShuttlegentEditorStyleKey::SecondaryButton, ShuttlegentStylePrivate::MakeButtonStyle(ShuttlegentEditorColor::Surface, ShuttlegentEditorColor::Raised, ShuttlegentEditorColor::Border, true));
	StyleSet->Set(ShuttlegentEditorStyleKey::IconButton, ShuttlegentStylePrivate::MakeButtonStyle(ShuttlegentEditorColor::Surface, ShuttlegentEditorColor::Raised, ShuttlegentEditorColor::Surface, false));

	FEditableTextBoxStyle InputBox = FAppStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(TEXT("NormalEditableTextBox"));
	const FSlateColorBrush Transparent(FLinearColor::Transparent);
	InputBox.SetBackgroundImageNormal(Transparent);
	InputBox.SetBackgroundImageHovered(Transparent);
	InputBox.SetBackgroundImageFocused(Transparent);
	InputBox.SetBackgroundImageReadOnly(Transparent);
	InputBox.SetForegroundColor(FSlateColor(ShuttlegentStylePrivate::Linear(ShuttlegentEditorColor::Text)));
	InputBox.SetFont(BodyFont());
	StyleSet->Set(ShuttlegentEditorStyleKey::InputBox, InputBox);
	ShuttlegentStylePrivate::RegisterMarkdownStyles(*StyleSet);

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
}

void FShuttlegentStyle::Unregister()
{
	if (!StyleSet.IsValid()) return;

	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
	StyleSet.Reset();
	ShuttlegentStylePrivate::MarkSheets.Reset();
}

const ISlateStyle& FShuttlegentStyle::Get()
{
	check(StyleSet.IsValid());
	return *StyleSet;
}

const FSlateBrush* FShuttlegentStyle::Icon(const TCHAR* Name)
{
	return Get().GetBrush(FName(FString(ShuttlegentEditorStyleKey::IconPrefix) + Name));
}

const FSlateBrush* FShuttlegentStyle::Brush(const FName& Key)
{
	return Get().GetBrush(Key);
}

const FShuttlegentMarkSheet* FShuttlegentStyle::MarkSheet(EShuttlegentMarkTier Tier, EShuttlegentMarkMotion Motion)
{
	const int32 Index = ShuttlegentStylePrivate::SheetIndex(Tier, Motion);
	if (!ShuttlegentStylePrivate::MarkSheets.IsValidIndex(Index)) return nullptr;

	const FShuttlegentMarkSheet& Sheet = ShuttlegentStylePrivate::MarkSheets[Index];
	return Sheet.FrameCount > 0 ? &Sheet : nullptr;
}

FLinearColor FShuttlegentStyle::Color(const FColor& Token)
{
	return ShuttlegentStylePrivate::Linear(Token);
}

FSlateFontInfo FShuttlegentStyle::BodyFont()
{
	return FCoreStyle::GetDefaultFontStyle(ShuttlegentStylePrivate::TypefaceRegular, ShuttlegentEditorMetric::FontBody);
}

FSlateFontInfo FShuttlegentStyle::CaptionFont()
{
	return FCoreStyle::GetDefaultFontStyle(ShuttlegentStylePrivate::TypefaceRegular, ShuttlegentEditorMetric::FontCaption);
}

FSlateFontInfo FShuttlegentStyle::SectionFont()
{
	return FCoreStyle::GetDefaultFontStyle(ShuttlegentStylePrivate::TypefaceRegular, ShuttlegentEditorMetric::FontSection);
}

FSlateFontInfo FShuttlegentStyle::EmphasisFont()
{
	return FCoreStyle::GetDefaultFontStyle(ShuttlegentStylePrivate::TypefaceBold, ShuttlegentEditorMetric::FontBody);
}
