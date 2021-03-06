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
* Author: Fermin Galan
*/
#include "gtest/gtest.h"
#include "unittest.h"

#include "logMsg/logMsg.h"
#include "logMsg/traceLevels.h"

#include "common/globals.h"
#include "mongoBackend/MongoGlobal.h"
#include "mongoBackend/mongoUpdateContext.h"
#include "ngsi/EntityId.h"
#include "ngsi10/UpdateContextRequest.h"
#include "ngsi10/UpdateContextResponse.h"

#include "mongo/client/dbclient.h"

/* ****************************************************************************
*
* Tests
*
* - newEntityLocAttribute
* - appendLocAttribute
* - updateLocAttribute
* - deleteLocAttribute
* - newEntityTwoLocAttributesFail
* - newEntityWrongCoordinatesFormatFail
* - newEntityNotSupportedLocationFail
* - appendAdditionalLocAttributeFail
* - appendWrongCoordinatesFormatFail
* - appendNotSupportedLocationFail
* - updateWrongCoordinatesFormatFail
* - updateLocationMetadataFail
* - deleteLocationMetadataFail
*
*/

/* ****************************************************************************
*
* prepareDatabase -
*
* This function is called before every test, to populate some information in the
* entities collection.
*/
static void prepareDatabase(void) {

  /* Set database */
  setupDatabase();

  DBClientBase* connection = getMongoConnection();


  BSONObj en1 = BSON("_id" << BSON("id" << "E1" << "type" << "T1") <<
                     "attrs" << BSON_ARRAY(
                        BSON("name" << "A1" << "type" << "TA1" << "value" << "-5, 2") <<
                        BSON("name" << "A2" << "type" << "TA2" << "value" << "noloc")
                        ) <<
                     "location" << BSON("attrName" << "A1" << "coords" << BSON_ARRAY(2.0 << -5.0))
                    );

  BSONObj en2 = BSON("_id" << BSON("id" << "E2" << "type" << "T2") <<
                     "attrs" << BSON_ARRAY(
                        BSON("name" << "A2" << "type" << "TA2" << "value" << "Y")
                        )
                    );

  connection->insert(ENTITIES_COLL, en1);
  connection->insert(ENTITIES_COLL, en2);

}

#define coordX(e) e.getObjectField("location").getField("coords").Array()[0].Double()
#define coordY(e) e.getObjectField("location").getField("coords").Array()[1].Double()

/* ****************************************************************************
*
* getAttr -
*
* We need this function because we can not trust on array index, at mongo will
* not sort the elements within the array. This function assumes that always will
* find a result, that is ok for testing code.
*
* FIXME P4: this functions is repeated in several places (e.g. mongoUpdateContext_test.cpp). Factorice in a common place.
*/
static BSONObj getAttr(std::vector<BSONElement> attrs, std::string name, std::string type, std::string id = "") {

    BSONElement be;
    for (unsigned int ix = 0; ix < attrs.size(); ++ix) {
        BSONObj attr = attrs[ix].embeddedObject();
        std::string attrName = STR_FIELD(attr, "name");
        std::string attrType = STR_FIELD(attr, "type");
        std::string attrId = STR_FIELD(attr, "id");
        if (attrName == name && attrType == type && ( id == "" || attrId == id )) {
            be = attrs[ix];
            break;
        }
    }
    return be.embeddedObject();

}

/* ****************************************************************************
*
* newEntityLocAttribute -
*/
TEST(mongoUpdateContextGeoRequest, newEntityLocAttribute)
{
    utInit();

    HttpStatusCode         ms;
    UpdateContextRequest   req;
    UpdateContextResponse  res;

    /* Prepare database */
    prepareDatabase();

    /* Forge the request (from "inside" to "outside") */
    ContextElement ce;
    ce.entityId.fill("E3", "T3", "false");
    ContextAttribute ca("A3", "TA3", "4, -5");
    Metadata md("location", "string", "WSG84");
    ca.metadataVector.push_back(&md);
    ce.contextAttributeVector.push_back(&ca);
    req.contextElementVector.push_back(&ce);
    req.updateActionType.set("APPEND");

    /* Invoke the function in mongoBackend library */
    servicePathVector.clear();    
    ms = mongoUpdateContext(&req, &res, "", servicePathVector, uriParams);

    /* Check response is as expected */
    EXPECT_EQ(SccOk, ms);

    EXPECT_EQ(0, res.errorCode.code);
    EXPECT_EQ(0, res.errorCode.reasonPhrase.size());
    EXPECT_EQ(0, res.errorCode.details.size());

    ASSERT_EQ(1, res.contextElementResponseVector.size());
    /* Context Element response # 1 */
    EXPECT_EQ("E3", RES_CER(0).entityId.id);
    EXPECT_EQ("T3", RES_CER(0).entityId.type);
    EXPECT_EQ("false", RES_CER(0).entityId.isPattern);
    ASSERT_EQ(1, RES_CER(0).contextAttributeVector.size());
    EXPECT_EQ("A3", RES_CER_ATTR(0, 0)->name);
    EXPECT_EQ("TA3", RES_CER_ATTR(0, 0)->type);
    EXPECT_EQ(0, RES_CER_ATTR(0, 0)->value.size());
    ASSERT_EQ(1, RES_CER_ATTR(0, 0)->metadataVector.size());
    EXPECT_EQ("location", RES_CER_ATTR(0, 0)->metadataVector.get(0)->name);
    EXPECT_EQ("string", RES_CER_ATTR(0, 0)->metadataVector.get(0)->type);
    EXPECT_EQ("WSG84", RES_CER_ATTR(0, 0)->metadataVector.get(0)->value);
    EXPECT_EQ(SccOk, RES_CER_STATUS(0).code);
    EXPECT_EQ("OK", RES_CER_STATUS(0).reasonPhrase);
    EXPECT_EQ(0, RES_CER_STATUS(0).details.size());

    /* Check that every involved collection at MongoDB is as expected */
    /* Note we are using EXPECT_STREQ() for some cases, as Mongo Driver returns const char*, not string
     * objects (see http://code.google.com/p/googletest/wiki/Primer#String_Comparison) */

    DBClientBase* connection = getMongoConnection();

    /* entities collection */
    BSONObj ent;
    std::vector<BSONElement> attrs;
    ASSERT_EQ(3, connection->count(ENTITIES_COLL, BSONObj()));

    ent = connection->findOne(ENTITIES_COLL, BSON("_id.id" << "E1" << "_id.type" << "T1"));
    EXPECT_STREQ("E1", C_STR_FIELD(ent.getObjectField("_id"), "id"));
    EXPECT_STREQ("T1", C_STR_FIELD(ent.getObjectField("_id"), "type"));
    EXPECT_FALSE(ent.hasField("modDate"));
    EXPECT_FALSE(ent.hasField("creDate"));
    attrs = ent.getField("attrs").Array();
    ASSERT_EQ(2, attrs.size());
    BSONObj a1 = getAttr(attrs, "A1", "TA1");
    EXPECT_STREQ("A1", C_STR_FIELD(a1, "name"));
    EXPECT_STREQ("TA1", C_STR_FIELD(a1, "type"));
    EXPECT_STREQ("-5, 2", C_STR_FIELD(a1, "value"));
    EXPECT_FALSE(a1.hasField("creDate"));
    EXPECT_FALSE(a1.hasField("modDate"));
    BSONObj a2 = getAttr(attrs, "A2", "TA2");
    EXPECT_STREQ("A2", C_STR_FIELD(a2, "name"));
    EXPECT_STREQ("TA2", C_STR_FIELD(a2, "type"));
    EXPECT_STREQ("noloc", C_STR_FIELD(a2, "value"));
    EXPECT_FALSE(a2.hasField("creDate"));
    EXPECT_FALSE(a2.hasField("modDate"));
    EXPECT_STREQ("A1", ent.getObjectField("location").getStringField("attrName"));
    EXPECT_EQ(2, coordX(ent));
    EXPECT_EQ(-5, coordY(ent));

    ent = connection->findOne(ENTITIES_COLL, BSON("_id.id" << "E2" << "_id.type" << "T2"));
    EXPECT_STREQ("E2", C_STR_FIELD(ent.getObjectField("_id"), "id"));
    EXPECT_STREQ("T2", C_STR_FIELD(ent.getObjectField("_id"), "type"));
    EXPECT_FALSE(ent.hasField("modDate"));
    EXPECT_FALSE(ent.hasField("creDate"));
    attrs = ent.getField("attrs").Array();
    ASSERT_EQ(1, attrs.size());
    a2 = getAttr(attrs, "A2", "TA2");
    EXPECT_STREQ("A2", C_STR_FIELD(a2, "name"));
    EXPECT_STREQ("TA2", C_STR_FIELD(a2, "type"));
    EXPECT_STREQ("Y", C_STR_FIELD(a2, "value"));
    EXPECT_FALSE(a2.hasField("creDate"));
    EXPECT_FALSE(a2.hasField("modDate"));
    EXPECT_FALSE(ent.hasField("location"));

    ent = connection->findOne(ENTITIES_COLL, BSON("_id.id" << "E3" << "_id.type" << "T3"));
    EXPECT_STREQ("E3", C_STR_FIELD(ent.getObjectField("_id"), "id"));
    EXPECT_STREQ("T3", C_STR_FIELD(ent.getObjectField("_id"), "type"));
    EXPECT_EQ(1360232700, ent.getIntField("creDate"));
    EXPECT_EQ(1360232700, ent.getIntField("modDate"));
    attrs = ent.getField("attrs").Array();
    ASSERT_EQ(1, attrs.size());
    BSONObj a3 = getAttr(attrs, "A3", "TA3");
    EXPECT_STREQ("A3", C_STR_FIELD(a3, "name"));
    EXPECT_STREQ("TA3", C_STR_FIELD(a3, "type"));
    EXPECT_STREQ("4, -5", C_STR_FIELD(a3, "value"));
    EXPECT_EQ(1360232700, a3.getIntField("creDate"));
    EXPECT_EQ(1360232700, a3.getIntField("modDate"));
    EXPECT_STREQ("A3", ent.getObjectField("location").getStringField("attrName"));
    EXPECT_EQ(-5, coordX(ent));
    EXPECT_EQ(4, coordY(ent));

    /* Release connection */
    mongoDisconnect();

    utExit();

}

