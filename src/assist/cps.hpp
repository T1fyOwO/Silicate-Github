#pragma once

#include <queue>
#include <slc/slc.hpp>

class CPSCounter {
    struct TimedAction {
        slc::v3::Action m_action;
        double m_time;
    };

   private:
    std::queue<TimedAction> m_bufferedActionsP1;
    std::queue<TimedAction> m_bufferedActionsP2;

    int m_maxCPSP1;
    int m_maxCPSP2;

   public:
    int queryCPS(int player = 1);
    int queryMaxCPS(int player = 1);

    void clearActions();
    void pushAction(slc::v3::Action a);

    void update();
};
