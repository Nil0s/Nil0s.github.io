#include "Editor/Build/BuildSettings.h"
#include <fstream>
#include <iomanip>
namespace NoJob {
namespace { std::filesystem::path P(const std::filesystem::path& r){return r/"BuildSettings.nojobbuild";} }
std::filesystem::path BuildSettings::StartupScene() const { return Scenes.empty()||StartupSceneIndex>=Scenes.size()?std::filesystem::path{}:Scenes[StartupSceneIndex]; }
bool BuildSettings::IsValid(std::string& e) const { if(BuildName.empty()){e="Build Name cannot be empty.";return false;} if(Scenes.empty()){e="Add at least one scene to Scenes in Build.";return false;} if(StartupSceneIndex>=Scenes.size()){e="Startup Scene is invalid.";return false;} return true; }
BuildSettings BuildSettings::Load(const std::filesystem::path& r){ BuildSettings s; s.BuildName=r.filename().string().empty()?"NoJobGame":r.filename().string(); std::ifstream i(P(r)); if(!i)return s; std::string h,c; int v=0; size_t n=0; if(!(i>>h>>v)||h!="NOJOB_BUILD_SETTINGS")return s; i>>std::quoted(s.BuildName)>>std::quoted(c)>>s.StartupSceneIndex>>n; s.Configuration=c=="Release"?L"Release":L"Debug"; s.Scenes.clear(); for(size_t x=0;x<n;++x){std::string q;if(i>>std::quoted(q))s.Scenes.emplace_back(q);} if(s.StartupSceneIndex>=s.Scenes.size())s.StartupSceneIndex=0; return s; }
bool BuildSettings::Save(const std::filesystem::path& r) const { std::ofstream o(P(r)); if(!o)return false; o<<"NOJOB_BUILD_SETTINGS 1\n"<<std::quoted(BuildName)<<'\n'<<std::quoted(Configuration==L"Release"?"Release":"Debug")<<'\n'<<StartupSceneIndex<<'\n'<<Scenes.size()<<'\n'; for(auto& s:Scenes)o<<std::quoted(s.generic_string())<<'\n'; return true; }
}
