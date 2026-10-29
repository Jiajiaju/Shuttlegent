#include "ShuttlegentProjectSettings.h"

namespace ShuttlegentProjectSettingsPrivate
{
	// Settings viewer location: Project Settings > Shuttlegent > General
	inline const TCHAR* ContainerName = TEXT("Project");
	inline const TCHAR* CategoryName = TEXT("Shuttlegent");
	inline const TCHAR* SectionName = TEXT("General");
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