/* ****************************************************************************
*
*  - appendLocAttribute
*/
TEST(mongoUpdateContextGeoRequest, appendLocAttribute)
{
    utInit();

    HttpStatusCode         ms;
    UpdateContextRequest   req;
    UpdateContextResponse  res;

    /* Prepare database */
    prepareDatabase();

    /* Forge the request (from "inside" to "outside") */
    ContextElement ce;
    ce.entityId.fill("E2", "T2", "false");
    ContextAttribute ca("A5", "TA5", "8, -9");
    Metadata md("location", "string", "WSG84");
    ca.metadataVector.push_back(&md);
    ce.contextAttributeVector.push_back(&ca);
    req.contextElementVector.push_back(&ce);
    req.updateActionType.set("APPEND");

    /* Invoke the function in mongoBackend library */
    servicePathVector.clear();    
    ms = mongoUpdateContext(&req, &res, "", servicePathVector, uriParams);

    /* Check response is as expected */
    EXPECT_EQ(SccOk, ms);

    EXPECT_EQ(0, res.errorCode.code);
    EXPECT_EQ(0, res.errorCode.reasonPhrase.size());
    EXPECT_EQ(0, res.errorCode.details.size());

    ASSERT_EQ(1, res.contextElementResponseVector.size());
    /* Context Element response # 1 */
    EXPECT_EQ("E2", RES_CER(0).entityId.id);
    EXPECT_EQ("T2", RES_CER(0).entityId.type);
    EXPECT_EQ("false", RES_CER(0).entityId.isPattern);
    ASSERT_EQ(1, RES_CER(0).contextAttributeVector.size());
    EXPECT_EQ("A5", RES_CER_ATTR(0, 0)->name);
    EXPECT_EQ("TA5", RES_CER_ATTR(0, 0)->type);
    EXPECT_EQ(0, RES_CER_ATTR(0, 0)->value.size());
    ASSERT_EQ(1, RES_CER_ATTR(0, 0)->metadataVector.size());
    EXPECT_EQ("location", RES_CER_ATTR(0, 0)->metadataVector.get(0)->name);
    EXPECT_EQ("string", RES_CER_ATTR(0, 0)->metadataVector.get(0)->type);
    EXPECT_EQ("WSG84", RES_CER_ATTR(0, 0)->metadataVector.get(0)->value);
    EXPECT_EQ(SccOk, RES_CER_STATUS(0).code);
    EXPECT_EQ("OK", RES_CER_STATUS(0).reasonPhrase);
    EXPECT_EQ(0, RES_CER_STATUS(0).details.size());

    /* Check that every involved collection at MongoDB is as expected */
    /* Note we are using EXPECT_STREQ() for some cases, as Mongo Driver returns const char*, not string
     * objects (see http://code.google.com/p/googletest/wiki/Primer#String_Comparison) */

    DBClientBase* connection = getMongoConnection();

    /* entities collection */
    BSONObj ent;
    std::vector<BSONElement> attrs;
    ASSERT_EQ(2, connection->count(ENTITIES_COLL, BSONObj()));

    ent = connection->findOne(ENTITIES_COLL, BSON("_id.id" << "E1" << "_id.type" << "T1"));
    EXPECT_STREQ("E1", C_STR_FIELD(ent.getObjectField("_id"), "id"));
    EXPECT_STREQ("T1", C_STR_FIELD(ent.getObjectField("_id"), "type"));
    EXPECT_FALSE(ent.hasField("creDate"));
    EXPECT_FALSE(ent.hasField("modDate"));
    attrs = ent.getField("attrs").Array();
    ASSERT_EQ(2, attrs.size());
    BSONObj a1 = getAttr(attrs, "A1", "TA1");
    EXPECT_STREQ("A1", C_STR_FIELD(a1, "name"));
    EXPECT_STREQ("TA1", C_STR_FIELD(a1, "type"));
    EXPECT_STREQ("-5, 2", C_STR_FIELD(a1, "value"));
    EXPECT_FALSE(a1.hasField("creDate"));
    EXPECT_FALSE(a1.hasField("modDate"));
    BSONObj a2 = getAttr(attrs, "A2", "TA2");
    EXPECT_STREQ("A2", C_STR_FIELD(a2, "name"));
    EXPECT_STREQ("TA2", C_STR_FIELD(a2, "type"));
    EXPECT_STREQ("noloc", C_STR_FIELD(a2, "value"));
    EXPECT_FALSE(a2.hasField("creDate"));
    EXPECT_FALSE(a2.hasField("modDate"));
    EXPECT_STREQ("A1", ent.getObjectField("location").getStringField("attrName"));
    EXPECT_EQ(2, coordX(ent));
    EXPECT_EQ(-5, coordY(ent));

    ent = connection->findOne(ENTITIES_COLL, BSON("_id.id" << "E2" << "_id.type" << "T2"));
    EXPECT_STREQ("E2", C_STR_FIELD(ent.getObjectField("_id"), "id"));
    EXPECT_STREQ("T2", C_STR_FIELD(ent.getObjectField("_id"), "type"));
    EXPECT_EQ(1360232700, ent.getIntField("modDate"));
    EXPECT_FALSE(ent.hasField("creDate"));
    attrs = ent.getField("attrs").Array();
    ASSERT_EQ(2, attrs.size());
    a2 = getAttr(attrs, "A2", "TA2");
    BSONObj a5 = getAttr(attrs, "A5", "TA5");
    EXPECT_STREQ("A2", C_STR_FIELD(a2, "name"));
    EXPECT_STREQ("TA2", C_STR_FIELD(a2, "type"));
    EXPECT_STREQ("Y", C_STR_FIELD(a2, "value"));
    EXPECT_FALSE(a2.hasField("creDate"));
    EXPECT_FALSE(a2.hasField("modDate"));
    EXPECT_STREQ("A5", C_STR_FIELD(a5, "name"));
    EXPECT_STREQ("TA5", C_STR_FIELD(a5, "type"));
    EXPECT_STREQ("8, -9", C_STR_FIELD(a5, "value"));
    EXPECT_EQ(1360232700, a5.getIntField("creDate"));
    EXPECT_EQ(1360232700, a5.getIntField("modDate"));
    EXPECT_STREQ("A5", ent.getObjectField("location").getStringField("attrName"));
    EXPECT_EQ(-9, coordX(ent));
    EXPECT_EQ(8, coordY(ent));

    /* Release connection */
    mongoDisconnect();

    utExit();
}

/* ****************************************************************************
*
*  - updateLocAttribute
*/
TEST(mongoUpdateContextGeoRequest, updateLocAttribute)
{
    utInit();

    HttpStatusCode         ms;
    UpdateContextRequest   req;
    UpdateContextResponse  res;

    /* Prepare database */
    prepareDatabase();

    /* Forge the request (from "inside" to "outside") */
    ContextElement ce;
    ce.entityId.fill("E1", "T1", "false");
    ContextAttribute ca("A1", "TA1", "2, -4");
    ce.contextAttributeVector.push_back(&ca);
    req.contextElementVector.push_back(&ce);
    req.updateActionType.set("UPDATE");

    /* Invoke the function in mongoBackend library */
    servicePathVector.clear();   
    ms = mongoUpdateContext(&req, &res, "", servicePathVector, uriParams);

    /* Check response is as expected */
    EXPECT_EQ(SccOk, ms);

    EXPECT_EQ(0, res.errorCode.code);
    EXPECT_EQ(0, res.errorCode.reasonPhrase.size());
    EXPECT_EQ(0, res.errorCode.details.size());

    ASSERT_EQ(1, res.contextElementResponseVector.size());
    /* Context Element response # 1 */
    EXPECT_EQ("E1", RES_CER(0).entityId.id);
    EXPECT_EQ("T1", RES_CER(0).entityId.type);
    EXPECT_EQ("false", RES_CER(0).entityId.isPattern);
    ASSERT_EQ(1, RES_CER(0).contextAttributeVector.size());
    EXPECT_EQ("A1", RES_CER_ATTR(0, 0)->name);
    EXPECT_EQ("TA1", RES_CER_ATTR(0, 0)->type);
    EXPECT_EQ(0, RES_CER_ATTR(0, 0)->value.size());
    ASSERT_EQ(0, RES_CER_ATTR(0, 0)->metadataVector.size());
    EXPECT_EQ(SccOk, RES_CER_STATUS(0).code);
    EXPECT_EQ("OK", RES_CER_STATUS(0).reasonPhrase);
    EXPECT_EQ(0, RES_CER_STATUS(0).details.size());

    /* Check that every involved collection at MongoDB is as expected */
    /* Note we are using EXPECT_STREQ() for some cases, as Mongo Driver returns const char*, not string
     * objects (see http://code.google.com/p/googletest/wiki/Primer#String_Comparison) */

    DBClientBase* connection = getMongoConnection();

    /* entities collection */
    BSONObj ent;
    std::vector<BSONElement> attrs;
    ASSERT_EQ(2, connection->count(ENTITIES_COLL, BSONObj()));

    ent = connection->findOne(ENTITIES_COLL, BSON("_id.id" << "E1" << "_id.type" << "T1"));
    EXPECT_STREQ("E1", C_STR_FIELD(ent.getObjectField("_id"), "id"));
    EXPECT_STREQ("T1", C_STR_FIELD(ent.getObjectField("_id"), "type"));
    EXPECT_FALSE(ent.hasField("creDate"));
    EXPECT_EQ(1360232700, ent.getIntField("modDate"));
    attrs = ent.getField("attrs").Array();
    ASSERT_EQ(2, attrs.size());
    BSONObj a1 = getAttr(attrs, "A1", "TA1");
    EXPECT_STREQ("A1", C_STR_FIELD(a1, "name"));
    EXPECT_STREQ("TA1", C_STR_FIELD(a1, "type"));
    EXPECT_STREQ("2, -4", C_STR_FIELD(a1, "value"));
    EXPECT_FALSE(a1.hasField("creDate"));
    EXPECT_EQ(1360232700, a1.getIntField("modDate"));
    BSONObj a2 = getAttr(attrs, "A2", "TA2");
    EXPECT_STREQ("A2", C_STR_FIELD(a2, "name"));
    EXPECT_STREQ("TA2", C_STR_FIELD(a2, "type"));
    EXPECT_STREQ("noloc", C_STR_FIELD(a2, "value"));
    EXPECT_FALSE(a2.hasField("creDate"));
    EXPECT_FALSE(a2.hasField("modDate"));
    EXPECT_STREQ("A1", ent.getObjectField("location").getStringField("attrName"));
    EXPECT_EQ(-4, coordX(ent));
    EXPECT_EQ(2, coordY(ent));

    ent = connection->findOne(ENTITIES_COLL, BSON("_id.id" << "E2" << "_id.type" << "T2"));
    EXPECT_STREQ("E2", C_STR_FIELD(ent.getObjectField("_id"), "id"));
    EXPECT_STREQ("T2", C_STR_FIELD(ent.getObjectField("_id"), "type"));
    EXPECT_FALSE(ent.hasField("modDate"));
    EXPECT_FALSE(ent.hasField("creDate"));
    attrs = ent.getField("attrs").Array();
    ASSERT_EQ(1, attrs.size());
    a2 = getAttr(attrs, "A2", "TA2");
    EXPECT_STREQ("A2", C_STR_FIELD(a2, "name"));
    EXPECT_STREQ("TA2", C_STR_FIELD(a2, "type"));
    EXPECT_STREQ("Y", C_STR_FIELD(a2, "value"));
    EXPECT_FALSE(a2.hasField("creDate"));
    EXPECT_FALSE(a2.hasField("modDate"));
    EXPECT_FALSE(ent.hasField("location"));

    /* Release connection */
    mongoDisconnect();

    utExit();
}

