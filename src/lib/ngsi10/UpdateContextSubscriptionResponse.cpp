/*
*
* Copyright 2013 Telefonica Investigacion y Desarrollo, S.A.U
*
* This file is part of Orion Context Broker.
*
* Orion Context Broker is free software: you can redistribute it and/or
* modify it under the terms of the GNU Affero General Public License as
* published by the Free Software Foundation, either version 3 of the
* License, or (at your option) any later version.
*
* Orion Context Broker is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero
* General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with Orion Context Broker. If not, see http://www.gnu.org/licenses/.
*
* For those usages not covered by this license please contact with
* fermin at tid dot es
*
* Author: Ken Zangelin
*/
#include <string>

#include "logMsg/traceLevels.h"
#include "logMsg/logMsg.h"
#include "common/Format.h"
#include "common/tag.h"
#include "ngsi/StatusCode.h"
#include "ngsi/SubscribeResponse.h"
#include "ngsi/SubscribeError.h"
#include "ngsi10/UpdateContextSubscriptionResponse.h"


/* ****************************************************************************
*
* UpdateContextSubscriptionResponse::UpdateContextSubscriptionResponse -
*/
UpdateContextSubscriptionResponse::UpdateContextSubscriptionResponse() {
   subscribeError.errorCode.tagSet("errorCode");
}

/* ****************************************************************************
*
* UpdateContextSubscriptionResponse::UpdateContextSubscriptionResponse -
*/
UpdateContextSubscriptionResponse::UpdateContextSubscriptionResponse(StatusCode& errorCode) {
   subscribeError.subscriptionId.set("000000000000000000000000");
   subscribeError.errorCode.fill(&errorCode);
   subscribeError.errorCode.tagSet("errorCode");
}

/* ****************************************************************************
*
* UpdateContextSubscriptionResponse::~UpdateContextSubscriptionResponse -
*/
UpdateContextSubscriptionResponse::~UpdateContextSubscriptionResponse() {
    LM_T(LmtDestructor,("destroyed"));
}

/* ****************************************************************************
*
* UpdateContextSubscriptionResponse::render - 
*/
std::string UpdateContextSubscriptionResponse::render(RequestType requestType, Format format, const std::string& indent)
{
  std::string out  = "";
  std::string tag  = "updateContextSubscriptionResponse";

  out += startTag(indent, tag, format, false);

  if (subscribeError.errorCode.code == SccNone)
     out += subscribeResponse.render(format, indent + "  ", false);
  else
     out += subscribeError.render(UpdateContextSubscription, format, indent + "  ", false);

  out += endTag(indent, tag, format);

  return out;
}
