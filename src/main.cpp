
#include <filesystem>
#include <whereami.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/file_sinks.h>
#include "cfg/WritableConfig.hpp"

// TODO: move to Report class
void registerSiegeLogger(const std::string& path, const std::string& name);

int main(int argc, char * argv[])
{
    using namespace ehb;

    // always initialize the console logger first
    auto log = spdlog::stdout_color_mt("log");

    /*
     * this reads the following configs (order is important):
     * argsConfig
     * iniConfig
     * registryConfig
     * steamConfig
     * userConfig
    */
    WritableConfig config(argc, argv);
    config.dump();

    registerSiegeLogger(config.getString("logs_path", ""), "filesystem");

    return 0;
}

void registerSiegeLogger(const std::string& path, const std::string& name)
{
    const int length = wai_getExecutablePath(nullptr, 0, nullptr);
    std::string exe(length, '\0');
    wai_getExecutablePath(exe.data(), length, nullptr);

    char dateTime[128] = { '\0' };
    std::time_t now = std::time(nullptr);
    strftime(dateTime, sizeof(dateTime), "%m/%d/%Y %I:%M:%S %p", std::localtime(&now));

    std::filesystem::path fullpath(path);
    fullpath = fullpath / std::string(name + ".log");

    try
    {
        auto log = spdlog::basic_logger_mt(name, fullpath.string(), true);

        log->info("-==--==--==--==--==--==--==--==--==--==--==--==--==--==--==--==--==--==--==-");
        log->info("-== App          : Open Siege ({} - Retail)", exe); // Platform::getExecutablePath()
        log->info("-== Log category : {}", log->name());
        log->info("-== Session      : {}", dateTime);
        log->info("-== Build        : {} (1.11.1.1486 (msqa:2003.07.0202))");
        log->info("-==--==--==--==--==--==--==--==--==--==--==--==--==--==--==--==--==--==--==-");

        // TODO: assign the console sink to the file loggers for verbose console output
    }
    catch (std::exception& e)
    {
        spdlog::get("log")->error("could not create {} logger: {}", name, e.what());
    }
}