/* ****************************************************************************
*
*  - deleteLocAttribute
*/
TEST(mongoUpdateContextGeoRequest, deleteLocAttribute)
{
    utInit();

    HttpStatusCode         ms;
    UpdateContextRequest   req;
    UpdateContextResponse  res;

    /* Prepare database */
    prepareDatabase();

    /* Forge the request (from "inside" to "outside") */
    ContextElement ce;
    ce.entityId.fill("E1", "T1", "false");
    ContextAttribute ca("A1", "TA1", "");
    ce.contextAttributeVector.push_back(&ca);
    req.contextElementVector.push_back(&ce);
    req.updateActionType.set("DELETE");

    /* Invoke the function in mongoBackend library */
    servicePathVector.clear();   
    ms = mongoUpdateContext(&req, &res, "", servicePathVector, uriParams);

    /* Check response is as expected */
    EXPECT_EQ(SccOk, ms);

    EXPECT_EQ(0, res.errorCode.code);
    EXPECT_EQ(0, res.errorCode.reasonPhrase.size());
    EXPECT_EQ(0, res.errorCode.details.size());

    ASSERT_EQ(1, res.contextElementResponseVector.size());
    /* Context Element response # 1 */
    EXPECT_EQ("E1", RES_CER(0).entityId.id);
    EXPECT_EQ("T1", RES_CER(0).entityId.type);
    EXPECT_EQ("false", RES_CER(0).entityId.isPattern);
    ASSERT_EQ(1, RES_CER(0).contextAttributeVector.size());
    EXPECT_EQ("A1", RES_CER_ATTR(0, 0)->name);
    EXPECT_EQ("TA1", RES_CER_ATTR(0, 0)->type);
    EXPECT_EQ(0, RES_CER_ATTR(0, 0)->value.size());
    ASSERT_EQ(0, RES_CER_ATTR(0, 0)->metadataVector.size());
    EXPECT_EQ(SccOk, RES_CER_STATUS(0).code);
    EXPECT_EQ("OK", RES_CER_STATUS(0).reasonPhrase);
    EXPECT_EQ(0, RES_CER_STATUS(0).details.size());

    /* Check that every involved collection at MongoDB is as expected */
    /* Note we are using EXPECT_STREQ() for some cases, as Mongo Driver returns const char*, not string
     * objects (see http://code.google.com/p/googletest/wiki/Primer#String_Comparison) */

    DBClientBase* connection = getMongoConnection();

    /* entities collection */
    BSONObj ent;
    std::vector<BSONElement> attrs;
    ASSERT_EQ(2, connection->count(ENTITIES_COLL, BSONObj()));

    ent = connection->findOne(ENTITIES_COLL, BSON("_id.id" << "E1" << "_id.type" << "T1"));
    EXPECT_STREQ("E1", C_STR_FIELD(ent.getObjectField("_id"), "id"));
    EXPECT_STREQ("T1", C_STR_FIELD(ent.getObjectField("_id"), "type"));
    EXPECT_FALSE(ent.hasField("creDate"));
    EXPECT_EQ(1360232700, ent.getIntField("modDate"));
    attrs = ent.getField("attrs").Array();
    ASSERT_EQ(1, attrs.size());
    BSONObj a2 = getAttr(attrs, "A2", "TA2");
    EXPECT_STREQ("A2", C_STR_FIELD(a2, "name"));
    EXPECT_STREQ("TA2", C_STR_FIELD(a2, "type"));
    EXPECT_STREQ("noloc", C_STR_FIELD(a2, "value"));
    EXPECT_FALSE(a2.hasField("creDate"));
    EXPECT_FALSE(a2.hasField("modDate"));
    EXPECT_FALSE(ent.hasField("location"));

    ent = connection->findOne(ENTITIES_COLL, BSON("_id.id" << "E2" << "_id.type" << "T2"));
    EXPECT_STREQ("E2", C_STR_FIELD(ent.getObjectField("_id"), "id"));
    EXPECT_STREQ("T2", C_STR_FIELD(ent.getObjectField("_id"), "type"));
    EXPECT_FALSE(ent.hasField("modDate"));
    EXPECT_FALSE(ent.hasField("creDate"));
    attrs = ent.getField("attrs").Array();
    ASSERT_EQ(1, attrs.size());
    a2 = getAttr(attrs, "A2", "TA2");
    EXPECT_STREQ("A2", C_STR_FIELD(a2, "name"));
    EXPECT_STREQ("TA2", C_STR_FIELD(a2, "type"));
    EXPECT_STREQ("Y", C_STR_FIELD(a2, "value"));
    EXPECT_FALSE(a2.hasField("creDate"));
    EXPECT_FALSE(a2.hasField("modDate"));
    EXPECT_FALSE(ent.hasField("location"));

    /* Release connection */
    mongoDisconnect();

    utExit();
}

/* ****************************************************************************
*
*  - newEntityTwoLocAttributesFail
*/
TEST(mongoUpdateContextGeoRequest, newEntityTwoLocAttributesFail)
{
    utInit();

    HttpStatusCode         ms;
    UpdateContextRequest   req;
    UpdateContextResponse  res;

    /* Prepare database */
    prepareDatabase();

    /* Forge the request (from "inside" to "outside") */
    ContextElement ce;
    ContextAttribute ca1, ca2;
    Metadata md1, md2;
    ce.entityId.fill("E3", "T3", "false");
    ca1 = ContextAttribute("A1", "TA1", "2, -4");
    md1 = Metadata("location", "string", "WSG84");
    ca1.metadataVector.push_back(&md1);
    ce.contextAttributeVector.push_back(&ca1);
    ca2 = ContextAttribute("A2", "TA2", "5, -6");
    md2 = Metadata("location", "string", "WSG84");
    ca2.metadataVector.push_back(&md2);
    ce.contextAttributeVector.push_back(&ca2);
    req.contextElementVector.push_back(&ce);
    req.updateActionType.set("APPEND");

    /* Invoke the function in mongoBackend library */
    servicePathVector.clear();    
    ms = mongoUpdateContext(&req, &res, "", servicePathVector, uriParams);

    /* Check response is as expected */
    EXPECT_EQ(SccOk, ms);

    EXPECT_EQ(0, res.errorCode.code);
    EXPECT_EQ(0, res.errorCode.reasonPhrase.size());
    EXPECT_EQ(0, res.errorCode.details.size());

    ASSERT_EQ(1, res.contextElementResponseVector.size());
    /* Context Element response # 1 */
    EXPECT_EQ("E3", RES_CER(0).entityId.id);
    EXPECT_EQ("T3", RES_CER(0).entityId.type);
    EXPECT_EQ("false", RES_CER(0).entityId.isPattern);
    ASSERT_EQ(2, RES_CER(0).contextAttributeVector.size());
    EXPECT_EQ("A1", RES_CER_ATTR(0, 0)->name);
    EXPECT_EQ("TA1", RES_CER_ATTR(0, 0)->type);
    EXPECT_EQ(0, RES_CER_ATTR(0, 0)->value.size());
    ASSERT_EQ(1, RES_CER_ATTR(0, 0)->metadataVector.size());
    EXPECT_EQ("location", RES_CER_ATTR(0, 0)->metadataVector.get(0)->name);
    EXPECT_EQ("string", RES_CER_ATTR(0, 0)->metadataVector.get(0)->type);
    EXPECT_EQ("WSG84", RES_CER_ATTR(0, 0)->metadataVector.get(0)->value);
    EXPECT_EQ("A2", RES_CER_ATTR(0, 1)->name);
    EXPECT_EQ("TA2", RES_CER_ATTR(0, 1)->type);
    EXPECT_EQ(0, RES_CER_ATTR(0, 1)->value.size());
    ASSERT_EQ(1, RES_CER_ATTR(0, 1)->metadataVector.size());
    EXPECT_EQ("location", RES_CER_ATTR(0, 1)->metadataVector.get(0)->name);
    EXPECT_EQ("string", RES_CER_ATTR(0, 1)->metadataVector.get(0)->type);
    EXPECT_EQ("WSG84", RES_CER_ATTR(0, 1)->metadataVector.get(0)->value);
    EXPECT_EQ(SccInvalidParameter, RES_CER_STATUS(0).code);
    EXPECT_EQ("request parameter is invalid/not allowed", RES_CER_STATUS(0).reasonPhrase);
    EXPECT_EQ("You cannot use more than one location attribute when creating an entity (see Orion user manual)", RES_CER_STATUS(0).details);

    /* Check that every involved collection at MongoDB is as expected */
    /* Note we are using EXPECT_STREQ() for some cases, as Mongo Driver returns const char*, not string
     * objects (see http://code.google.com/p/googletest/wiki/Primer#String_Comparison) */

    DBClientBase* connection = getMongoConnection();

    /* entities collection */
    BSONObj ent;
    std::vector<BSONElement> attrs;
    ASSERT_EQ(2, connection->count(ENTITIES_COLL, BSONObj()));

    ent = connection->findOne(ENTITIES_COLL, BSON("_id.id" << "E1" << "_id.type" << "T1"));
    EXPECT_STREQ("E1", C_STR_FIELD(ent.getObjectField("_id"), "id"));
    EXPECT_STREQ("T1", C_STR_FIELD(ent.getObjectField("_id"), "type"));
    EXPECT_FALSE(ent.hasField("creDate"));
    EXPECT_FALSE(ent.hasField("modDate"));
    attrs = ent.getField("attrs").Array();
    ASSERT_EQ(2, attrs.size());
    BSONObj a1 = getAttr(attrs, "A1", "TA1");
    EXPECT_STREQ("A1", C_STR_FIELD(a1, "name"));
    EXPECT_STREQ("TA1", C_STR_FIELD(a1, "type"));
    EXPECT_STREQ("-5, 2", C_STR_FIELD(a1, "value"));
    EXPECT_FALSE(a1.hasField("creDate"));
    EXPECT_FALSE(a1.hasField("modDate"));
    BSONObj a2 = getAttr(attrs, "A2", "TA2");
    EXPECT_STREQ("A2", C_STR_FIELD(a2, "name"));
    EXPECT_STREQ("TA2", C_STR_FIELD(a2, "type"));
    EXPECT_STREQ("noloc", C_STR_FIELD(a2, "value"));
    EXPECT_FALSE(a2.hasField("creDate"));
    EXPECT_FALSE(a2.hasField("modDate"));
    EXPECT_STREQ("A1", ent.getObjectField("location").getStringField("attrName"));
    EXPECT_EQ(2, coordX(ent));
    EXPECT_EQ(-5, coordY(ent));

    ent = connection->findOne(ENTITIES_COLL, BSON("_id.id" << "E2" << "_id.type" << "T2"));
    EXPECT_STREQ("E2", C_STR_FIELD(ent.getObjectField("_id"), "id"));
    EXPECT_STREQ("T2", C_STR_FIELD(ent.getObjectField("_id"), "type"));
    EXPECT_FALSE(ent.hasField("modDate"));
    EXPECT_FALSE(ent.hasField("creDate"));
    attrs = ent.getField("attrs").Array();
    ASSERT_EQ(1, attrs.size());
    a2 = getAttr(attrs, "A2", "TA2");
    EXPECT_STREQ("A2", C_STR_FIELD(a2, "name"));
    EXPECT_STREQ("TA2", C_STR_FIELD(a2, "type"));
    EXPECT_STREQ("Y", C_STR_FIELD(a2, "value"));
    EXPECT_FALSE(a2.hasField("creDate"));
    EXPECT_FALSE(a2.hasField("modDate"));
    EXPECT_FALSE(ent.hasField("location"));

    /* Release connection */
    mongoDisconnect();

    utExit();
}

