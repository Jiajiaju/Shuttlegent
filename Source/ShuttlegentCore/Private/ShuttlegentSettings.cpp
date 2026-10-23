#include "ShuttlegentSettings.h"

namespace ShuttlegentSettingsPrivate
{
	// Settings viewer location: Editor Preferences > Plugins > Shuttlegent
	inline const TCHAR* ContainerName = TEXT("Editor");
	inline const TCHAR* CategoryName = TEXT("Plugins");
	inline const TCHAR* SectionName = TEXT("Shuttlegent");
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
