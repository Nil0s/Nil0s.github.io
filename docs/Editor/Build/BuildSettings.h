#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace NoJob { struct BuildSettings { std::string BuildName = "NoJobGame"; std::vector<std::filesystem::path> Scenes; std::size_t StartupSceneIndex = 0; std::wstring Configuration = L"Debug"; std::filesystem::path StartupScene() const; bool IsValid(std::string& error) const; static BuildSettings Load(const std::filesystem::path& projectRoot); bool Save(const std::filesystem::path& projectRoot) const; }; }