/* ****************************************************************************
*
*  - newEntityWrongCoordinatesFormatFail
*/
TEST(mongoUpdateContextGeoRequest, newEntityWrongCoordinatesFormatFail)
{
    utInit();

    HttpStatusCode         ms;
    UpdateContextRequest   req;
    UpdateContextResponse  res;

    /* Prepare database */
    prepareDatabase();

    /* Forge the request (from "inside" to "outside") */
    ContextElement ce;
    ce.entityId.fill("E3", "T3", "false");
    ContextAttribute ca("A1", "TA1", "invalid");
    Metadata md("location", "string", "WSG84");
    ca.metadataVector.push_back(&md);
    ce.contextAttributeVector.push_back(&ca);
    req.contextElementVector.push_back(&ce);
    req.updateActionType.set("APPEND");

    /* Invoke the function in mongoBackend library */
    servicePathVector.clear();   
    ms = mongoUpdateContext(&req, &res, "", servicePathVector, uriParams);

    /* Check response is as expected */
    EXPECT_EQ(SccOk, ms);

    EXPECT_EQ(0, res.errorCode.code);
    EXPECT_EQ(0, res.errorCode.reasonPhrase.size());
    EXPECT_EQ(0, res.errorCode.details.size());

    ASSERT_EQ(1, res.contextElementResponseVector.size());
    /* Context Element response # 1 */
    EXPECT_EQ("E3", RES_CER(0).entityId.id);
    EXPECT_EQ("T3", RES_CER(0).entityId.type);
    EXPECT_EQ("false", RES_CER(0).entityId.isPattern);
    ASSERT_EQ(1, RES_CER(0).contextAttributeVector.size());
    EXPECT_EQ("A1", RES_CER_ATTR(0, 0)->name);
    EXPECT_EQ("TA1", RES_CER_ATTR(0, 0)->type);
    EXPECT_EQ(0, RES_CER_ATTR(0, 0)->value.size());
    ASSERT_EQ(1, RES_CER_ATTR(0, 0)->metadataVector.size());
    EXPECT_EQ("location", RES_CER_ATTR(0, 0)->metadataVector.get(0)->name);
    EXPECT_EQ("string", RES_CER_ATTR(0, 0)->metadataVector.get(0)->type);
    EXPECT_EQ("WSG84", RES_CER_ATTR(0, 0)->metadataVector.get(0)->value);
    EXPECT_EQ(SccInvalidParameter, RES_CER_STATUS(0).code);
    EXPECT_EQ("request parameter is invalid/not allowed", RES_CER_STATUS(0).reasonPhrase);
    EXPECT_EQ("coordinate format error (see Orion user manual): invalid", RES_CER_STATUS(0).details);

    /* Check that every involved collection at MongoDB is as expected */
    /* Note we are using EXPECT_STREQ() for some cases, as Mongo Driver returns const char*, not string
     * objects (see http://code.google.com/p/googletest/wiki/Primer#String_Comparison) */

    DBClientBase* connection = getMongoConnection();

    /* entities collection */
    BSONObj ent;
    std::vector<BSONElement> attrs;
    ASSERT_EQ(2, connection->count(ENTITIES_COLL, BSONObj()));

    ent = connection->findOne(ENTITIES_COLL, BSON("_id.id" << "E1" << "_id.type" << "T1"));
    EXPECT_STREQ("E1", C_STR_FIELD(ent.getObjectField("_id"), "id"));
    EXPECT_STREQ("T1", C_STR_FIELD(ent.getObjectField("_id"), "type"));
    EXPECT_FALSE(ent.hasField("creDate"));
    EXPECT_FALSE(ent.hasField("modDate"));
    attrs = ent.getField("attrs").Array();
    ASSERT_EQ(2, attrs.size());
    BSONObj a1 = getAttr(attrs, "A1", "TA1");
    EXPECT_STREQ("A1", C_STR_FIELD(a1, "name"));
    EXPECT_STREQ("TA1", C_STR_FIELD(a1, "type"));
    EXPECT_STREQ("-5, 2", C_STR_FIELD(a1, "value"));
    EXPECT_FALSE(a1.hasField("creDate"));
    EXPECT_FALSE(a1.hasField("modDate"));
    BSONObj a2 = getAttr(attrs, "A2", "TA2");
    EXPECT_STREQ("A2", C_STR_FIELD(a2, "name"));
    EXPECT_STREQ("TA2", C_STR_FIELD(a2, "type"));
    EXPECT_STREQ("noloc", C_STR_FIELD(a2, "value"));
    EXPECT_FALSE(a2.hasField("creDate"));
    EXPECT_FALSE(a2.hasField("modDate"));
    EXPECT_STREQ("A1", ent.getObjectField("location").getStringField("attrName"));
    EXPECT_EQ(2, coordX(ent));
    EXPECT_EQ(-5, coordY(ent));

    ent = connection->findOne(ENTITIES_COLL, BSON("_id.id" << "E2" << "_id.type" << "T2"));
    EXPECT_STREQ("E2", C_STR_FIELD(ent.getObjectField("_id"), "id"));
    EXPECT_STREQ("T2", C_STR_FIELD(ent.getObjectField("_id"), "type"));
    EXPECT_FALSE(ent.hasField("modDate"));
    EXPECT_FALSE(ent.hasField("creDate"));
    attrs = ent.getField("attrs").Array();
    ASSERT_EQ(1, attrs.size());
    a2 = getAttr(attrs, "A2", "TA2");
    EXPECT_STREQ("A2", C_STR_FIELD(a2, "name"));
    EXPECT_STREQ("TA2", C_STR_FIELD(a2, "type"));
    EXPECT_STREQ("Y", C_STR_FIELD(a2, "value"));
    EXPECT_FALSE(a2.hasField("creDate"));
    EXPECT_FALSE(a2.hasField("modDate"));
    EXPECT_FALSE(ent.hasField("location"));

    /* Release connection */
    mongoDisconnect();

    utExit();
}

