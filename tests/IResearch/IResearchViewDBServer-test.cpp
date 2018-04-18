////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "catch.hpp"
#include "../IResearch/AgencyCommManagerMock.h"
#include "../IResearch/StorageEngineMock.h"
#include "Basics/files.h"
#include "Cluster/ClusterInfo.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchViewDBServer.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "velocypack/Parser.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchViewDBServerSetup {
  GeneralClientConnectionMapMock* agency;
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  std::string testFilesystemPath;

  IResearchViewDBServerSetup(): server(nullptr, nullptr) {
    auto* agencyCommManager = new AgencyCommManagerMock();

    agency = agencyCommManager->addConnection<GeneralClientConnectionMapMock>();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::RoleEnum::ROLE_PRIMARY);
    arangodb::EngineSelectorFeature::ENGINE = &engine;
    arangodb::AgencyCommManager::MANAGER.reset(agencyCommManager);

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::WARN);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);

    // setup required application features
    features.emplace_back(new arangodb::AuthenticationFeature(&server), false); // required for AgencyComm::send(...)
    features.emplace_back(new arangodb::DatabasePathFeature(&server), false);
    features.emplace_back(new arangodb::QueryRegistryFeature(&server), false); // required for TRI_vocbase_t instantiation
    features.emplace_back(new arangodb::ViewTypesFeature(&server), false); // required for TRI_vocbase_t::createView(...)
    features.emplace_back(new arangodb::iresearch::IResearchFeature(&server), false); // required for instantiating IResearchView*

    for (auto& f: features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.first);
    }

    for (auto& f: features) {
      f.first->prepare();
    }

    for (auto& f: features) {
      if (f.second) {
        f.first->start();
      }
    }

    arangodb::ClusterInfo::createInstance(nullptr); // required for generating view id

    testFilesystemPath = (
      (irs::utf8_path()/=
      TRI_GetTempPath())/=
      (std::string("arangodb_tests.") + std::to_string(TRI_microtime()))
    ).utf8();
    auto* dbPathFeature = arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabasePathFeature>("DatabasePath");
    const_cast<std::string&>(dbPathFeature->directory()) = testFilesystemPath;

    long systemError;
    std::string systemErrorStr;
    TRI_CreateDirectory(testFilesystemPath.c_str(), systemError, systemErrorStr);
  }

  ~IResearchViewDBServerSetup() {
    TRI_RemoveDirectory(testFilesystemPath.c_str());
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
    arangodb::application_features::ApplicationServer::server = nullptr;
    arangodb::AgencyCommManager::MANAGER.reset();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::RoleEnum::ROLE_SINGLE);

    // destroy application features
    for (auto& f: features) {
      if (f.second) {
        f.first->stop();
      }
    }

    for (auto& f: features) {
      f.first->unprepare();
    }

    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::DEFAULT);
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("IResearchViewDBServerTest", "[cluster][iresearch][iresearch-view]") {
  IResearchViewDBServerSetup s;
  (void)(s);

SECTION("test_create") {
  s.agency->responses["POST /_api/agency/read HTTP/1.1\r\n\r\n[[\"/Sync/LatestID\"]]"] = "http/1.0 200\n\n[ { \"\": { \"Sync\": { \"LatestID\" : 1 } } } ]";
  s.agency->responses["POST /_api/agency/write HTTP/1.1"] = "http/1.0 200\n\n{\"results\": []}";
  auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto wiew = arangodb::iresearch::IResearchViewDBServer::make(vocbase, json->slice(), 42);
  CHECK((false == !wiew));
  auto* impl = dynamic_cast<arangodb::iresearch::IResearchViewDBServer*>(wiew.get());
  CHECK((nullptr != impl));

  auto view = impl->create(123);
  CHECK((false == !view));
  CHECK((std::string("testView") == view->name()));
  CHECK((false == view->deleted()));
  CHECK((wiew->id() != view->id())); // must have unique ID
  CHECK((view->id() == view->planId())); // same as view ID
  CHECK((0 == view->planVersion()));
  CHECK((arangodb::iresearch::DATA_SOURCE_TYPE == view->type()));
  CHECK((&vocbase == &(view->vocbase())));
}

SECTION("test_drop") {
  // FIXME TODO implemet
}

SECTION("test_emplace") {
  // FIXME TODO implemet
}

SECTION("test_make") {
  // FIXME TODO implemet
}

SECTION("test_open") {
  // FIXME TODO implemet
}

SECTION("test_rename") {
  // FIXME TODO implemet
}

SECTION("test_toVelocyPack") {
  // FIXME TODO implemet
}

SECTION("test_updateProperties") {
  // FIXME TODO implemet
}

SECTION("test_visitCollections") {
  // FIXME TODO implemet
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------