/*MT*
    
    MediaTomb - http://www.mediatomb.cc/
    
    add_object.cc - this file is part of MediaTomb.
    
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

/// \file add_object.cc

#include "cds_objects.h"
#include "common.h"
#include "config/config_manager.h"
#include "content_manager.h"
#include "metadata/metadata_handler.h"
#include "pages.h"
#include "server.h"
#include "util/tools.h"
#include <cstdio>

using namespace zmm;
using namespace mxml;

web::addObject::addObject(std::shared_ptr<ConfigManager> config, std::shared_ptr<Storage> storage,
    std::shared_ptr<ContentManager> content, std::shared_ptr<SessionManager> sessionManager)
    : WebRequestHandler(config, storage, content, sessionManager)
{
}

/*static Ref<Element> addOption(std::string option_name, std::string option_type, std::string default_value = "")
{
    Ref<Element> option (new Element("option"));
    option->addAttribute("name", option_name);
    option->addAttribute("type", option_type);
    
    if (default_value != nullptr)
        option->addAttribute("default", default_value);
    
    return option;
}*/

void web::addObject::addContainer(int parentID)
{
    content->addContainer(parentID, param("title"), param("class"));
}

std::shared_ptr<CdsObject> web::addObject::addItem(int parentID, std::shared_ptr<CdsItem> item)
{
    std::string tmp;

    item->setParentID(parentID);

    item->setTitle(param("title"));
    item->setLocation(param("location"));
    item->setClass(param("class"));

    tmp = param("description");
    if (string_ok(tmp))
        item->setMetadata(MetadataHandler::getMetaFieldName(M_DESCRIPTION), tmp);

    /// \todo is there a default setting? autoscan? import settings?
    tmp = param("mime-type");
    if (!string_ok(tmp))
        tmp = MIMETYPE_DEFAULT;
    item->setMimeType(tmp);

    item->setFlag(OBJECT_FLAG_USE_RESOURCE_REF);

    return item;
}

std::shared_ptr<CdsObject> web::addObject::addActiveItem(int parentID)
{
    std::string tmp;
    auto item = std::make_shared<CdsActiveItem>(storage);

    item->setAction(param("action"));

    /// \todo is there a default setting? autoscan? import settings?
    tmp = param("state");
    if (string_ok(tmp))
        item->setState(tmp);

    item->setParentID(parentID);
    item->setLocation(param("location"));

    tmp = param("mime-type");
    if (!string_ok(tmp))
        tmp = MIMETYPE_DEFAULT;
    item->setMimeType(tmp);

    MetadataHandler::setMetadata(config, item);

    item->setTitle(param("title"));
    item->setClass(param("class"));

    tmp = param("description");
    if (string_ok(tmp))
        item->setMetadata(MetadataHandler::getMetaFieldName(M_DESCRIPTION), tmp);

    /// \todo is there a default setting? autoscan? import settings?

    //    auto resource = std::make_shared<CdsResource>(CH_DEFAULT);

    //    auto resource = item->getResource(0); // added by m-handler
    //    resource->addAttribute(MetadataHandler::getResAttrName(R_PROTOCOLINFO),
    //                           renderProtocolInfo(tmp));
    //    item->addResource(resource);

    return item;
}

std::shared_ptr<CdsObject> web::addObject::addUrl(int parentID, std::shared_ptr<CdsItemExternalURL> item, bool addProtocol)
{
    std::string tmp;
    std::string protocolInfo;

    item->setParentID(parentID);

    item->setTitle(param("title"));
    item->setURL(param("location"));
    item->setClass(param("class"));

    tmp = param("description");
    if (string_ok(tmp))
        item->setMetadata(MetadataHandler::getMetaFieldName(M_DESCRIPTION), tmp);

    /// \todo is there a default setting? autoscan? import settings?
    tmp = param("mime-type");
    if (!string_ok(tmp))
        tmp = MIMETYPE_DEFAULT;
    item->setMimeType(tmp);

    if (addProtocol) {
        std::string protocol = param("protocol");
        if (string_ok(protocol))
            protocolInfo = renderProtocolInfo(tmp, protocol);
        else
            protocolInfo = renderProtocolInfo(tmp);
    } else
        protocolInfo = renderProtocolInfo(tmp);

    auto resource = std::make_shared<CdsResource>(CH_DEFAULT);
    resource->addAttribute(MetadataHandler::getResAttrName(R_PROTOCOLINFO),
        protocolInfo);
    item->addResource(resource);

    return item;
}

void web::addObject::process()
{
    check_request();

    std::string obj_type = param("obj_type");
    std::string location = param("location");

    if (!string_ok(param("title")))
        throw _Exception("empty title");

    if (!string_ok(param("class")))
        throw _Exception("empty class");

    int parentID = intParam("parent_id", 0);

    std::shared_ptr<CdsObject> obj = nullptr;

    Ref<Element> updateContainerEl;

    bool allow_fifo = false;

    if (obj_type == STRING_OBJECT_TYPE_CONTAINER) {
        this->addContainer(parentID);
        //updateContainerEl = Ref<Element>(new Element("updateContainer"));
        //updateContainerEl->setText(parID);
        //updateContainerEl->addAttribute("add", "1");
        //root->appendChild(updateContainerEl);
    } else if (obj_type == STRING_OBJECT_TYPE_ITEM) {
        if (!string_ok(location))
            throw _Exception("no location given");
        if (!check_path(location, false))
            throw _Exception("file not found");
        obj = this->addItem(parentID, std::make_shared<CdsItem>(storage));
        allow_fifo = true;
    } else if (obj_type == STRING_OBJECT_TYPE_ACTIVE_ITEM) {
        if (!string_ok(param("action")))
            throw _Exception("no action given");
        if (!string_ok(location))
            throw _Exception("no location given");
        if (!check_path(location, false))
            throw _Exception("path not found");
        obj = this->addActiveItem(parentID);
        allow_fifo = true;
    } else if (obj_type == STRING_OBJECT_TYPE_EXTERNAL_URL) {
        if (!string_ok(location))
            throw _Exception("No URL given");
        obj = this->addUrl(parentID, std::make_shared<CdsItemExternalURL>(storage), true);
    } else if (obj_type == STRING_OBJECT_TYPE_INTERNAL_URL) {
        if (!string_ok(location))
            throw _Exception("No URL given");
        obj = this->addUrl(parentID, std::make_shared<CdsItemInternalURL>(storage), false);
    } else {
        throw _Exception("unknown object type: " + obj_type);
    }

    if (obj != nullptr) {
        obj->setVirtual(true);
        if (obj_type == STRING_OBJECT_TYPE_ITEM) {
            content->addVirtualItem(obj, allow_fifo);
        } else {
            content->addObject(obj);
        }
    }
}