/* ****************************************************************************
*
*  - newEntityNotSupportedLocationFail
*/
TEST(mongoUpdateContextGeoRequest, newEntityNotSupportedLocationFail)
{
    utInit();

    HttpStatusCode         ms;
    UpdateContextRequest   req;
    UpdateContextResponse  res;

    /* Prepare database */
    prepareDatabase();

    /* Forge the request (from "inside" to "outside") */
    ContextElement ce;
    ce.entityId.fill("E3", "T3", "false");
    ContextAttribute ca("A1", "TA1", "2, 4");
    Metadata md("location", "string", "gurugu");
    ca.metadataVector.push_back(&md);
    ce.contextAttributeVector.push_back(&ca);
    req.contextElementVector.push_back(&ce);
    req.updateActionType.set("APPEND");

    /* Invoke the function in mongoBackend library */
    servicePathVector.clear();    
    ms = mongoUpdateContext(&req, &res, "", servicePathVector, uriParams);

    /* Check response is as expected */
    EXPECT_EQ(SccOk, ms);

    EXPECT_EQ(0, res.errorCode.code);
    EXPECT_EQ(0, res.errorCode.reasonPhrase.size());
    EXPECT_EQ(0, res.errorCode.details.size());

    ASSERT_EQ(1, res.contextElementResponseVector.size());
    /* Context Element response # 1 */
    EXPECT_EQ("E3", RES_CER(0).entityId.id);
    EXPECT_EQ("T3", RES_CER(0).entityId.type);
    EXPECT_EQ("false", RES_CER(0).entityId.isPattern);
    ASSERT_EQ(1, RES_CER(0).contextAttributeVector.size());
    EXPECT_EQ("A1", RES_CER_ATTR(0, 0)->name);
    EXPECT_EQ("TA1", RES_CER_ATTR(0, 0)->type);
    EXPECT_EQ(0, RES_CER_ATTR(0, 0)->value.size());
    ASSERT_EQ(1, RES_CER_ATTR(0, 0)->metadataVector.size());
    EXPECT_EQ("location", RES_CER_ATTR(0, 0)->metadataVector.get(0)->name);
    EXPECT_EQ("string", RES_CER_ATTR(0, 0)->metadataVector.get(0)->type);
    EXPECT_EQ("gurugu", RES_CER_ATTR(0, 0)->metadataVector.get(0)->value);
    EXPECT_EQ(SccInvalidParameter, RES_CER_STATUS(0).code);
    EXPECT_EQ("request parameter is invalid/not allowed", RES_CER_STATUS(0).reasonPhrase);
    EXPECT_EQ("only WSG84 are supported, found: gurugu", RES_CER_STATUS(0).details);

    /* Check that every involved collection at MongoDB is as expected */
    /* Note we are using EXPECT_STREQ() for some cases, as Mongo Driver returns const char*, not string
     * objects (see http://code.google.com/p/googletest/wiki/Primer#String_Comparison) */

    DBClientBase* connection = getMongoConnection();

    /* entities collection */
    BSONObj ent;
    std::vector<BSONElement> attrs;
    ASSERT_EQ(2, connection->count(ENTITIES_COLL, BSONObj()));

    ent = connection->findOne(ENTITIES_COLL, BSON("_id.id" << "E1" << "_id.type" << "T1"));
    EXPECT_STREQ("E1", C_STR_FIELD(ent.getObjectField("_id"), "id"));
    EXPECT_STREQ("T1", C_STR_FIELD(ent.getObjectField("_id"), "type"));
    EXPECT_FALSE(ent.hasField("creDate"));
    EXPECT_FALSE(ent.hasField("modDate"));
    attrs = ent.getField("attrs").Array();
    ASSERT_EQ(2, attrs.size());
    BSONObj a1 = getAttr(attrs, "A1", "TA1");
    EXPECT_STREQ("A1", C_STR_FIELD(a1, "name"));
    EXPECT_STREQ("TA1", C_STR_FIELD(a1, "type"));
    EXPECT_STREQ("-5, 2", C_STR_FIELD(a1, "value"));
    EXPECT_FALSE(a1.hasField("creDate"));
    EXPECT_FALSE(a1.hasField("modDate"));
    BSONObj a2 = getAttr(attrs, "A2", "TA2");
    EXPECT_STREQ("A2", C_STR_FIELD(a2, "name"));
    EXPECT_STREQ("TA2", C_STR_FIELD(a2, "type"));
    EXPECT_STREQ("noloc", C_STR_FIELD(a2, "value"));
    EXPECT_FALSE(a2.hasField("creDate"));
    EXPECT_FALSE(a2.hasField("modDate"));
    EXPECT_STREQ("A1", ent.getObjectField("location").getStringField("attrName"));
    EXPECT_EQ(2, coordX(ent));
    EXPECT_EQ(-5, coordY(ent));

    ent = connection->findOne(ENTITIES_COLL, BSON("_id.id" << "E2" << "_id.type" << "T2"));
    EXPECT_STREQ("E2", C_STR_FIELD(ent.getObjectField("_id"), "id"));
    EXPECT_STREQ("T2", C_STR_FIELD(ent.getObjectField("_id"), "type"));
    EXPECT_FALSE(ent.hasField("modDate"));
    EXPECT_FALSE(ent.hasField("creDate"));
    attrs = ent.getField("attrs").Array();
    ASSERT_EQ(1, attrs.size());
    a2 = getAttr(attrs, "A2", "TA2");
    EXPECT_STREQ("A2", C_STR_FIELD(a2, "name"));
    EXPECT_STREQ("TA2", C_STR_FIELD(a2, "type"));
    EXPECT_STREQ("Y", C_STR_FIELD(a2, "value"));
    EXPECT_FALSE(a2.hasField("creDate"));
    EXPECT_FALSE(a2.hasField("modDate"));
    EXPECT_FALSE(ent.hasField("location"));

    /* Release connection */
    mongoDisconnect();

    utExit();
}


/* ****************************************************************************
*
*  - appendAdditionalLocAttributeFail
*/
TEST(mongoUpdateContextGeoRequest, appendAdditionalLocAttributeFail)
{
    utInit();

    HttpStatusCode         ms;
    UpdateContextRequest   req;
    UpdateContextResponse  res;

    /* Prepare database */
    prepareDatabase();

    /* Forge the request (from "inside" to "outside") */
    ContextElement ce;
    ce.entityId.fill("E1", "T1", "false");
    ContextAttribute ca("A5", "TA5", "2, 4");
    Metadata md("location", "string", "WSG84");
    ca.metadataVector.push_back(&md);
    ce.contextAttributeVector.push_back(&ca);
    req.contextElementVector.push_back(&ce);
    req.updateActionType.set("APPEND");

    /* Invoke the function in mongoBackend library */
    servicePathVector.clear();    
    ms = mongoUpdateContext(&req, &res, "", servicePathVector, uriParams);

    /* Check response is as expected */
    EXPECT_EQ(SccOk, ms);

    EXPECT_EQ(0, res.errorCode.code);
    EXPECT_EQ(0, res.errorCode.reasonPhrase.size());
    EXPECT_EQ(0, res.errorCode.details.size());

    ASSERT_EQ(1, res.contextElementResponseVector.size());
    /* Context Element response # 1 */
    EXPECT_EQ("E1", RES_CER(0).entityId.id);
    EXPECT_EQ("T1", RES_CER(0).entityId.type);
    EXPECT_EQ("false", RES_CER(0).entityId.isPattern);
    ASSERT_EQ(1, RES_CER(0).contextAttributeVector.size());
    EXPECT_EQ("A5", RES_CER_ATTR(0, 0)->name);
    EXPECT_EQ("TA5", RES_CER_ATTR(0, 0)->type);
    EXPECT_EQ(0, RES_CER_ATTR(0, 0)->value.size());
    ASSERT_EQ(1, RES_CER_ATTR(0, 0)->metadataVector.size());
    EXPECT_EQ("location", RES_CER_ATTR(0, 0)->metadataVector.get(0)->name);
    EXPECT_EQ("string", RES_CER_ATTR(0, 0)->metadataVector.get(0)->type);
    EXPECT_EQ("WSG84", RES_CER_ATTR(0, 0)->metadataVector.get(0)->value);
    EXPECT_EQ(SccInvalidParameter, RES_CER_STATUS(0).code);
    EXPECT_EQ("request parameter is invalid/not allowed", RES_CER_STATUS(0).reasonPhrase);
    EXPECT_EQ("action: APPEND - entity: (E1, T1) - offending attribute: A5 - attemp to define a location attribute (A5) when another one has been previously defined (A1)", RES_CER_STATUS(0).details);

    /* Check that every involved collection at MongoDB is as expected */
    /* Note we are using EXPECT_STREQ() for some cases, as Mongo Driver returns const char*, not string
     * objects (see http://code.google.com/p/googletest/wiki/Primer#String_Comparison) */

    DBClientBase* connection = getMongoConnection();

    /* entities collection */
    BSONObj ent;
    std::vector<BSONElement> attrs;
    ASSERT_EQ(2, connection->count(ENTITIES_COLL, BSONObj()));

    ent = connection->findOne(ENTITIES_COLL, BSON("_id.id" << "E1" << "_id.type" << "T1"));
    EXPECT_STREQ("E1", C_STR_FIELD(ent.getObjectField("_id"), "id"));
    EXPECT_STREQ("T1", C_STR_FIELD(ent.getObjectField("_id"), "type"));
    EXPECT_FALSE(ent.hasField("creDate"));
    EXPECT_FALSE(ent.hasField("modDate"));
    attrs = ent.getField("attrs").Array();
    ASSERT_EQ(2, attrs.size());
    BSONObj a1 = getAttr(attrs, "A1", "TA1");
    EXPECT_STREQ("A1", C_STR_FIELD(a1, "name"));
    EXPECT_STREQ("TA1", C_STR_FIELD(a1, "type"));
    EXPECT_STREQ("-5, 2", C_STR_FIELD(a1, "value"));
    EXPECT_FALSE(a1.hasField("creDate"));
    EXPECT_FALSE(a1.hasField("modDate"));
    BSONObj a2 = getAttr(attrs, "A2", "TA2");
    EXPECT_STREQ("A2", C_STR_FIELD(a2, "name"));
    EXPECT_STREQ("TA2", C_STR_FIELD(a2, "type"));
    EXPECT_STREQ("noloc", C_STR_FIELD(a2, "value"));
    EXPECT_FALSE(a2.hasField("creDate"));
    EXPECT_FALSE(a2.hasField("modDate"));
    EXPECT_STREQ("A1", ent.getObjectField("location").getStringField("attrName"));
    EXPECT_EQ(2, coordX(ent));
    EXPECT_EQ(-5, coordY(ent));

    ent = connection->findOne(ENTITIES_COLL, BSON("_id.id" << "E2" << "_id.type" << "T2"));
    EXPECT_STREQ("E2", C_STR_FIELD(ent.getObjectField("_id"), "id"));
    EXPECT_STREQ("T2", C_STR_FIELD(ent.getObjectField("_id"), "type"));
    EXPECT_FALSE(ent.hasField("modDate"));
    EXPECT_FALSE(ent.hasField("creDate"));
    attrs = ent.getField("attrs").Array();
    ASSERT_EQ(1, attrs.size());
    a2 = getAttr(attrs, "A2", "TA2");
    EXPECT_STREQ("A2", C_STR_FIELD(a2, "name"));
    EXPECT_STREQ("TA2", C_STR_FIELD(a2, "type"));
    EXPECT_STREQ("Y", C_STR_FIELD(a2, "value"));
    EXPECT_FALSE(a2.hasField("creDate"));
    EXPECT_FALSE(a2.hasField("modDate"));
    EXPECT_FALSE(ent.hasField("location"));

    /* Release connection */
    mongoDisconnect();

    utExit();
}

