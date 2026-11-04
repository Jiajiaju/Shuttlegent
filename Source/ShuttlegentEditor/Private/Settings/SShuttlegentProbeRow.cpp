#include "Settings/SShuttlegentProbeRow.h"

#include "DetailLayoutBuilder.h"
#include "Styling/StyleColors.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "FShuttlegentModelProbe.h"
#include "FShuttlegentModelProbeResult.h"
#include "ShuttlegentConstants.h"
#include "ShuttlegentEditorConstants.h"

#define LOCTEXT_NAMESPACE "ShuttlegentProbeRow"

namespace SShuttlegentProbeRowPrivate
{
	/** 一行结论，每种检测结果一种说法。 */
	FText Conclusion(const FShuttlegentModelProbeResult& Result)
	{
		switch (Result.Outcome)
		{
		case EShuttlegentModelProbeOutcome::Connected:
			{
				return FText::Format(LOCTEXT("Connected", "已连通（{0}，{1} ms）"), FText::FromString(Result.ModelId), FText::AsNumber(Result.ElapsedMs));
			}
		case EShuttlegentModelProbeOutcome::RateLimited:
			{
				return LOCTEXT("RateLimited", "已连通，当前被限流（HTTP 429）");
			}
		case EShuttlegentModelProbeOutcome::KeyInvalid:
			{
				return FText::Format(LOCTEXT("KeyInvalid", "密钥无效或无权限（HTTP {0}）"), FText::AsNumber(Result.StatusCode));
			}
		case EShuttlegentModelProbeOutcome::ProtocolMismatch:
			{
				return FText::Format(LOCTEXT("ProtocolMismatch", "地址与协议不配套（HTTP {0}）：检查地址是否为该协议的接口地址"), FText::AsNumber(Result.StatusCode));
			}
		case EShuttlegentModelProbeOutcome::WrongModel:
			{
				return FText::Format(LOCTEXT("WrongModel", "模型 ID 不对：{0}"), FText::FromString(Result.ModelId));
			}
		case EShuttlegentModelProbeOutcome::HttpError:
			{
				return FText::Format(LOCTEXT("HttpError", "请求失败（HTTP {0}）"), FText::AsNumber(Result.StatusCode));
			}
		case EShuttlegentModelProbeOutcome::MissingKey:
			{
				return LOCTEXT("MissingKey", "未填写密钥，未发请求");
			}
		case EShuttlegentModelProbeOutcome::MissingBaseUrl:
			{
				return LOCTEXT("MissingBaseUrl", "未填写地址，未发请求");
			}
		case EShuttlegentModelProbeOutcome::MissingModel:
			{
				return LOCTEXT("MissingModel", "没有填写模型 ID，未发请求");
			}
		case EShuttlegentModelProbeOutcome::Unreachable:
		default:
			{
				return Result.bTimedOut ? LOCTEXT("TimedOut", "地址不可达：请求超时") : LOCTEXT("Unreachable", "地址不可达：域名无法解析或连接被拒绝");
			}
		}
	}

	bool IsSuccess(EShuttlegentModelProbeOutcome Outcome)
	{
		return Outcome == EShuttlegentModelProbeOutcome::Connected || Outcome == EShuttlegentModelProbeOutcome::RateLimited;
	}

	/** 服务方的错误说明只在它能帮用户定位问题时显示。 */
	bool ShowsDetail(EShuttlegentModelProbeOutcome Outcome)
	{
		return Outcome == EShuttlegentModelProbeOutcome::WrongModel || Outcome == EShuttlegentModelProbeOutcome::HttpError || Outcome == EShuttlegentModelProbeOutcome::KeyInvalid;
	}
}

void SShuttlegentProbeRow::Construct(const FArguments& InArgs)
{
	OnGetTargets = InArgs._OnGetTargets;

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text_Lambda([this]() { return bProbing ? LOCTEXT("Probing", "检测中…") : LOCTEXT("Probe", "检测连接"); })
				.ToolTipText(LOCTEXT("ProbeTooltip", "向模型服务发一次最小请求（输出 1 个 token），确认地址、协议、密钥与模型 ID；不需要运行时已启动"))
				.IsEnabled_Lambda([this]() { return !bProbing; })
				.OnClicked(this, &SShuttlegentProbeRow::HandleClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(ShuttlegentEditorSettingsUi::ProbeButtonGap, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ProbeHint", "按当前填写的值检测，不必先保存"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(ResultBox, SVerticalBox)
		]
	];
}

FReply SShuttlegentProbeRow::HandleClicked()
{
	if (bProbing || !OnGetTargets.IsBound()) return FReply::Handled();

	Pending = OnGetTargets.Execute();
	ResultBox->ClearChildren();
	bProbing = true;
	ProbeNext();
	return FReply::Handled();
}

void SShuttlegentProbeRow::ProbeNext()
{
	if (Pending.IsEmpty())
	{
		bProbing = false;
		return;
	}

	const FShuttlegentModelProbeTarget Target = Pending[0];
	Pending.RemoveAt(0);

	// 设置页可能在请求返回前关闭：控件销毁后回调不再执行
	const TWeakPtr<SShuttlegentProbeRow> WeakSelf = StaticCastWeakPtr<SShuttlegentProbeRow>(AsWeak());
	FShuttlegentModelProbe::Probe(Target, FShuttlegentModelProbe::FOnProbeDone::CreateLambda([WeakSelf](const FShuttlegentModelProbeResult& Result)
	{
		const TSharedPtr<SShuttlegentProbeRow> Self = WeakSelf.Pin();
		if (Self.IsValid()) Self->HandleProbeDone(Result);
	}));
}

void SShuttlegentProbeRow::HandleProbeDone(const FShuttlegentModelProbeResult& Result)
{
	AddResultLines(Result);
	ProbeNext();
}

void SShuttlegentProbeRow::AddResultLines(const FShuttlegentModelProbeResult& Result)
{
	const bool bSuccess = SShuttlegentProbeRowPrivate::IsSuccess(Result.Outcome);
	const FText Label = Result.Label.IsEmpty() ? FText::GetEmpty() : FText::Format(LOCTEXT("LabelPrefix", "{0}："), FText::FromString(Result.Label));

	ResultBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, ShuttlegentEditorSettingsUi::ProbeLineGap, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("ResultLine", "{0}{1}"), Label, SShuttlegentProbeRowPrivate::Conclusion(Result)))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ColorAndOpacity(bSuccess ? FStyleColors::Success : FStyleColors::Error)
			.AutoWrapText(true)
		];

	if (Result.Url.IsEmpty()) return;

	FString Request = FString::Printf(TEXT("%s %s"), ShuttlegentProbeConstant::Verb, *Result.Url);
	if (SShuttlegentProbeRowPrivate::ShowsDetail(Result.Outcome) && !Result.Detail.IsEmpty()) Request += ShuttlegentEditorSettingsUi::ProbeFieldSeparator + Result.Detail;

	ResultBox->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("RequestLine", "实际请求：{0}"), FText::FromString(Request)))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		];
}

#undef LOCTEXT_NAMESPACE
