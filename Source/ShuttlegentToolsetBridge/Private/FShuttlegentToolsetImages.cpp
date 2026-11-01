#include "FShuttlegentToolsetImages.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "ShuttlegentLog.h"
#include "Tool/FShuttlegentToolImage.h"

namespace FShuttlegentToolsetImagesPrivate
{
	// FToolsetImage 序列化后的字段名（ToolsetImage.h 的 MimeType、Data），按不区分大小写匹配
	inline const TCHAR* FieldMimeType = TEXT("MimeType");
	inline const TCHAR* FieldData = TEXT("Data");
	inline const TCHAR* ImageMimePrefix = TEXT("image/");
	inline const TCHAR* PngMimeType = TEXT("image/png");

	// 图片模块名
	inline const TCHAR* ImageWrapperModuleName = TEXT("ImageWrapper");

	// 交给模型的图片的最长边（像素）；更大的截图先缩小，节省模型上下文
	inline constexpr int32 MaxSide = 1600;

	// 图片在本机的保存目录（相对项目 Saved 目录），供面板显示；按内容命名
	inline const TCHAR* SaveDir = TEXT("Shuttlegent/Captures");
	inline const TCHAR* PngExtension = TEXT(".png");

	FString FindString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name)
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
		{
			FString Value;
			if (Pair.Key.Equals(Name, ESearchCase::IgnoreCase) && Pair.Value.IsValid() && Pair.Value->TryGetString(Value)) return Value;
		}
		return FString();
	}

	/** 解码为 PNG 并在需要时缩小；失败时返回 false。 */
	bool NormalizePng(const TArray<uint8>& Bytes, TArray<uint8>& OutPng, int32& OutWidth, int32& OutHeight)
	{
		IImageWrapperModule& Module = FModuleManager::LoadModuleChecked<IImageWrapperModule>(ImageWrapperModuleName);
		const EImageFormat Format = Module.DetectImageFormat(Bytes.GetData(), Bytes.Num());
		const TSharedPtr<IImageWrapper> Reader = Format == EImageFormat::Invalid ? nullptr : Module.CreateImageWrapper(Format);
		TArray64<uint8> Raw;
		if (!Reader.IsValid() || !Reader->SetCompressed(Bytes.GetData(), Bytes.Num()) || !Reader->GetRaw(ERGBFormat::BGRA, 8, Raw)) return false;

		const int32 Width = static_cast<int32>(Reader->GetWidth());
		const int32 Height = static_cast<int32>(Reader->GetHeight());
		const float Scale = FMath::Min(1.0f, static_cast<float>(MaxSide) / FMath::Max(Width, Height));
		OutWidth = FMath::Max(1, FMath::RoundToInt32(Width * Scale));
		OutHeight = FMath::Max(1, FMath::RoundToInt32(Height * Scale));
		if (Format == EImageFormat::PNG && OutWidth == Width && OutHeight == Height)
		{
			OutPng = Bytes;
			return true;
		}

		// 原样转 PNG 或缩小后转 PNG
		TArray<FColor> Source;
		Source.SetNumUninitialized(Width * Height);
		FMemory::Memcpy(Source.GetData(), Raw.GetData(), Source.Num() * sizeof(FColor));
		TArray<FColor> Resized;
		if (OutWidth == Width && OutHeight == Height) Resized = MoveTemp(Source);
		else
		{
			Resized.SetNumUninitialized(OutWidth * OutHeight);
			FImageUtils::ImageResize(Width, Height, Source, OutWidth, OutHeight, Resized, false);
		}

		const TSharedPtr<IImageWrapper> Writer = Module.CreateImageWrapper(EImageFormat::PNG);
		if (!Writer.IsValid() || !Writer->SetRaw(Resized.GetData(), Resized.Num() * sizeof(FColor), OutWidth, OutHeight, ERGBFormat::BGRA, 8)) return false;

		const TArray64<uint8> Compressed = Writer->GetCompressed();
		OutPng = TArray<uint8>(Compressed.GetData(), static_cast<int32>(Compressed.Num()));
		return true;
	}

	/** 把一个图片对象换成说明文字；不是图片对象时返回 false。 */
	bool TakeImage(const TSharedPtr<FJsonObject>& Object, TArray<FShuttlegentToolImage>& OutImages, FString& OutNote)
	{
		const FString MimeType = FindString(Object, FieldMimeType);
		const FString Data = FindString(Object, FieldData);
		if (!MimeType.StartsWith(ImageMimePrefix, ESearchCase::IgnoreCase) || Data.IsEmpty()) return false;

		TArray<uint8> Bytes;
		TArray<uint8> Png;
		int32 Width = 0;
		int32 Height = 0;
		if (!FBase64::Decode(Data, Bytes) || !NormalizePng(Bytes, Png, Width, Height))
		{
			OutNote = FString::Printf(TEXT("[image %d could not be decoded: %s]"), OutImages.Num() + 1, *MimeType);
			return true;
		}

		const FString File = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), SaveDir, FMD5::HashBytes(Png.GetData(), Png.Num()) + PngExtension));
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
		if (!IFileManager::Get().FileExists(*File) && !FFileHelper::SaveArrayToFile(Png, *File)) UE_LOG(LogShuttlegent, Warning, TEXT("Saving a toolset image failed: %s"), *File);

		FShuttlegentToolImage& Image = OutImages.AddDefaulted_GetRef();
		Image.MimeType = PngMimeType;
		Image.Data = FBase64::Encode(Png);
		OutNote = FString::Printf(TEXT("[image %d: %s %dx%d, attached below; saved to %s]"), OutImages.Num(), PngMimeType, Width, Height, *File);
		return true;
	}

	/** 递归替换值中的图片对象；有替换时返回 true。 */
	bool ReplaceImages(TSharedPtr<FJsonValue>& Value, TArray<FShuttlegentToolImage>& OutImages)
	{
		if (!Value.IsValid()) return false;

		const TSharedPtr<FJsonObject>* Object = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (Value->TryGetObject(Object))
		{
			FString Note;
			if (TakeImage(*Object, OutImages, Note))
			{
				Value = MakeShared<FJsonValueString>(Note);
				return true;
			}

			bool bReplaced = false;
			for (auto& Pair : (*Object)->Values)
			{
				bReplaced |= ReplaceImages(Pair.Value, OutImages);
			}
			return bReplaced;
		}
		if (Value->TryGetArray(Array))
		{
			TArray<TSharedPtr<FJsonValue>> Items = *Array;
			bool bReplaced = false;
			for (TSharedPtr<FJsonValue>& Item : Items)
			{
				bReplaced |= ReplaceImages(Item, OutImages);
			}
			if (bReplaced) Value = MakeShared<FJsonValueArray>(Items);
			return bReplaced;
		}
		return false;
	}
}

void FShuttlegentToolsetImages::Extract(FString& InOutText, TArray<FShuttlegentToolImage>& OutImages)
{
	TSharedPtr<FJsonValue> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InOutText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !FShuttlegentToolsetImagesPrivate::ReplaceImages(Root, OutImages)) return;

	FString Text;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text);
	if (FJsonSerializer::Serialize(Root, FString(), Writer)) InOutText = Text;
}
