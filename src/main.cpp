#include <Geode/Geode.hpp>

using namespace geode::prelude;

#include "bot/bot.hpp"

$on_mod(Loaded) { Bot::get()->initialize(); }
