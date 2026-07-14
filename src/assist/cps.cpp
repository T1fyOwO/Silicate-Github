#include "cps.hpp"

#include "bot/bot.hpp"
#include "bot/updater.hpp"

int CPSCounter::queryCPS(int player) {
    if (player == 1) {
        return m_bufferedActionsP1.size();
    } else if (player == 2) {
        return m_bufferedActionsP2.size();
    }

    return -1;
}

int CPSCounter::queryMaxCPS(int player) {
    if (player == 1) {
        return m_maxCPSP1;
    } else if (player == 2) {
        return m_maxCPSP2;
    }

    return -1;
}

void CPSCounter::clearActions() {
    m_maxCPSP1 = 0;
    m_maxCPSP2 = 0;

    while (!m_bufferedActionsP1.empty()) m_bufferedActionsP1.pop();
    while (!m_bufferedActionsP2.empty()) m_bufferedActionsP2.pop();
}

void CPSCounter::pushAction(slc::v3::Action a) {
    double time = GJBaseGameLayer::get()->m_gameState.m_levelTime;

    if (a.m_player2) {
        m_bufferedActionsP2.push({
            .m_action = a,
            .m_time = time,
        });
    } else {
        m_bufferedActionsP1.push({
            .m_action = a,
            .m_time = time,
        });
    }

    m_maxCPSP1 =
        std::max(m_maxCPSP1, static_cast<int>(m_bufferedActionsP1.size()));
    m_maxCPSP2 =
        std::max(m_maxCPSP2, static_cast<int>(m_bufferedActionsP2.size()));
}

void CPSCounter::update() {
    if (m_bufferedActionsP1.empty() && m_bufferedActionsP2.empty()) return;

    double twTime = GJBaseGameLayer::get()->m_gameState.m_levelTime;

    while (!m_bufferedActionsP1.empty() &&
           twTime - m_bufferedActionsP1.front().m_time > 1.0) {
        m_bufferedActionsP1.pop();
    }

    while (!m_bufferedActionsP2.empty() &&
           twTime - m_bufferedActionsP2.front().m_time > 1.0) {
        m_bufferedActionsP2.pop();
    }
}
