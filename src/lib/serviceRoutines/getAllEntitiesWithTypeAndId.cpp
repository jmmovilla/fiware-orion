/*
*
* Copyright 2014 Telefonica Investigacion y Desarrollo, S.A.U
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

#include "logMsg/logMsg.h"
#include "logMsg/traceLevels.h"

#include "convenienceMap/mapGetIndividualContextEntity.h"
#include "ngsi/ParseData.h"
#include "ngsi/ContextElementResponse.h"
#include "rest/ConnectionInfo.h"
#include "serviceRoutines/getAllEntitiesWithTypeAndId.h"



/* ****************************************************************************
*
* getAllEntitiesWithTypeAndId - 
*/
extern std::string getAllEntitiesWithTypeAndId
(
  ConnectionInfo*            ciP,
  int                        components,
  std::vector<std::string>&  compV,
  ParseData*                 parseDataP
)
{
  std::string             answer;
  std::string             enType = compV[3];
  std::string             enId   = compV[5];
  ContextElementResponse  response;

  LM_T(LmtConvenience, ("CONVENIENCE: got 'GET' request with type='%s', and id='%s'", enType.c_str(), enId.c_str()));

  ciP->httpStatusCode = mapGetIndividualContextEntity(enId, enType, EntityTypeEmptyOrNotEmpty, &response, ciP);
  answer = response.render(ciP, IndividualContextEntity, "");
  response.release();

  return answer;
}
