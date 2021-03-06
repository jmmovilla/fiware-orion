#ifndef SRC_LIB_NGSI_STATUSCODE_H_
#define SRC_LIB_NGSI_STATUSCODE_H_

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

#include "common/Format.h"
#include "ngsi/Request.h"
#include "rest/HttpStatusCode.h"



/* ****************************************************************************
*
* StatusCode - 
*/
typedef struct StatusCode
{
  HttpStatusCode  code;             // Mandatory
  std::string     reasonPhrase;     // Mandatory
  std::string     details;          // Optional

  std::string     tag;              // tag to be rendered

  StatusCode();
  StatusCode(const std::string& _tag);
  StatusCode(HttpStatusCode _code, const std::string& _details, const std::string& _tag = "statusCode");

  std::string  render(Format format, const std::string& indent, bool comma = false, bool showTag = true);
  void         fill(HttpStatusCode _code, const std::string& _details = "");
  void         fill(StatusCode* scP);
  void         present(const std::string& indent);
  void         release(void);
  void         tagSet(const std::string& _tag);

  std::string  check(RequestType         requestType,
                     Format              format,
                     const std::string&  indent,
                     const std::string&  predetectedError,
                     int                 counter);
} StatusCode;

#endif  // SRC_LIB_NGSI_STATUSCODE_H_
