#include "Editor/Build/StandaloneBuilder.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>

namespace NoJob
{
    std::string StandaloneBuilder::ReadProjectName(const std::filesystem::path& root)
    {
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(root, ec))
        {
            if (entry.path().extension() != ".nojobproject") continue;
            std::ifstream in(entry.path());
            std::string header;
            int version = 0;
            std::string name;
            if ((in >> header >> version) && header == "NOJOB_PROJECT" &&
                (in >> std::quoted(name)) && !name.empty())
                return name;
        }
        return root.filename().string().empty() ? std::string("NoJobGame") : root.filename().string();
    }

    std::filesystem::path StandaloneBuilder::FindBuiltRuntimeExecutable(
        const std::filesystem::path& buildRoot, const std::wstring& configuration)
    {
        const std::vector<std::filesystem::path> candidates = {
            buildRoot / configuration / "NoJobRuntime.exe",
            buildRoot / "NoJobRuntime.exe",
            buildRoot / "Runtime" / configuration / "NoJobRuntime.exe"
        };
        for (const auto& candidate : candidates)
            if (std::filesystem::exists(candidate)) return candidate;

        std::error_code ec;
        for (auto it = std::filesystem::recursive_directory_iterator(
                 buildRoot, std::filesystem::directory_options::skip_permission_denied, ec);
             !ec && it != std::filesystem::recursive_directory_iterator(); ++it)
            if (it->is_regular_file(ec) && it->path().filename() == "NoJobRuntime.exe")
                return it->path();
        return {};
    }

    bool StandaloneBuilder::IsRuntimeAssetExtension(const std::filesystem::path& path)
    {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        static const std::vector<std::string> extensions = {
            ".nojobscene", ".nojobmat", ".nojobprefab",
            ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".hdr",
            ".obj", ".fbx", ".gltf", ".glb", ".dae", ".stl", ".ply", ".3ds", ".blend",
            ".wav", ".mp3", ".flac"
        };
        return std::find(extensions.begin(), extensions.end(), ext) != extensions.end();
    }

    bool StandaloneBuilder::CopyRuntimeAsset(
        const std::filesystem::path& root, const std::filesystem::path& source,
        const std::filesystem::path& outputRoot,
        std::vector<std::filesystem::path>& cooked, std::string& error)
    {
        std::error_code ec;
        if (!std::filesystem::exists(source, ec) || !std::filesystem::is_regular_file(source, ec))
            return true;

        auto relative = std::filesystem::relative(source, root, ec);
        if (ec || relative.empty() || relative.native().find(L"..") == 0)
            return true;

        const auto destination = outputRoot / relative;
        std::filesystem::create_directories(destination.parent_path(), ec);
        if (ec) { error = ec.message(); return false; }
        std::filesystem::copy_file(source, destination,
            std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) { error = ec.message(); return false; }
        cooked.push_back(relative);
        return true;
    }

    bool StandaloneBuilder::CookRuntimeAssets(
        const std::filesystem::path& root, const BuildSettings& settings,
        const std::filesystem::path& outputRoot,
        std::vector<std::filesystem::path>& cooked, std::string& error)
    {
        std::error_code ec;
        for (const auto& sceneRelative : settings.Scenes)
        {
            const auto scenePath = sceneRelative.is_absolute() ? sceneRelative : root / sceneRelative;
            if (!std::filesystem::exists(scenePath)) { error = "Scene does not exist: " + scenePath.string(); return false; }
            if (!CopyRuntimeAsset(root, scenePath, outputRoot, cooked, error)) return false;
            std::ifstream scene(scenePath);
            std::string contents((std::istreambuf_iterator<char>(scene)), std::istreambuf_iterator<char>()), token;
            std::vector<std::filesystem::path> candidates;
            auto flush=[&](){ if(token.empty())return; for(char& c:token)if(c=='\\')c='/'; std::filesystem::path q(token); if(IsRuntimeAssetExtension(q))candidates.push_back(q); token.clear(); };
            bool quoted=false;
            for(char c:contents){ if(c=='"'){quoted=!quoted;flush();continue;} if(quoted)token+=c; else if(std::isalnum((unsigned char)c)||c=='/'||c=='\\'||c=='.'||c=='_'||c=='-'||c==':')token+=c; else flush(); } flush();
            for(const auto& candidate:candidates){ auto source=candidate.is_absolute()?candidate:root/candidate; if(!CopyRuntimeAsset(root,source,outputRoot,cooked,error))return false; std::string ext=source.extension().string(); std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return(char)std::tolower(c);}); if(ext==".obj"||ext==".gltf"||ext==".dae"){ for(const auto& sibling:std::filesystem::directory_iterator(source.parent_path(),ec)){ if(ec)break; if(!sibling.is_regular_file(ec))continue; auto se=sibling.path().extension().string(); if(IsRuntimeAssetExtension(sibling.path())||se==".mtl"||se==".bin") if(!CopyRuntimeAsset(root,sibling.path(),outputRoot,cooked,error))return false; } } }
        }
        std::sort(cooked.begin(),cooked.end()); cooked.erase(std::unique(cooked.begin(),cooked.end()),cooked.end());
        std::ofstream manifest(outputRoot/"NoJobRuntimeAssets.manifest"); manifest<<"NOJOB_RUNTIME_ASSETS 2\n"; for(auto& q:cooked)manifest<<q.generic_string()<<'\n';
        std::ofstream config(outputRoot/"NoJobBuildConfig.nojobbuild"); config<<"NOJOB_BUILD_CONFIG 1\n"<<std::quoted(settings.BuildName)<<'\n'<<std::quoted(settings.StartupScene().generic_string())<<'\n'<<settings.Scenes.size()<<'\n'; for(auto& q:settings.Scenes)config<<std::quoted(q.generic_string())<<'\n';
        return true;
    }

    bool StandaloneBuilder::ValidateStandalonePackage(
        const std::filesystem::path& outputRoot, const std::string& buildName,
        std::string& error)
    {
        const std::vector<std::filesystem::path> required = {
            outputRoot / (buildName + ".exe"),
            outputRoot / "NoJobRuntimeAssets.manifest",
            outputRoot / "Assets"
        };
        for (const auto& path : required)
            if (!std::filesystem::exists(path))
            {
                error = "Missing runtime package item: " + path.string();
                return false;
            }

        const std::vector<std::filesystem::path> forbiddenDirectories = {
            outputRoot / "out", outputRoot / "Editor", outputRoot / "Engine",
            outputRoot / ".git", outputRoot / ".vs"
        };
        for (const auto& path : forbiddenDirectories)
            if (std::filesystem::exists(path))
            {
                error = "Development directory leaked into package: " + path.string();
                return false;
            }

        std::error_code ec;
        bool hasProjectFile = false;
        for (auto it = std::filesystem::recursive_directory_iterator(
                 outputRoot, std::filesystem::directory_options::skip_permission_denied, ec);
             !ec && it != std::filesystem::recursive_directory_iterator(); ++it)
        {
            if (!it->is_regular_file(ec)) continue;
            const auto ext = it->path().extension().string();
            if (ext == ".nojobproject") hasProjectFile = true;
            if (ext == ".cpp" || ext == ".h" || ext == ".hpp" ||
                ext == ".pdb" || ext == ".ilk" || ext == ".obj")
            {
                error = "Development/source file leaked into package: " + it->path().string();
                return false;
            }
        }
        if (!hasProjectFile)
        {
            error = "No .nojobproject file found in package.";
            return false;
        }
        return true;
    }

    StandaloneBuilder::Result StandaloneBuilder::Build(
        const std::filesystem::path& root,
        const std::filesystem::path& cmake,
        const BuildSettings& settings,
        const LogFunction& log,
        const ProcessFunction& runProcess)
    {
        Result result;
        const std::wstring configuration = settings.Configuration;
        const std::string projectName = ReadProjectName(root);
        const std::string buildName = settings.BuildName;
        const auto buildRoot = root / "out" / "StandaloneBuild";
        const auto outputRoot = root / "Builds" / buildName;
        auto fail = [&](std::string message)
        {
            result.Error = std::move(message);
            log("[Build] " + result.Error);
            return result;
        };

        log("[Build] Building standalone " + std::string(configuration.begin(), configuration.end()) + "...");

        if (runProcess(cmake, {L"-S", root.wstring(), L"-B", buildRoot.wstring()}, "[Build/CMake] ") != 0)
            return fail("CMake configure failed.");

        if (runProcess(cmake, {L"--build", buildRoot.wstring(), L"--config", configuration,
                              L"--target", L"NoJobRuntime"}, "[Build/CMake] ") != 0)
            return fail("NoJobRuntime build failed.");

        if (runProcess(cmake, {L"--build", buildRoot.wstring(), L"--config", configuration,
                              L"--target", L"NoJobProjectScripts"}, "[Build/Scripts] ") != 0)
            return fail("ProjectScripts " + std::string(configuration.begin(), configuration.end()) + " build failed.");

        const auto runtimeExe = FindBuiltRuntimeExecutable(buildRoot, configuration);
        if (runtimeExe.empty()) return fail("NoJobRuntime.exe was not found after build.");

        std::error_code ec;
        if (std::filesystem::exists(outputRoot, ec))
        {
            ec.clear();
            std::filesystem::remove_all(outputRoot, ec);
            if (ec)
                return fail("Could not clean previous output: " + ec.message() +
                            ". Close the previous game EXE/Explorer handles and retry.");
        }

        std::filesystem::create_directories(outputRoot, ec);
        if (ec) return fail("Could not create output: " + ec.message());

        std::filesystem::copy_file(runtimeExe, outputRoot / (buildName + ".exe"),
            std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) return fail("Could not copy runtime executable: " + ec.message());

        std::string copyError;
        std::vector<std::filesystem::path> cookedAssets;
        if (!CookRuntimeAssets(root, settings, outputRoot, cookedAssets, copyError))
            return fail("Asset cooking failed: " + copyError);
        log("[Build] Cooked " + std::to_string(cookedAssets.size()) + " runtime asset(s).");

        for (const auto& entry : std::filesystem::directory_iterator(root, ec))
        {
            if (entry.path().extension() != ".nojobproject") continue;
            std::filesystem::copy_file(entry.path(), outputRoot / entry.path().filename(),
                std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) return fail("Project config copy failed: " + ec.message());
            break;
        }

        const auto scriptsDestination = outputRoot / "RuntimeData" / "ProjectScripts";
        std::filesystem::create_directories(scriptsDestination, ec);
        if (ec) return fail("ProjectScripts output failed: " + ec.message());

        const bool releaseBuild = configuration == L"Release";
        std::filesystem::path selectedDll;
        std::filesystem::file_time_type selectedTime{};
        for (auto it = std::filesystem::recursive_directory_iterator(
                 buildRoot, std::filesystem::directory_options::skip_permission_denied, ec);
             !ec && it != std::filesystem::recursive_directory_iterator(); ++it)
        {
            if (!it->is_regular_file(ec) || it->path().extension() != ".dll") continue;
            const std::string stem = it->path().stem().string();
            if (stem.rfind("NoJobProjectScripts_", 0) != 0) continue;
            const bool debugDll = !stem.empty() && stem.back() == 'd';
            if (releaseBuild == debugDll) continue;
            const auto time = std::filesystem::last_write_time(it->path(), ec);
            if (ec) { ec.clear(); continue; }
            if (selectedDll.empty() || time > selectedTime)
            {
                selectedDll = it->path();
                selectedTime = time;
            }
        }

        if (!selectedDll.empty())
        {
            std::filesystem::copy_file(selectedDll, scriptsDestination / selectedDll.filename(),
                std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) return fail("ProjectScripts DLL copy failed: " + ec.message());
            log("[Build] Packaged ProjectScripts: " + selectedDll.filename().string());
        }
        else
        {
            log("[Build] No matching " + std::string(releaseBuild ? "Release" : "Debug") +
                " ProjectScripts DLL found; package will contain no project scripts.");
        }

        std::string validationError;
        if (!ValidateStandalonePackage(outputRoot, buildName, validationError))
            return fail("Package validation FAILED: " + validationError);

        result.Succeeded = true;
        result.OutputDirectory = outputRoot;
        log("[Build] Package validation passed.");
        log("[Build] Package layout: Assets + RuntimeData + manifest (no development out folder).");
        log("[Build] Standalone build completed: " + outputRoot.string());
        return result;
    }
}
