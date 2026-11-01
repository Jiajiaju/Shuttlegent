#include "Web/FShuttlegentImageFiles.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "ShuttlegentEditorConstants.h"
#include "ShuttlegentLog.h"

namespace FShuttlegentImageFilesPrivate
{
	// 扩展名与 MIME 类型，即运行时接受的四种图片（ThirdParty/deepseek-harness/packages/acp/acp/src/content.ts）
	inline const TCHAR* Extensions[][2] = {
		{ TEXT("png"), TEXT("image/png") },
		{ TEXT("jpg"), TEXT("image/jpeg") },
		{ TEXT("jpeg"), TEXT("image/jpeg") },
		{ TEXT("webp"), TEXT("image/webp") },
		{ TEXT("gif"), TEXT("image/gif") },
	};

	// 文件对话框的筛选串
	inline const TCHAR* DialogFilter = TEXT("Images (*.png;*.jpg;*.jpeg;*.webp;*.gif)|*.png;*.jpg;*.jpeg;*.webp;*.gif");

	// 单个文件的大小上限（字节），与运行时的单图上限一致（ThirdParty/deepseek-harness/packages/attachment/attachment-local/src/index.ts 的 DEFAULT_MAX_IMAGE_BYTES）；页面随后再按尺寸缩小
	inline constexpr int64 MaxFileBytes = 20 * 1024 * 1024;

	const TCHAR* FindMimeType(const FString& Path)
	{
		const FString Extension = FPaths::GetExtension(Path);
		for (const auto& Entry : Extensions)
		{
			if (Extension.Equals(Entry[0], ESearchCase::IgnoreCase)) return Entry[1];
		}
		return nullptr;
	}
}

bool FShuttlegentImageFiles::IsImageFile(const FString& Path)
{
	return FShuttlegentImageFilesPrivate::FindMimeType(Path) != nullptr;
}

TArray<TSharedPtr<FJsonValue>> FShuttlegentImageFiles::Load(const TArray<FString>& Paths)
{
	TArray<TSharedPtr<FJsonValue>> Images;
	for (const FString& Path : Paths)
	{
		const TCHAR* MimeType = FShuttlegentImageFilesPrivate::FindMimeType(Path);
		const int64 Size = IFileManager::Get().FileSize(*Path);
		TArray<uint8> Bytes;
		if (MimeType == nullptr || Size <= 0 || Size > FShuttlegentImageFilesPrivate::MaxFileBytes || !FFileHelper::LoadFileToArray(Bytes, *Path))
		{
			UE_LOG(LogShuttlegent, Warning, TEXT("Shuttlegent panel: skipped image file %s (not an image, unreadable or over %lld bytes)."), *Path, FShuttlegentImageFilesPrivate::MaxFileBytes);
			continue;
		}

		const TSharedRef<FJsonObject> Image = MakeShared<FJsonObject>();
		Image->SetStringField(ShuttlegentEditorWebField::MimeType, MimeType);
		Image->SetStringField(ShuttlegentEditorWebField::Data, FBase64::Encode(Bytes));
		Image->SetStringField(ShuttlegentEditorWebField::Name, FPaths::GetCleanFilename(Path));
		Images.Add(MakeShared<FJsonValueObject>(Image));
	}
	return Images;
}

const TCHAR* FShuttlegentImageFiles::GetDialogFilter()
{
	return FShuttlegentImageFilesPrivate::DialogFilter;
}
