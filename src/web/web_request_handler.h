/*MT*
    
    MediaTomb - http://www.mediatomb.cc/
    
    web_request_handler.h - this file is part of MediaTomb.
    
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

/// \file web_request_handler.h
/// \brief Definition of the WebRequestHandler class.
#ifndef __WEB_REQUEST_HANDLER_H__
#define __WEB_REQUEST_HANDLER_H__

#include "common.h"
#include "config/config_manager.h"
#include "session_manager.h"
#include "mxml/mxml.h"
#include "request_handler.h"
#include "util/exception.h"
#include "util/generic_task.h"

// forward declaration
class ContentManager;
class Storage;

namespace web {

class SessionException : public Exception {
public:
    SessionException(std::string message)
        : Exception(message)
    {
    }
};

class LoginException : public Exception {
public:
    LoginException(std::string message)
        : Exception(message)
    {
    }
};

class SessionManager;

/// \brief This class is responsible for processing requests that come to the user interface.
class WebRequestHandler : public RequestHandler {
protected:
    std::shared_ptr<ContentManager> content;
    std::shared_ptr<web::SessionManager> sessionManager;

    bool checkRequestCalled;

    /// \brief Decoded URL parameters are stored as a dictionary.
    std::map<std::string,std::string> params;

    /// \brief The original filename from url if anyone needs it.
    std::string filename;

    /// \brief We can also always see what mode was requested.
    enum UpnpOpenFileMode mode;

    /// \brief This is the root xml element to be populated by process() method.
    zmm::Ref<mxml::Element> root;

    /// \brief The current session, used for this request; will be filled by
    /// check_request()
    std::shared_ptr<Session> session;

    /// \brief Little support function to access stuff from the dictionary in
    /// in an easier fashion.
    /// \param name of the parameter we are looking for.
    /// \return Value of the parameter with the given name or nullptr if not found.
    inline std::string param(std::string name) { return getValueOrDefault(params, name); }

    int intParam(std::string name, int invalid = 0);
    bool boolParam(std::string name);

    /// \brief Checks if the incoming request is valid.
    ///
    /// Each request going to the ui must at least have a valid session id,
    /// and a driver parameter. Also, there can only by a primary or a
    /// a decondary driver.
    void check_request(bool checkLogin = true);

    /// \brief Helper function to create a generic XML document header.
    /// \param xsl_link If not nullptr, also adds header information that is required for the XSL processor.
    /// \return The header as a string... because our parser does not yet understand <? ?> stuff :)
    std::string renderXMLHeader();

    /// \brief Prepares the output buffer and calls the process function.
    /// \return IOHandler
    /// \todo Genych, chto tut proishodit, ya tolkom che to ne wrubaus??
    std::unique_ptr<IOHandler> open(enum UpnpOpenFileMode mode);

    /// \brief add the ui update ids from the given session as xml tags to the given root element
    /// \param root the xml element to add the elements to
    /// \param session the session from which the ui update ids should be taken
    void addUpdateIDs(zmm::Ref<mxml::Element> root, std::shared_ptr<Session> session);

    /// \brief check if ui update ids should be added to the response and add
    /// them in that case.
    /// must only be called after check_request
    void handleUpdateIDs();

    /// \brief add the content manager task to the given xml element as xml elements
    /// \param el the xml element to add the elements to
    /// \param task the task to add to the given xml element
    void appendTask(zmm::Ref<mxml::Element> el, zmm::Ref<GenericTask> task);

    /// \brief check if accounts are enabled in the config
    /// \return true if accounts are enabled, false if not
    bool accountsEnabled() { return config->getBoolOption(CFG_SERVER_UI_ACCOUNTS_ENABLED); }

    std::string mapAutoscanType(int type);

public:
    /// \brief Constructor, currently empty.
    WebRequestHandler(std::shared_ptr<ConfigManager> config, std::shared_ptr<Storage> storage,
        std::shared_ptr<ContentManager> content, std::shared_ptr<web::SessionManager> sessionManager);

    /// \brief Returns information about the requested content.
    /// \param filename Requested URL
    /// \param info File_Info structure, quite similar to statbuf.
    ///
    /// We need to override this, because for serving UI pages (mostly generated from
    /// dynamic XML) we do not know the size of the data. This is of course different
    /// for the FileRequestHandler, where we can check the file and return all
    /// information about it.
    void getInfo(const char *filename, UpnpFileInfo *info) override;

    /// \brief Decodes the parameters from the filename (URL) and calls the internal open() function.
    /// \param filename The requested URL
    /// \param mode either UPNP_READ or UPNP_WRITE
    /// \return the appropriate IOHandler for the request.
    std::unique_ptr<IOHandler> open(const char* filename,
        enum UpnpOpenFileMode mode,
        std::string range) override;

    /// \brief This method must be overridden by the subclasses that actually process the given request.
    virtual void process() = 0;
};

} // namespace

#endif // __WEB_REQUEST_HANDLER_H__
