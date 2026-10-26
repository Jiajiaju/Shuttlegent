#include "ShuttlegentProjectSettings.h"

namespace ShuttlegentProjectSettingsPrivate
{
	// Settings viewer location: Project Settings > Plugins > Shuttlegent
	inline const TCHAR* ContainerName = TEXT("Project");
	inline const TCHAR* CategoryName = TEXT("Plugins");
	inline const TCHAR* SectionName = TEXT("Shuttlegent");
}

FName UShuttlegentProjectSettings::GetContainerName() const
{
	return FName(ShuttlegentProjectSettingsPrivate::ContainerName);
}

FName UShuttlegentProjectSettings::GetCategoryName() const
{
	return FName(ShuttlegentProjectSettingsPrivate::CategoryName);
}

FName UShuttlegentProjectSettings::GetSectionName() const
{
	return FName(ShuttlegentProjectSettingsPrivate::SectionName);
}
