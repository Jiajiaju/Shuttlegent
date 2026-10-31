#include "ShuttlegentSettings.h"

namespace ShuttlegentSettingsPrivate
{
	// 设置面板中的位置：编辑器偏好设置 > Shuttlegent > General
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
