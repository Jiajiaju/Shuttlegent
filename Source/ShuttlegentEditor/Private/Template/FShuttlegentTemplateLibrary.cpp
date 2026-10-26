#include "Template/FShuttlegentTemplateLibrary.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "FShuttlegentProjectConfig.h"
#include "ShuttlegentEditorConstants.h"

TArray<FShuttlegentTemplate> FShuttlegentTemplateLibrary::Load()
{
	TArray<FShuttlegentTemplate> Templates;
	TSet<FString> Names;
	for (const FString& Dir : FShuttlegentProjectConfig::GetTemplateDirs())
	{
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *Dir, ShuttlegentEditorTemplate::FilePattern, true, false);
		Files.Sort();
		for (const FString& File : Files)
		{
			const FString Name = FPaths::GetBaseFilename(File);
			FString Body;
			if (Names.Contains(Name.ToLower()) || !FFileHelper::LoadFileToString(Body, *File) || Body.TrimStartAndEnd().IsEmpty()) continue;

			// The first line, with heading marks and spaces trimmed, says what the template is for
			FString FirstLine;
			Body.TrimStartAndEnd().Split(ShuttlegentEditorTemplate::LineBreak, &FirstLine, nullptr);
			if (FirstLine.IsEmpty()) FirstLine = Body.TrimStartAndEnd();

			FShuttlegentTemplate& Template = Templates.AddDefaulted_GetRef();
			Template.Name = Name;
			Template.Description = FirstLine.TrimStartAndEnd().TrimChar(ShuttlegentEditorTemplate::HeadingMark).TrimStartAndEnd().Left(ShuttlegentEditorTemplate::MaxDescriptionChars);
			Template.Body = Body.TrimStartAndEnd();
			Names.Add(Name.ToLower());
		}
	}
	Templates.Sort([](const FShuttlegentTemplate& Left, const FShuttlegentTemplate& Right) { return Left.Name < Right.Name; });
	return Templates;
}
