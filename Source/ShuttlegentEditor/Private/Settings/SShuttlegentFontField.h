#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class SEditableTextBox;
class SMenuAnchor;
class ITableRow;
class STableViewBase;

/** 字体下拉列表中的一行：“默认”、分组标题或一个字族。 */
struct FShuttlegentFontItem
{
	/** 字族名；“默认”为空。 */
	FString Family;

	/** 显示的文字。 */
	FText Label;

	/** 分组标题，不可选。 */
	bool bHeader = false;

	/** “默认”：选中时清空设置。 */
	bool bDefault = false;
};

/**
 * 字体设置的单个输入框：点进框里或输入时，下方列出本机字体（默认、推荐、全部），按输入内容包含匹配筛选。
 * 框里可写逗号分隔的多个字体作后备，选中一项只替换最后一段；↑↓ 选择、回车确认、Esc 收起；手输的内容在回车或失去焦点时提交。
 * 填写的字体都没有安装时，框下方给出黄色提示。
 */
class SShuttlegentFontField : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnFontCommitted, const FString& /*Value*/);

	SLATE_BEGIN_ARGS(SShuttlegentFontField) {}
		/** 当前设置值。 */
		SLATE_ATTRIBUTE(FString, Value)

		/** 推荐分组：其中本机已安装的排在“全部字体”之前。 */
		SLATE_ARGUMENT(TArray<FString>, Recommended)

		/** 只列等宽字族（代码字体）。 */
		SLATE_ARGUMENT(bool, bMonospaceOnly)

		/** 列表第一项“默认”的文字，说明留空时用的字体。 */
		SLATE_ARGUMENT(FText, DefaultLabel)

		SLATE_EVENT(FOnFontCommitted, OnCommitted)
	SLATE_END_ARGS()

private:
	TAttribute<FString> Value;
	TArray<FString> Recommended;
	bool bMonospaceOnly = false;
	FText DefaultLabel;
	FOnFontCommitted OnCommitted;

	TSharedPtr<SEditableTextBox> TextBox;
	TSharedPtr<SMenuAnchor> MenuAnchor;
	TSharedPtr<SListView<TSharedPtr<FShuttlegentFontItem>>> ListView;
	TArray<TSharedPtr<FShuttlegentFontItem>> Items;

	/** 从列表中选中一项时置位，避免随后的失焦提交覆盖刚选的值。 */
	bool bApplyingItem = false;

public:
	void Construct(const FArguments& InArgs);

	//~ Begin SWidget Interface.
	virtual FReply OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	//~ End SWidget Interface.

private:
	/** 按框里最后一段文字重建列表；为空时列出默认、推荐与全部字体。 */
	void RebuildItems(const FString& Filter);

	void OpenList(const FString& Filter);
	void CloseList();

	TSharedRef<ITableRow> GenerateRow(TSharedPtr<FShuttlegentFontItem> Item, const TSharedRef<STableViewBase>& OwnerTable) const;
	void HandleItemClicked(TSharedPtr<FShuttlegentFontItem> Item);

	/** 选中的一项写入：默认清空，其余替换最后一段。 */
	void ApplyItem(const TSharedPtr<FShuttlegentFontItem>& Item);

	/** 在可选的行之间移动选中项（跳过分组标题）。 */
	void MoveSelection(int32 Direction);

	void HandleTextChanged(const FText& Text);
	void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitType);
	FReply HandleKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent);

	/** 填写的字体全都没有安装时的提示，否则为空。 */
	FText GetWarning() const;

	/** 框里最后一个逗号之后的文字，去掉首尾空白。 */
	static FString LastSegment(const FString& Text);
};
