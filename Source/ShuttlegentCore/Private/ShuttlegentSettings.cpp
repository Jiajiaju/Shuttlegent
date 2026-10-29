#include "ShuttlegentSettings.h"

namespace ShuttlegentSettingsPrivate
{
	// Settings viewer location: Editor Preferences > Shuttlegent > General
	inline const TCHAR* ContainerName = TEXT("Editor");
	inline const TCHAR* CategoryName = TEXT("Shuttlegent");
	inline const TCHAR* SectionName = TEXT("General");
}

FName UShuttlegentSettings::GetContainerName() const
{
	return FName(ShuttlegentSettingsPrivate::ContainerName);
}

FName UShuttlegentSettings::GetCategoryName() const
{
	return FName(ShuttlegentSettingsPrivate::CategoryName);
}

FName UShuttlegentSettings::GetSectionName() const
{
	return FName(ShuttlegentSettingsPrivate::SectionName);
}