/* ****************************************************************************
*
*  - appendWrongCoordinatesFormatFail
*/
TEST(mongoUpdateContextGeoRequest, appendWrongCoordinatesFormatFail)
{
    utInit();

    HttpStatusCode         ms;
    UpdateContextRequest   req;
    UpdateContextResponse  res;

    /* Prepare database */
    prepareDatabase();

    /* Forge the request (from "inside" to "outside") */
    ContextElement ce;
    ce.entityId.fill("E2", "T2", "false");
    ContextAttribute ca("A5", "TA5", "erroneous");
    Metadata md("location", "string", "WSG84");
    ca.metadataVector.push_back(&md);
    ce.contextAttributeVector.push_back(&ca);
    req.contextElementVector.push_back(&ce);
    req.updateActionType.set("APPEND");

    /* Invoke the function in mongoBackend library */
    servicePathVector.clear();   
    ms = mongoUpdateContext(&req, &res, "", servicePathVector, uriParams);

    /* Check response is as expected */
    EXPECT_EQ(SccOk, ms);

    EXPECT_EQ(0, res.errorCode.code);
    EXPECT_EQ(0, res.errorCode.reasonPhrase.size());
    EXPECT_EQ(0, res.errorCode.details.size());

    ASSERT_EQ(1, res.contextElementResponseVector.size());
    /* Context Element response # 1 */
    EXPECT_EQ("E2", RES_CER(0).entityId.id);
    EXPECT_EQ("T2", RES_CER(0).entityId.type);
    EXPECT_EQ("false", RES_CER(0).entityId.isPattern);
    ASSERT_EQ(1, RES_CER(0).contextAttributeVector.size());
    EXPECT_EQ("A5", RES_CER_ATTR(0, 0)->name);
    EXPECT_EQ("TA5", RES_CER_ATTR(0, 0)->type);
    EXPECT_EQ(0, RES_CER_ATTR(0, 0)->value.size());
    ASSERT_EQ(1, RES_CER_ATTR(0, 0)->metadataVector.size());
    EXPECT_EQ("location", RES_CER_ATTR(0, 0)->metadataVector.get(0)->name);
    EXPECT_EQ("string", RES_CER_ATTR(0, 0)->metadataVector.get(0)->type);
    EXPECT_EQ("WSG84", RES_CER_ATTR(0, 0)->metadataVector.get(0)->value);
    EXPECT_EQ(SccInvalidParameter, RES_CER_STATUS(0).code);
    EXPECT_EQ("request parameter is invalid/not allowed", RES_CER_STATUS(0).reasonPhrase);
    EXPECT_EQ("action: APPEND - entity: (E2, T2) - offending attribute: A5 - error parsing location attribute, value: <erroneous>", RES_CER_STATUS(0).details);

    /* Check that every involved collection at MongoDB is as expected */
    /* Note we are using EXPECT_STREQ() for some cases, as Mongo Driver returns const char*, not string
     * objects (see http://code.google.com/p/googletest/wiki/Primer#String_Comparison) */

    DBClientBase* connection = getMongoConnection();

    /* entities collection */
    BSONObj ent;
    std::vector<BSONElement> attrs;
    ASSERT_EQ(2, connection->count(ENTITIES_COLL, BSONObj()));

    ent = connection->findOne(ENTITIES_COLL, BSON("_id.id" << "E1" << "_id.type" << "T1"));
    EXPECT_STREQ("E1", C_STR_FIELD(ent.getObjectField("_id"), "id"));
    EXPECT_STREQ("T1", C_STR_FIELD(ent.getObjectField("_id"), "type"));
    EXPECT_FALSE(ent.hasField("creDate"));
    EXPECT_FALSE(ent.hasField("modDate"));
    attrs = ent.getField("attrs").Array();
    ASSERT_EQ(2, attrs.size());
    BSONObj a1 = getAttr(attrs, "A1", "TA1");
    EXPECT_STREQ("A1", C_STR_FIELD(a1, "name"));
    EXPECT_STREQ("TA1", C_STR_FIELD(a1, "type"));
    EXPECT_STREQ("-5, 2", C_STR_FIELD(a1, "value"));
    EXPECT_FALSE(a1.hasField("creDate"));
    EXPECT_FALSE(a1.hasField("modDate"));
    BSONObj a2 = getAttr(attrs, "A2", "TA2");
    EXPECT_STREQ("A2", C_STR_FIELD(a2, "name"));
    EXPECT_STREQ("TA2", C_STR_FIELD(a2, "type"));
    EXPECT_STREQ("noloc", C_STR_FIELD(a2, "value"));
    EXPECT_FALSE(a2.hasField("creDate"));
    EXPECT_FALSE(a2.hasField("modDate"));
    EXPECT_STREQ("A1", ent.getObjectField("location").getStringField("attrName"));
    EXPECT_EQ(2, coordX(ent));
    EXPECT_EQ(-5, coordY(ent));

    ent = connection->findOne(ENTITIES_COLL, BSON("_id.id" << "E2" << "_id.type" << "T2"));
    EXPECT_STREQ("E2", C_STR_FIELD(ent.getObjectField("_id"), "id"));
    EXPECT_STREQ("T2", C_STR_FIELD(ent.getObjectField("_id"), "type"));
    EXPECT_FALSE(ent.hasField("modDate"));
    EXPECT_FALSE(ent.hasField("creDate"));
    attrs = ent.getField("attrs").Array();
    ASSERT_EQ(1, attrs.size());
    a2 = getAttr(attrs, "A2", "TA2");
    EXPECT_STREQ("A2", C_STR_FIELD(a2, "name"));
    EXPECT_STREQ("TA2", C_STR_FIELD(a2, "type"));
    EXPECT_STREQ("Y", C_STR_FIELD(a2, "value"));
    EXPECT_FALSE(a2.hasField("creDate"));
    EXPECT_FALSE(a2.hasField("modDate"));
    EXPECT_FALSE(ent.hasField("location"));

    /* Release connection */
    mongoDisconnect();

    utExit();
}

/* ****************************************************************************
*
*  - appendNotSupportedLocationFail
*/
TEST(mongoUpdateContextGeoRequest, appendNotSupportedLocationFail)
{
    utInit();

    HttpStatusCode         ms;
    UpdateContextRequest   req;
    UpdateContextResponse  res;

    /* Prepare database */
    prepareDatabase();

    /* Forge the request (from "inside" to "outside") */
    ContextElement ce;
    ce.entityId.fill("E2", "T2", "false");
    ContextAttribute ca("A5", "TA5", "8, -9");
    Metadata md("location", "string", "gurugu");
    ca.metadataVector.push_back(&md);
    ce.contextAttributeVector.push_back(&ca);
    req.contextElementVector.push_back(&ce);
    req.updateActionType.set("APPEND");

    /* Invoke the function in mongoBackend library */
    servicePathVector.clear();    
    ms = mongoUpdateContext(&req, &res, "", servicePathVector, uriParams);

    /* Check response is as expected */
    EXPECT_EQ(SccOk, ms);

    EXPECT_EQ(0, res.errorCode.code);
    EXPECT_EQ(0, res.errorCode.reasonPhrase.size());
    EXPECT_EQ(0, res.errorCode.details.size());

    ASSERT_EQ(1, res.contextElementResponseVector.size());
    /* Context Element response # 1 */
    EXPECT_EQ("E2", RES_CER(0).entityId.id);
    EXPECT_EQ("T2", RES_CER(0).entityId.type);
    EXPECT_EQ("false", RES_CER(0).entityId.isPattern);
    ASSERT_EQ(1, RES_CER(0).contextAttributeVector.size());
    EXPECT_EQ("A5", RES_CER_ATTR(0, 0)->name);
    EXPECT_EQ("TA5", RES_CER_ATTR(0, 0)->type);
    EXPECT_EQ(0, RES_CER_ATTR(0, 0)->value.size());
    ASSERT_EQ(1, RES_CER_ATTR(0, 0)->metadataVector.size());
    EXPECT_EQ("location", RES_CER_ATTR(0, 0)->metadataVector.get(0)->name);
    EXPECT_EQ("string", RES_CER_ATTR(0, 0)->metadataVector.get(0)->type);
    EXPECT_EQ("gurugu", RES_CER_ATTR(0, 0)->metadataVector.get(0)->value);
    EXPECT_EQ(SccInvalidParameter, RES_CER_STATUS(0).code);
    EXPECT_EQ("request parameter is invalid/not allowed", RES_CER_STATUS(0).reasonPhrase);
    EXPECT_EQ("action: APPEND - entity: (E2, T2) - offending attribute: A5 - only WSG84 is supported for location, found: <gurugu>", RES_CER_STATUS(0).details);

    /* Check that every involved collection at MongoDB is as expected */
    /* Note we are using EXPECT_STREQ() for some cases, as Mongo Driver returns const char*, not string
     * objects (see http://code.google.com/p/googletest/wiki/Primer#String_Comparison) */

    DBClientBase* connection = getMongoConnection();

    /* entities collection */
    BSONObj ent;
    std::vector<BSONElement> attrs;
    ASSERT_EQ(2, connection->count(ENTITIES_COLL, BSONObj()));

    ent = connection->findOne(ENTITIES_COLL, BSON("_id.id" << "E1" << "_id.type" << "T1"));
    EXPECT_STREQ("E1", C_STR_FIELD(ent.getObjectField("_id"), "id"));
    EXPECT_STREQ("T1", C_STR_FIELD(ent.getObjectField("_id"), "type"));
    EXPECT_FALSE(ent.hasField("creDate"));
    EXPECT_FALSE(ent.hasField("modDate"));
    attrs = ent.getField("attrs").Array();
    ASSERT_EQ(2, attrs.size());
    BSONObj a1 = getAttr(attrs, "A1", "TA1");
    EXPECT_STREQ("A1", C_STR_FIELD(a1, "name"));
    EXPECT_STREQ("TA1", C_STR_FIELD(a1, "type"));
    EXPECT_STREQ("-5, 2", C_STR_FIELD(a1, "value"));
    EXPECT_FALSE(a1.hasField("creDate"));
    EXPECT_FALSE(a1.hasField("modDate"));
    BSONObj a2 = getAttr(attrs, "A2", "TA2");
    EXPECT_STREQ("A2", C_STR_FIELD(a2, "name"));
    EXPECT_STREQ("TA2", C_STR_FIELD(a2, "type"));
    EXPECT_STREQ("noloc", C_STR_FIELD(a2, "value"));
    EXPECT_FALSE(a2.hasField("creDate"));
    EXPECT_FALSE(a2.hasField("modDate"));
    EXPECT_STREQ("A1", ent.getObjectField("location").getStringField("attrName"));
    EXPECT_EQ(2, coordX(ent));
    EXPECT_EQ(-5, coordY(ent));

    ent = connection->findOne(ENTITIES_COLL, BSON("_id.id" << "E2" << "_id.type" << "T2"));
    EXPECT_STREQ("E2", C_STR_FIELD(ent.getObjectField("_id"), "id"));
    EXPECT_STREQ("T2", C_STR_FIELD(ent.getObjectField("_id"), "type"));
    EXPECT_FALSE(ent.hasField("modDate"));
    EXPECT_FALSE(ent.hasField("creDate"));
    attrs = ent.getField("attrs").Array();
    ASSERT_EQ(1, attrs.size());
    a2 = getAttr(attrs, "A2", "TA2");
    EXPECT_STREQ("A2", C_STR_FIELD(a2, "name"));
    EXPECT_STREQ("TA2", C_STR_FIELD(a2, "type"));
    EXPECT_STREQ("Y", C_STR_FIELD(a2, "value"));
    EXPECT_FALSE(a2.hasField("creDate"));
    EXPECT_FALSE(a2.hasField("modDate"));
    EXPECT_FALSE(ent.hasField("location"));

    /* Release connection */
    mongoDisconnect();

    utExit();
}

