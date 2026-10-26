#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

/**
 * Editor Preferences > Plugins > Shuttlegent: shows UShuttlegentSettings::DisabledTools as one switch per registered
 * editor tool (built-in, project and sample tools alike), so a tool registered later starts switched on.
 */
class FShuttlegentSettingsCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	//~ Begin IDetailCustomization Interface.
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	//~ End IDetailCustomization Interface.
};
