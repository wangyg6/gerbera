/*MT*
    
    MediaTomb - http://www.mediatomb.cc/
    
    session_manager.cc - this file is part of MediaTomb.
    
    Copyright (C) 2005 Gena Batyan <bgeradz@mediatomb.cc>,
                       Sergey 'Jin' Bostandzhyan <jin@mediatomb.cc>
    
    Copyright (C) 2006-2010 Gena Batyan <bgeradz@mediatomb.cc>,
                            Sergey 'Jin' Bostandzhyan <jin@mediatomb.cc>,
                            Leonhard Wimmer <leo@mediatomb.cc>
    
    MediaTomb is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2
    as published by the Free Software Foundation.
    
    MediaTomb is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    version 2 along with MediaTomb; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
    
    $Id$
*/

/// \file session_manager.cc

#include <memory>
#include <unordered_set>

#include "config/config_manager.h"
#include "session_manager.h"
#include "util/timer.h"
#include "util/tools.h"

#define UI_UPDATE_ID_HASH_SIZE 61
#define MAX_UI_UPDATE_IDS 10

using namespace mxml;
using namespace std;

namespace web {

Session::Session(long timeout)
{
    this->timeout = timeout;
    loggedIn = false;
    sessionID = "";
    uiUpdateIDs = make_shared<unordered_set<int>>();
    //(new DBRHash<int>(UI_UPDATE_ID_HASH_SIZE, MAX_UI_UPDATE_IDS + 5, INVALID_OBJECT_ID, INVALID_OBJECT_ID_2));
    updateAll = false;
    access();
}

void Session::put(std::string key, std::string value)
{
    AutoLock lock(mutex);
    dict[key] = value;
}

std::string Session::get(std::string key)
{
    AutoLock lock(mutex);
    return getValueOrDefault(dict, key);
}

void Session::containerChangedUI(int objectID)
{
    if (objectID == INVALID_OBJECT_ID)
        return;
    if (!updateAll) {
        AutoLock lock(mutex);
        if (!updateAll) {
            if (uiUpdateIDs->size() >= MAX_UI_UPDATE_IDS) {
                updateAll = true;
                uiUpdateIDs->clear();
            } else
                uiUpdateIDs->insert(objectID);
        }
    }
}

void Session::containerChangedUI(const std::vector<int>& objectIDs)
{
    if (updateAll)
        return;

    size_t arSize = objectIDs.size();
    AutoLock lock(mutex);

    if (updateAll)
        return;

    if (uiUpdateIDs->size() + arSize >= MAX_UI_UPDATE_IDS) {
        updateAll = true;
        uiUpdateIDs->clear();
        return;
    }
    for (int objectId : objectIDs) {
        uiUpdateIDs->insert(objectId);
    }
}

std::string Session::getUIUpdateIDs()
{
    if (!hasUIUpdateIDs())
        return "";
    AutoLock lock(mutex);
    if (updateAll) {
        updateAll = false;
        return "all";
    }
    std::string ret = toCSV(uiUpdateIDs);
    if (!ret.empty())
        uiUpdateIDs->clear();
    return ret;
}

bool Session::hasUIUpdateIDs()
{
    if (updateAll)
        return true;
    // AutoLock lock(mutex); only accessing an int - shouldn't be necessary
    return (uiUpdateIDs->size() > 0);
}

void Session::clearUpdateIDs()
{
    log_debug("clearing UI updateIDs\n");
    AutoLock lock(mutex);
    uiUpdateIDs->clear();
    updateAll = false;
}

SessionManager::SessionManager(std::shared_ptr<ConfigManager> config, std::shared_ptr<Timer> timer)
{
    this->timer = timer;
    accounts = config->getDictionaryOption(CFG_SERVER_UI_ACCOUNT_LIST);
    timerAdded = false;
}

std::shared_ptr<Session> SessionManager::createSession(long timeout)
{
    auto newSession = std::make_shared<Session>(timeout);
    AutoLock lock(mutex);

    int count = 0;
    std::string sessionID;
    do {
        sessionID = generate_random_id();
        if (count++ > 100)
            throw _Exception("There seems to be something wrong with the random numbers. I tried to get a unique id 100 times and failed. last sessionID: " + sessionID);
    } while (getSession(sessionID, false) != nullptr); // for the rare case, where we get a random id, that is already taken

    newSession->setID(sessionID);
    sessions.push_back(newSession);
    checkTimer();
    return newSession;
}

std::shared_ptr<Session> SessionManager::getSession(std::string sessionID, bool doLock)
{
    unique_lock<decltype(mutex)> lock(mutex, std::defer_lock);
    if (doLock)
        lock.lock();
    for (size_t i = 0; i < sessions.size(); i++) {
        auto s = sessions[i];
        if (s->getID() == sessionID)
            return s;
    }
    return nullptr;
}

void SessionManager::removeSession(std::string sessionID)
{
    AutoLock lock(mutex);
    for (size_t i = 0; i < sessions.size(); i++) {
        auto s = sessions[i];
        if (s->getID() == sessionID) {
            sessions.erase(sessions.begin() + i);
            checkTimer();
            i--; // to not skip a session. the removed id is now taken by another session
            return;
        }
    }
}

std::string SessionManager::getUserPassword(std::string user)
{
    return getValueOrDefault(accounts, user);
}

void SessionManager::containerChangedUI(int objectID)
{
    if (sessions.size() <= 0)
        return;
    AutoLock lock(mutex);
    for (size_t i = 0; i < sessions.size(); i++) {
        auto session = sessions[i];
        if (session->isLoggedIn())
            session->containerChangedUI(objectID);
    }
}

void SessionManager::containerChangedUI(const std::vector<int>& objectIDs)
{
    if (sessions.size() <= 0)
        return;
    AutoLock lock(mutex);
    for (size_t i = 0; i < sessions.size(); i++) {
        auto session = sessions[i];
        if (session->isLoggedIn())
            session->containerChangedUI(objectIDs);
    }
}

void SessionManager::checkTimer()
{
    if (sessions.size() > 0 && !timerAdded) {
        timer->addTimerSubscriber(this, SESSION_TIMEOUT_CHECK_INTERVAL);
        timerAdded = true;
    } else if (sessions.size() <= 0 && timerAdded) {
        timer->removeTimerSubscriber(this);
        timerAdded = false;
    }
}

void SessionManager::timerNotify(std::shared_ptr<Timer::Parameter> parameter)
{
    log_debug("notified... %d web sessions.\n", sessions.size());
    AutoLock lock(mutex);
    struct timespec now;
    getTimespecNow(&now);
    for (size_t i = 0; i < sessions.size(); i++) {
        auto session = sessions[i];
        if (getDeltaMillis(session->getLastAccessTime(), &now) > 1000 * session->getTimeout()) {
            log_debug("session timeout: %s - diff: %ld\n", session->getID().c_str(), getDeltaMillis(session->getLastAccessTime(), &now));
            sessions.erase(sessions.begin() + i);
            checkTimer();
            i--; // to not skip a session. the removed id is now taken by another session
        }
    }
}

} // namespace