/* ****************************************************************************
*
*  - updateWrongCoordinatesFormatFail
*/
TEST(mongoUpdateContextGeoRequest, updateWrongCoordinatesFormatFail)
{
    utInit();

    HttpStatusCode         ms;
    UpdateContextRequest   req;
    UpdateContextResponse  res;

    /* Prepare database */
    prepareDatabase();

    /* Forge the request (from "inside" to "outside") */
    ContextElement ce;
    ce.entityId.fill("E1", "T1", "false");
    ContextAttribute ca("A1", "TA1", "invalid");
    ce.contextAttributeVector.push_back(&ca);
    req.contextElementVector.push_back(&ce);
    req.updateActionType.set("UPDATE");

    /* Invoke the function in mongoBackend library */
    servicePathVector.clear();    
    ms = mongoUpdateContext(&req, &res, "", servicePathVector, uriParams);

    /* Check response is as expected */
    EXPECT_EQ(SccOk, ms);

    EXPECT_EQ(0, res.errorCode.code);
    EXPECT_EQ(0, res.errorCode.reasonPhrase.size());
    EXPECT_EQ(0, res.errorCode.details.size());

    ASSERT_EQ(1, res.contextElementResponseVector.size());
    /* Context Element response # 1 */
    EXPECT_EQ("E1", RES_CER(0).entityId.id);
    EXPECT_EQ("T1", RES_CER(0).entityId.type);
    EXPECT_EQ("false", RES_CER(0).entityId.isPattern);
    ASSERT_EQ(1, RES_CER(0).contextAttributeVector.size());
    EXPECT_EQ("A1", RES_CER_ATTR(0, 0)->name);
    EXPECT_EQ("TA1", RES_CER_ATTR(0, 0)->type);
    EXPECT_EQ(0, RES_CER_ATTR(0, 0)->value.size());
    ASSERT_EQ(0, RES_CER_ATTR(0, 0)->metadataVector.size());
    EXPECT_EQ(SccInvalidParameter, RES_CER_STATUS(0).code);
    EXPECT_EQ("request parameter is invalid/not allowed", RES_CER_STATUS(0).reasonPhrase);
    EXPECT_EQ("action: UPDATE - entity: (E1, T1) - offending attribute: A1 - error parsing location attribute, value: <invalid>", RES_CER_STATUS(0).details);

    /* Check that every involved collection at MongoDB is as expected */
    /* Note we are using EXPECT_STREQ() for some cases, as Mongo Driver returns const char*, not string
     * objects (see http://code.google.com/p/googletest/wiki/Primer#String_Comparison) */

    DBClientBase* connection = getMongoConnection();

    /* entities collection */
    BSONObj ent;
    std::vector<BSONElement> attrs;
    ASSERT_EQ(2, connection->count(ENTITIES_COLL, BSONObj()));

    ent = connection->findOne(ENTITIES_COLL, BSON("_id.id" << "E1" << "_id.type" << "T1"));
    EXPECT_STREQ("E1", C_STR_FIELD(ent.getObjectField("_id"), "id"));
    EXPECT_STREQ("T1", C_STR_FIELD(ent.getObjectField("_id"), "type"));
    EXPECT_FALSE(ent.hasField("creDate"));
    EXPECT_FALSE(ent.hasField("modDate"));
    attrs = ent.getField("attrs").Array();
    ASSERT_EQ(2, attrs.size());
    BSONObj a1 = getAttr(attrs, "A1", "TA1");
    EXPECT_STREQ("A1", C_STR_FIELD(a1, "name"));
    EXPECT_STREQ("TA1", C_STR_FIELD(a1, "type"));
    EXPECT_STREQ("-5, 2", C_STR_FIELD(a1, "value"));
    EXPECT_FALSE(a1.hasField("creDate"));
    EXPECT_FALSE(a1.hasField("modDate"));
    BSONObj a2 = getAttr(attrs, "A2", "TA2");
    EXPECT_STREQ("A2", C_STR_FIELD(a2, "name"));
    EXPECT_STREQ("TA2", C_STR_FIELD(a2, "type"));
    EXPECT_STREQ("noloc", C_STR_FIELD(a2, "value"));
    EXPECT_FALSE(a2.hasField("creDate"));
    EXPECT_FALSE(a2.hasField("modDate"));
    EXPECT_STREQ("A1", ent.getObjectField("location").getStringField("attrName"));
    EXPECT_EQ(2, coordX(ent));
    EXPECT_EQ(-5, coordY(ent));

    ent = connection->findOne(ENTITIES_COLL, BSON("_id.id" << "E2" << "_id.type" << "T2"));
    EXPECT_STREQ("E2", C_STR_FIELD(ent.getObjectField("_id"), "id"));
    EXPECT_STREQ("T2", C_STR_FIELD(ent.getObjectField("_id"), "type"));
    EXPECT_FALSE(ent.hasField("modDate"));
    EXPECT_FALSE(ent.hasField("creDate"));
    attrs = ent.getField("attrs").Array();
    ASSERT_EQ(1, attrs.size());
    a2 = getAttr(attrs, "A2", "TA2");
    EXPECT_STREQ("A2", C_STR_FIELD(a2, "name"));
    EXPECT_STREQ("TA2", C_STR_FIELD(a2, "type"));
    EXPECT_STREQ("Y", C_STR_FIELD(a2, "value"));
    EXPECT_FALSE(a2.hasField("creDate"));
    EXPECT_FALSE(a2.hasField("modDate"));
    EXPECT_FALSE(ent.hasField("location"));

    /* Release connection */
    mongoDisconnect();

    utExit();
}

