#ifndef SRC_LIB_CONVENIENCE_REGISTERPROVIDERREQUEST_H_
#define SRC_LIB_CONVENIENCE_REGISTERPROVIDERREQUEST_H_

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
#include <vector>

#include "ngsi/MetadataVector.h"
#include "ngsi/Duration.h"
#include "ngsi/ProvidingApplication.h"
#include "ngsi/RegistrationId.h"
#include "common/Format.h"



/* ****************************************************************************
*
* RegisterProviderRequest - 
*/
typedef struct RegisterProviderRequest
{
  MetadataVector             metadataVector;             // Optional
  Duration                   duration;                   // Optional
  ProvidingApplication       providingApplication;       // Mandatory
  RegistrationId             registrationId;             // Optional

  RegisterProviderRequest();

  std::string  render(Format format, std::string indent);
  std::string  check(RequestType requestType, Format format, std::string indent, std::string preError, int counter);
  void         present(std::string indent);
  void         release();
} RegisterProviderRequest;

#endif  // SRC_LIB_CONVENIENCE_REGISTERPROVIDERREQUEST_H_
