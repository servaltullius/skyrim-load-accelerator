#include "Hooks.h"
#include "Plugin.h"
#include "Settings.h"

static void InitializeLogging() {
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        std::format("Data/SKSE/Plugins/{}.log", Plugin::Name), true);
    auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

    log->set_level(spdlog::level::info);
    log->flush_on(spdlog::level::info);

    spdlog::set_default_logger(std::move(log));
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v"s);
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse) {
    SKSE::Init(a_skse);

    Settings::Load();

    if (Settings::bEnableLogging) {
        InitializeLogging();
    }

    spdlog::info("{} v{}.{}.{} loaded", Plugin::Name,
        Plugin::Version.major(), Plugin::Version.minor(), Plugin::Version.patch());

    Hooks::Install();

    spdlog::info("All hooks installed successfully");

    return true;
}
