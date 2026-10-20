#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include "Editor/Build/BuildSettings.h"

namespace NoJob
{
    class StandaloneBuilder
    {
    public:
        using LogFunction = std::function<void(std::string)>;
        using ProcessFunction = std::function<int(
            const std::filesystem::path&,
            const std::vector<std::wstring>&,
            const std::string&)>;

        struct Result
        {
            bool Succeeded = false;
            std::filesystem::path OutputDirectory;
            std::string Error;
        };

        // Performs the complete standalone build/cook/package pipeline synchronously.
        // EditorLayer owns the worker thread so standalone and script compilation keep
        // their existing mutual-exclusion/lifetime behavior.
        static Result Build(
            const std::filesystem::path& projectRoot,
            const std::filesystem::path& cmakeExecutable,
            const BuildSettings& settings,
            const LogFunction& log,
            const ProcessFunction& runProcess);

    private:
        static std::string ReadProjectName(const std::filesystem::path& root);
        static std::filesystem::path FindBuiltRuntimeExecutable(
            const std::filesystem::path& buildRoot,
            const std::wstring& configuration);
        static bool IsRuntimeAssetExtension(const std::filesystem::path& path);
        static bool CopyRuntimeAsset(
            const std::filesystem::path& root,
            const std::filesystem::path& source,
            const std::filesystem::path& outputRoot,
            std::vector<std::filesystem::path>& cooked,
            std::string& error);
        static bool CookRuntimeAssets(
            const std::filesystem::path& root,
            const BuildSettings& settings,
            const std::filesystem::path& outputRoot,
            std::vector<std::filesystem::path>& cooked,
            std::string& error);
        static bool ValidateStandalonePackage(
            const std::filesystem::path& outputRoot,
            const std::string& buildName,
            std::string& error);
    };
}
