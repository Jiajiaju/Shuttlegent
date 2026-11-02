#include "Settings/FShuttlegentSystemFonts.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <dwrite_1.h>
#include "Microsoft/COMPointer.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif // PLATFORM_WINDOWS

#include "ShuttlegentLog.h"

namespace FShuttlegentSystemFontsPrivate
{
	// 首次查询时枚举的字族（排好序）与全部名称（含本地化名，小写），编辑器进程内不再刷新
	TArray<FShuttlegentSystemFont> Fonts;
	TSet<FString> LowerNames;
	bool bEnumerated = false;

#if PLATFORM_WINDOWS
	// 取字族英文名所用的区域名；没有英文名的字族取第一个名字
	inline const TCHAR* EnglishLocale = TEXT("en-us");

	FString ReadName(IDWriteLocalizedStrings* Names, UINT32 NameIndex)
	{
		UINT32 Length = 0;
		if (FAILED(Names->GetStringLength(NameIndex, &Length))) return FString();

		TArray<WCHAR> Buffer;
		Buffer.SetNumZeroed(Length + 1);
		if (FAILED(Names->GetString(NameIndex, Buffer.GetData(), Length + 1))) return FString();
		return FString(Buffer.GetData());
	}

	void Enumerate()
	{
		TComPtr<IDWriteFactory> Factory;
		if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&Factory)))) return;

		TComPtr<IDWriteFontCollection> Collection;
		if (FAILED(Factory->GetSystemFontCollection(&Collection, false))) return;

		const UINT32 FamilyCount = Collection->GetFontFamilyCount();
		for (UINT32 FamilyIndex = 0; FamilyIndex < FamilyCount; ++FamilyIndex)
		{
			TComPtr<IDWriteFontFamily> Family;
			TComPtr<IDWriteLocalizedStrings> Names;
			if (FAILED(Collection->GetFontFamily(FamilyIndex, &Family)) || FAILED(Family->GetFamilyNames(&Names))) continue;

			UINT32 EnglishIndex = 0;
			BOOL bHasEnglish = 0;
			Names->FindLocaleName(EnglishLocale, &EnglishIndex, &bHasEnglish);

			FShuttlegentSystemFont Font;
			Font.Family = ReadName(Names, bHasEnglish ? EnglishIndex : 0);
			if (Font.Family.IsEmpty()) continue;

			const UINT32 NameCount = Names->GetCount();
			for (UINT32 NameIndex = 0; NameIndex < NameCount; ++NameIndex)
			{
				LowerNames.Add(ReadName(Names, NameIndex).ToLower());
			}

			// 同一字族的字体等宽与否一致，看第一个即可
			TComPtr<IDWriteFont> First;
			TComPtr<IDWriteFont1> First1;
			if (SUCCEEDED(Family->GetFont(0, &First)) && SUCCEEDED(First1.FromQueryInterface(__uuidof(IDWriteFont1), First))) Font.bMonospace = First1->IsMonospacedFont() != 0;

			Fonts.Add(MoveTemp(Font));
		}
	}
#endif // PLATFORM_WINDOWS

	void EnsureEnumerated()
	{
		if (bEnumerated) return;

		bEnumerated = true;
#if PLATFORM_WINDOWS
		Enumerate();
#endif // PLATFORM_WINDOWS

		Fonts.Sort([](const FShuttlegentSystemFont& Left, const FShuttlegentSystemFont& Right) { return Left.Family.Compare(Right.Family, ESearchCase::IgnoreCase) < 0; });
		UE_LOG(LogShuttlegent, Log, TEXT("Shuttlegent settings: %d installed font families found."), Fonts.Num());
	}
}

const TArray<FShuttlegentSystemFont>& FShuttlegentSystemFonts::Get()
{
	FShuttlegentSystemFontsPrivate::EnsureEnumerated();
	return FShuttlegentSystemFontsPrivate::Fonts;
}

bool FShuttlegentSystemFonts::IsInstalled(const FString& Family)
{
	FShuttlegentSystemFontsPrivate::EnsureEnumerated();
	return FShuttlegentSystemFontsPrivate::LowerNames.Contains(Family.TrimStartAndEnd().ToLower());
}
