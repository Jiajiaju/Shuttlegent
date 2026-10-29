#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

/**
 * Editor Preferences > Shuttlegent > General: shows UShuttlegentSettings::DisabledTools as one switch per registered
 * editor tool (built-in, project and sample tools alike), so a tool registered later starts switched on, and
 * DisabledToolsets as one switch per engine toolset; toolsets the project blocks show switched off and locked.
 */
class FShuttlegentSettingsCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	//~ Begin IDetailCustomization Interface.
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	//~ End IDetailCustomization Interface.

private:
	void CustomizeToolsets(IDetailLayoutBuilder& DetailBuilder);
};