/* ****************************************************************************
*
*  - updateLocationMetadataFail
*/
TEST(mongoUpdateContextGeoRequest, updateLocationMetadataFail)
{
    utInit();

    HttpStatusCode         ms;
    UpdateContextRequest   req;
    UpdateContextResponse  res;

    /* Prepare database */
    prepareDatabase();

    /* Forge the request (from "inside" to "outside") */
    ContextElement ce;
    ce.entityId.fill("E1", "T1", "false");
    ContextAttribute ca("A2", "TA2", "2, -4");
    Metadata md("location", "string", "WSG84");
    ca.metadataVector.push_back(&md);
    ce.contextAttributeVector.push_back(&ca);
    req.contextElementVector.push_back(&ce);
    req.updateActionType.set("UPDATE");

    /* Invoke the function in mongoBackend library */
    servicePathVector.clear();   
    ms = mongoUpdateContext(&req, &res, "", servicePathVector, uriParams);

    /* Check response is as expected */
    EXPECT_EQ(SccOk, ms);

    EXPECT_EQ(0, res.errorCode.code);
    EXPECT_EQ(0, res.errorCode.reasonPhrase.size());
    EXPECT_EQ(0, res.errorCode.details.size());

    ASSERT_EQ(1, res.contextElementResponseVector.size());
    /* Context Element response # 1 */
    EXPECT_EQ("E1", RES_CER(0).entityId.id);
    EXPECT_EQ("T1", RES_CER(0).entityId.type);
    EXPECT_EQ("false", RES_CER(0).entityId.isPattern);
    ASSERT_EQ(1, RES_CER(0).contextAttributeVector.size());
    EXPECT_EQ("A2", RES_CER_ATTR(0, 0)->name);
    EXPECT_EQ("TA2", RES_CER_ATTR(0, 0)->type);
    EXPECT_EQ(0, RES_CER_ATTR(0, 0)->value.size());
    ASSERT_EQ(1, RES_CER_ATTR(0, 0)->metadataVector.size());
    EXPECT_EQ("location", RES_CER_ATTR(0, 0)->metadataVector.get(0)->name);
    EXPECT_EQ("string", RES_CER_ATTR(0, 0)->metadataVector.get(0)->type);
    EXPECT_EQ("WSG84", RES_CER_ATTR(0, 0)->metadataVector.get(0)->value);
    EXPECT_EQ(SccInvalidParameter, RES_CER_STATUS(0).code);
    EXPECT_EQ("request parameter is invalid/not allowed", RES_CER_STATUS(0).reasonPhrase);
    EXPECT_EQ("action: UPDATE - entity: (E1, T1) - offending attribute: A2 - location nature of an attribute has to be defined at creation time, with APPEND", RES_CER_STATUS(0).details);

    /* Check that every involved collection at MongoDB is as expected */
    /* Note we are using EXPECT_STREQ() for some cases, as Mongo Driver returns const char*, not string
     * objects (see http://code.google.com/p/googletest/wiki/Primer#String_Comparison) */

    DBClientBase* connection = getMongoConnection();

    /* entities collection */
    BSONObj ent;
    std::vector<BSONElement> attrs;
    ASSERT_EQ(2, connection->count(ENTITIES_COLL, BSONObj()));

    ent = connection->findOne(ENTITIES_COLL, BSON("_id.id" << "E1" << "_id.type" << "T1"));
    EXPECT_STREQ("E1", C_STR_FIELD(ent.getObjectField("_id"), "id"));
    EXPECT_STREQ("T1", C_STR_FIELD(ent.getObjectField("_id"), "type"));
    EXPECT_FALSE(ent.hasField("creDate"));
    EXPECT_FALSE(ent.hasField("modDate"));
    attrs = ent.getField("attrs").Array();
    ASSERT_EQ(2, attrs.size());
    BSONObj a1 = getAttr(attrs, "A1", "TA1");
    EXPECT_STREQ("A1", C_STR_FIELD(a1, "name"));
    EXPECT_STREQ("TA1", C_STR_FIELD(a1, "type"));
    EXPECT_STREQ("-5, 2", C_STR_FIELD(a1, "value"));
    EXPECT_FALSE(a1.hasField("creDate"));
    EXPECT_FALSE(a1.hasField("modDate"));
    BSONObj a2 = getAttr(attrs, "A2", "TA2");
    EXPECT_STREQ("A2", C_STR_FIELD(a2, "name"));
    EXPECT_STREQ("TA2", C_STR_FIELD(a2, "type"));
    EXPECT_STREQ("noloc", C_STR_FIELD(a2, "value"));
    EXPECT_FALSE(a2.hasField("creDate"));
    EXPECT_FALSE(a2.hasField("modDate"));
    EXPECT_STREQ("A1", ent.getObjectField("location").getStringField("attrName"));
    EXPECT_EQ(2, coordX(ent));
    EXPECT_EQ(-5, coordY(ent));

    ent = connection->findOne(ENTITIES_COLL, BSON("_id.id" << "E2" << "_id.type" << "T2"));
    EXPECT_STREQ("E2", C_STR_FIELD(ent.getObjectField("_id"), "id"));
    EXPECT_STREQ("T2", C_STR_FIELD(ent.getObjectField("_id"), "type"));
    EXPECT_FALSE(ent.hasField("modDate"));
    EXPECT_FALSE(ent.hasField("creDate"));
    attrs = ent.getField("attrs").Array();
    ASSERT_EQ(1, attrs.size());
    a2 = getAttr(attrs, "A2", "TA2");
    EXPECT_STREQ("A2", C_STR_FIELD(a2, "name"));
    EXPECT_STREQ("TA2", C_STR_FIELD(a2, "type"));
    EXPECT_STREQ("Y", C_STR_FIELD(a2, "value"));
    EXPECT_FALSE(a2.hasField("creDate"));
    EXPECT_FALSE(a2.hasField("modDate"));
    EXPECT_FALSE(ent.hasField("location"));

    /* Release connection */
    mongoDisconnect();

    utExit();
}

/* ****************************************************************************
*
*  - deleteLocationMetadataFail
*/
TEST(mongoUpdateContextGeoRequest, deleteLocationMetadataFail)
{

    utInit();

    HttpStatusCode         ms;
    UpdateContextRequest   req;
    UpdateContextResponse  res;

    /* Prepare database */
    prepareDatabase();

    /* Forge the request (from "inside" to "outside") */
    ContextElement ce;
    ce.entityId.fill("E1", "T1", "false");
    ContextAttribute ca("A1", "TA1", "");
    Metadata md("location", "string", "WSG84");
    ca.metadataVector.push_back(&md);
    ce.contextAttributeVector.push_back(&ca);
    req.contextElementVector.push_back(&ce);
    req.updateActionType.set("DELETE");

    /* Invoke the function in mongoBackend library */
    servicePathVector.clear();    
    ms = mongoUpdateContext(&req, &res, "", servicePathVector, uriParams);

    /* Check response is as expected */
    EXPECT_EQ(SccOk, ms);

    EXPECT_EQ(0, res.errorCode.code);
    EXPECT_EQ(0, res.errorCode.reasonPhrase.size());
    EXPECT_EQ(0, res.errorCode.details.size());

    ASSERT_EQ(1, res.contextElementResponseVector.size());
    /* Context Element response # 1 */
    EXPECT_EQ("E1", RES_CER(0).entityId.id);
    EXPECT_EQ("T1", RES_CER(0).entityId.type);
    EXPECT_EQ("false", RES_CER(0).entityId.isPattern);
    ASSERT_EQ(1, RES_CER(0).contextAttributeVector.size());
    EXPECT_EQ("A1", RES_CER_ATTR(0, 0)->name);
    EXPECT_EQ("TA1", RES_CER_ATTR(0, 0)->type);
    EXPECT_EQ(0, RES_CER_ATTR(0, 0)->value.size());
    ASSERT_EQ(1, RES_CER_ATTR(0, 0)->metadataVector.size());
    EXPECT_EQ("location", RES_CER_ATTR(0, 0)->metadataVector.get(0)->name);
    EXPECT_EQ("string", RES_CER_ATTR(0, 0)->metadataVector.get(0)->type);
    EXPECT_EQ("WSG84", RES_CER_ATTR(0, 0)->metadataVector.get(0)->value);
    EXPECT_EQ(SccInvalidParameter, RES_CER_STATUS(0).code);
    EXPECT_EQ("request parameter is invalid/not allowed", RES_CER_STATUS(0).reasonPhrase);
    EXPECT_EQ("action: DELETE - entity: (E1, T1) - offending attribute: A1 - location attribute has to be defined at creation time, with APPEND", RES_CER_STATUS(0).details);

    /* Check that every involved collection at MongoDB is as expected */
    /* Note we are using EXPECT_STREQ() for some cases, as Mongo Driver returns const char*, not string
     * objects (see http://code.google.com/p/googletest/wiki/Primer#String_Comparison) */

    DBClientBase* connection = getMongoConnection();

    /* entities collection */
    BSONObj ent;
    std::vector<BSONElement> attrs;
    ASSERT_EQ(2, connection->count(ENTITIES_COLL, BSONObj()));

    ent = connection->findOne(ENTITIES_COLL, BSON("_id.id" << "E1" << "_id.type" << "T1"));
    EXPECT_STREQ("E1", C_STR_FIELD(ent.getObjectField("_id"), "id"));
    EXPECT_STREQ("T1", C_STR_FIELD(ent.getObjectField("_id"), "type"));
    EXPECT_FALSE(ent.hasField("creDate"));
    EXPECT_FALSE(ent.hasField("modDate"));
    attrs = ent.getField("attrs").Array();
    ASSERT_EQ(2, attrs.size());
    BSONObj a1 = getAttr(attrs, "A1", "TA1");
    EXPECT_STREQ("A1", C_STR_FIELD(a1, "name"));
    EXPECT_STREQ("TA1", C_STR_FIELD(a1, "type"));
    EXPECT_STREQ("-5, 2", C_STR_FIELD(a1, "value"));
    EXPECT_FALSE(a1.hasField("creDate"));
    EXPECT_FALSE(a1.hasField("modDate"));
    BSONObj a2 = getAttr(attrs, "A2", "TA2");
    EXPECT_STREQ("A2", C_STR_FIELD(a2, "name"));
    EXPECT_STREQ("TA2", C_STR_FIELD(a2, "type"));
    EXPECT_STREQ("noloc", C_STR_FIELD(a2, "value"));
    EXPECT_FALSE(a2.hasField("creDate"));
    EXPECT_FALSE(a2.hasField("modDate"));
    EXPECT_STREQ("A1", ent.getObjectField("location").getStringField("attrName"));
    EXPECT_EQ(2, coordX(ent));
    EXPECT_EQ(-5, coordY(ent));

    ent = connection->findOne(ENTITIES_COLL, BSON("_id.id" << "E2" << "_id.type" << "T2"));
    EXPECT_STREQ("E2", C_STR_FIELD(ent.getObjectField("_id"), "id"));
    EXPECT_STREQ("T2", C_STR_FIELD(ent.getObjectField("_id"), "type"));
    EXPECT_FALSE(ent.hasField("modDate"));
    EXPECT_FALSE(ent.hasField("creDate"));
    attrs = ent.getField("attrs").Array();
    ASSERT_EQ(1, attrs.size());
    a2 = getAttr(attrs, "A2", "TA2");
    EXPECT_STREQ("A2", C_STR_FIELD(a2, "name"));
    EXPECT_STREQ("TA2", C_STR_FIELD(a2, "type"));
    EXPECT_STREQ("Y", C_STR_FIELD(a2, "value"));
    EXPECT_FALSE(a2.hasField("creDate"));
    EXPECT_FALSE(a2.hasField("modDate"));
    EXPECT_FALSE(ent.hasField("location"));

    /* Release connection */
    mongoDisconnect();

    utExit();
}



