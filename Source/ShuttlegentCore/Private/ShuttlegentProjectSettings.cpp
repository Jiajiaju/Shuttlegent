#include "ShuttlegentProjectSettings.h"

namespace ShuttlegentProjectSettingsPrivate
{
	// 设置面板中的位置：项目设置 > Shuttlegent > General
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
