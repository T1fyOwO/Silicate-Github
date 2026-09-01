#include "bot.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <asp/fs/fs.hpp>
#include <filesystem>

#include "assist/autoclicker.hpp"
#include "assist/cps.hpp"
#include "assist/hitboxes.hpp"
#include "checkpoint/fix.hpp"
#include "label/label.hpp"
#include "render/renderer.hpp"
#include "replay/system.hpp"
#include "scheduler.hpp"
#include "shared/value/value.hpp"
#include "trajectory/trajectory.hpp"
#include "ui/manager.hpp"
#include "ui/widgets/info_modal.hpp"
#include "updater.hpp"

#ifdef SILICATE_PROTECT
#include "VMProtect/VMProtectSDK.h"
#endif

using namespace geode::prelude;

class Bot::Impl {
    BotUpdater m_updater;
    BotScheduler m_scheduler;

    UIManager m_ui;

    ReplaySystem m_replaySystem;

    PracticeFix m_practiceFix;
    TrajectoryManager m_trajectory;

    Autoclicker m_autoclicker;
    Hitboxes m_hitboxes;

    labels::LabelManager m_labels;
    CPSCounter m_cps;

    friend class Bot;
};

#define BOT_GETTER(ty, name) \
    ty& Bot::name() { return m_impl->m_##name; }

BOT_GETTER(BotScheduler, scheduler)
BOT_GETTER(BotUpdater, updater)
BOT_GETTER(UIManager, ui)
BOT_GETTER(ReplaySystem, replaySystem)
BOT_GETTER(PracticeFix, practiceFix)
BOT_GETTER(TrajectoryManager, trajectory)
BOT_GETTER(Autoclicker, autoclicker)
BOT_GETTER(Hitboxes, hitboxes)
BOT_GETTER(labels::LabelManager, labels)
BOT_GETTER(CPSCounter, cps)

Bot::Bot() : m_impl(std::make_unique<Impl>()) {}
Bot::~Bot() = default;

void Bot::initialize() {
    std::filesystem::path dir = Mod::get()->getPersistentDir(true);

#define TRY_OR_LOG_ERR(x)                                \
    if (auto _err = x; _err.isErr()) {                   \
        geode::log::error("Failed to create {}: {}", #x, \
                          _err.unwrapErr().message());   \
    }

    TRY_OR_LOG_ERR(asp::fs::createDirAll(dir / "replays"));
    TRY_OR_LOG_ERR(asp::fs::createDirAll(dir / "videos"));
    TRY_OR_LOG_ERR(asp::fs::createDirAll(dir / "logs"));
    TRY_OR_LOG_ERR(asp::fs::createDirAll(dir / "presets"));
    TRY_OR_LOG_ERR(asp::fs::createDirAll(dir / "scripts"));
    TRY_OR_LOG_ERR(asp::fs::createDirAll(dir / "backups"));
    TRY_OR_LOG_ERR(asp::fs::createDirAll(dir / "libraries"));

#undef TRY_OR_LOG_ERR

    // CBF fix
    geode::Mod* cbf =
        Loader::get()->getInstalledMod("syzzi.click_between_frames");
    if (cbf) {
        cbf->setSettingValue("soft-toggle", true);
        cbf->setSettingValue("physics-bypass", false);
    }

    geode::Mod* sip =
        Loader::get()->getInstalledMod("chizz.superb-input-precision");
    if (sip) {
        sip->setSettingValue("mod-enabled", false);
    }

    std::filesystem::path settingsPath =
        geode::Mod::get()->getConfigDir(true) / "settings.json";
    auto& settings = *SLSettings::get();
    auto ec =
        glz::read_file_json(settings, settingsPath.string(), std::string{});
    if (ec) {
        geode::log::error("Failed to read settings");
    }

    std::filesystem::path keybindsPath =
        geode::Mod::get()->getConfigDir(true) / "keybinds.json";
    SLBindingManager::get()->readFromFile(keybindsPath);

    if (settings.lastLoadedPreset != "") {
        auto presetPath = Mod::get()->getPersistentDir() / "presets" /
                          std::string(settings.lastLoadedPreset + ".json");

        if (asp::fs::exists(presetPath)) {
            geode::log::info("Loading preset {}", settings.lastLoadedPreset);
            Renderer::get()->loadSettings(presetPath);
            Bot::get()->ui().m_state.m_presetName = settings.lastLoadedPreset;
        } else {
            geode::log::error("Preset {} does not exist",
                              settings.lastLoadedPreset);
        }
    } else {
        Renderer::get()->initializeDefaults();
    }

    m_enabled->handle([&](bool& enabled) {
        if (PlayLayer::get()) {
            enabled = true;

            slui::showInfoModal({
                .title = "Error",
                .message =
                    "Please leave the level you're in to disable Silicate.",
            });

            return;
        }

        if (!enabled) {
            this->updater().m_tps->inner() = 240.0;
            this->updater().m_tps->notifyChange();
        }

        auto patches = Mod::get()->getPatches();
        std::ranges::for_each(patches, [enabled](Patch* p) {
            if (enabled) {
                geode::log::info("Enabling patch at 0x{:x}", p->getAddress());
                if (GEODE_UNWRAP_IF_ERR(e, p->enable())) {
                    geode::log::error("Failed to enable patch at 0x{:x}: {}",
                                      p->getAddress(), e);
                }
            } else {
                geode::log::info("Disabling patch at 0x{:x}", p->getAddress());
                (void)p->disable();
            }
        });

        auto hooks = Mod::get()->getHooks();
        std::ranges::for_each(hooks, [enabled](Hook* h) {
            if (h->getDisplayName() == "cocos2d::CCEGLView::swapBuffers") {
                return;
            }

            if (enabled) {
                if (GEODE_UNWRAP_IF_ERR(e, h->enable())) {
                    geode::log::error("Failed to enable hook for {}: {}",
                                      h->getDisplayName(), e);
                }
            } else {
                (void)h->disable();
            }
        });

        if (enabled) {
            geode::log::info("Successfully enabled Silicate.");
        } else {
            geode::log::info("Successfully disabled Silicate.");
        }
    });

    this->replaySystem().m_autosaveInterval->notifyChange();

    m_hasInitialized = true;

    m_enabled->notifyChange();
}

bool Bot::isEnabled() { return m_enabled->inner(); }
