#include "Tools/FShuttlegentMemoryTools.h"

#include "FShuttlegentMemoryStore.h"
#include "FShuttlegentToolBase.h"
#include "Tool/FShuttlegentToolArguments.h"
#include "Tool/FShuttlegentToolSchema.h"

namespace FShuttlegentMemoryToolsPrivate
{
	// 工具名称
	inline const TCHAR* RememberName = TEXT("remember");
	inline const TCHAR* ForgetName = TEXT("forget");

	// 参数名称
	inline const TCHAR* ArgText = TEXT("text");
	inline const TCHAR* ArgReplaces = TEXT("replaces");

	/** 记一条长期有效的偏好或约定；不修改编辑器中的任何内容，因此不经写入门。 */
	class FRememberTool : public FShuttlegentToolBase
	{
	public:
		FRememberTool()
		{
			Definition.Name = RememberName;
			Definition.Description = TEXT("Save one lasting fact about how this user wants you to work, so every later conversation follows it. "
				"Save, without asking, when the user tells you to remember something or says how things should always be done, and when the user corrects how you did something in a way that applies beyond this task. "
				"Worth saving: naming rules, default values, things to avoid, the user's role and what they work on, how they want answers. "
				"Not worth saving: details of the current task, anything you can read from the editor (asset paths, current values, what is in the level), secrets. "
				"One fact per call, one short sentence in the user's language. When it changes or contradicts a line you already remember, pass that line as replaces instead of adding a second line. "
				"The user sees a one-line note and can undo it; do not mention saving in your reply unless asked.");
			Definition.bReadOnly = true;
			Definition.InputSchema = FShuttlegentToolSchema()
				.String(ArgText, TEXT("The fact, one short sentence in the user's language."), true)
				.String(ArgReplaces, TEXT("A line you already remember that this fact replaces, copied exactly. Omit to add a new line."))
				.Build();
		}

		virtual FShuttlegentToolResult Run(const TSharedRef<FJsonObject>& Arguments) override
		{
			bool bChanged = false;
			FString Error;
			if (!FShuttlegentMemoryStore::Remember(FShuttlegentToolArguments::GetString(Arguments, ArgText), FShuttlegentToolArguments::GetString(Arguments, ArgReplaces), true, bChanged, Error)) return FShuttlegentToolResult::Error(Error);

			return FShuttlegentToolResult::Ok(bChanged ? TEXT("Remembered.") : TEXT("Already remembered; nothing changed."));
		}
	};

	/** 删除一条已不适用的记忆。 */
	class FForgetTool : public FShuttlegentToolBase
	{
	public:
		FForgetTool()
		{
			Definition.Name = ForgetName;
			Definition.Description = TEXT("Remove a line from your memory of this user when it is wrong or no longer applies, for example when the user asks you to forget it or says it has changed and gives nothing to replace it with. "
				"To change a line, call remember with replaces instead.");
			Definition.bReadOnly = true;
			Definition.InputSchema = FShuttlegentToolSchema()
				.String(ArgText, TEXT("The remembered line to remove, copied exactly."), true)
				.Build();
		}

		virtual FShuttlegentToolResult Run(const TSharedRef<FJsonObject>& Arguments) override
		{
			FString Error;
			if (!FShuttlegentMemoryStore::Forget(FShuttlegentToolArguments::GetString(Arguments, ArgText), true, Error)) return FShuttlegentToolResult::Error(Error);

			return FShuttlegentToolResult::Ok(TEXT("Forgotten."));
		}
	};
}

void FShuttlegentMemoryTools::Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools)
{
	OutTools.Add(MakeShared<FShuttlegentMemoryToolsPrivate::FRememberTool>());
	OutTools.Add(MakeShared<FShuttlegentMemoryToolsPrivate::FForgetTool>());
}
