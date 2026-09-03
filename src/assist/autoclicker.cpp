#include "autoclicker.hpp"

#include "bot/bot.hpp"
#include "bot/updater.hpp"

void Autoclicker::updatePlayer(PlayLayer* pl, bool player2) {
    PlayerSettings& s = player2 ? m_player2 : m_player1;

    if (!pl) {
        s.m_clicked = false;
        return;
    }

    if (!s.m_enabled || !m_enabled->inner()) {
        if (s.m_clicked) {
            pl->queueButton(1, false, player2, 0.0);
            s.m_clicked = false;
        }

        return;
    }

    auto bot = Bot::get();
    if (!bot->isRecording()) {
        return;
    }

    const uint64_t tick = bot->updater().getFrame();
    if (tick == s.m_lastFrame) {
        return;
    }

    uint64_t nextEventTick =
        s.m_lastFrame + (s.m_clicked ? s.m_holdDelay : s.m_releaseDelay);

    if (tick >= nextEventTick || s.m_lastFrame == UINT64_MAX) {
        s.m_lastFrame = tick;

        if (!s.m_clicked) {
            pl->queueButton(1, true, player2, 0.0);

            for (size_t i = 0; i < s.m_clicksPerHold - 1; i++) {
                pl->queueButton(1, false, player2, 0.0);
                pl->queueButton(1, true, player2, 0.0);
            }

            s.m_clicked = true;
        } else {
            pl->queueButton(1, false, player2, 0.0);
            s.m_clicked = false;

            return;
        }

        if (s.m_performSwifts && s.m_clicked) {
            pl->queueButton(1, false, player2, 0.0);
            s.m_clicked = false;
        }
    }
}

void Autoclicker::update(PlayLayer* pl) {
    updatePlayer(pl, false);
    updatePlayer(pl, true);
}
