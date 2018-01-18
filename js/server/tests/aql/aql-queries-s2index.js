/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, geo queries
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var db = require("@arangodb").db;
var errors = require("@arangodb").errors;
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;
var assertQueryError = helper.assertQueryError;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlGeoTestSuite() {
  var locations = null;
  var locationsNon = null;

  function runQuery(query) {
    var result = getQueryResults(query);

    result = result.map(function (row) {
      var r;
      if (row !== null && typeof row === 'object') {
        for (r in row) {
          if (row.hasOwnProperty(r)) {
            var value = row[r];
            if (typeof (value) === "number") {
              if (value !== parseFloat(parseInt(value))) {
                row[r] = Number(value).toFixed(5);
              }
            }
          }
        }
      }

      return row;
    });

    return result;
  }

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      var lat, lon;
      db._drop("UnitTestsAhuacatlLocations");
      db._drop("UnitTestsAhuacatlLocationsNon");

      locations = db._create("UnitTestsAhuacatlLocations");
      for (lat = -40; lat <= 40; ++lat) {
        for (lon = -40; lon <= 40; ++lon) {
          locations.save({ "latitude": lat, "longitude": lon });
        }
      }

      locations.ensureIndex({ type: "s2index", fields: ["latitude", "longitude"] });

      locationsNon = db._create("UnitTestsAhuacatlLocationsNon");

      for (lat = -40; lat <= 40; ++lat) {
        for (lon = -40; lon <= 40; ++lon) {
          locationsNon.save({ "latitude": lat, "longitude": lon });
        }
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      db._drop("UnitTestsAhuacatlLocations");
      db._drop("UnitTestsAhuacatlLocationsNon");
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test near function
    ////////////////////////////////////////////////////////////////////////////////

    /*testNear1 : function () {
      var expected = [ { "distance" : "111194.92664", "latitude" : -1, "longitude" : 0 }, { "distance" : "111194.92664", "latitude" : 0, "longitude" : -1 }, { "distance" : "111194.92664", "latitude" : 0, "longitude" : 1 }, { "distance" : "111194.92664", "latitude" : 1, "longitude" : 0 }, { "distance" : 0, "latitude" : 0, "longitude" : 0 } ];
      var actual = runQuery("FOR x IN NEAR(" + locations.name() + ", 0, 0, 5, \"distance\") SORT x.distance DESC, x.latitude, x.longitude RETURN x");
      assertEqual(expected, actual);
    },*/

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test near function
    ////////////////////////////////////////////////////////////////////////////////

    testNear2: function () {
      var expected = [{ "latitude": -10, "longitude": 24 }, { "latitude": -10, "longitude": 25 }, { "latitude": -10, "longitude": 26 }];
      var actual = runQuery("FOR x IN NEAR(" + locations.name() + ", -10, 25, 3) SORT x.latitude, x.longitude RETURN x");
      assertEqual(expected, actual);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test near function
    ////////////////////////////////////////////////////////////////////////////////

    /*testNear3 : function () {
      var expected = [ { "distance" : "14891044.54146", "latitude" : 40, "longitude" : -40 }, { "distance" : "14853029.30724", "latitude" : 40, "longitude" : -39 }, { "distance" : "14815001.47646", "latitude" : 40, "longitude" : -38 } ];
      var actual = runQuery("FOR x IN NEAR(" + locations.name() + ", -70, 70, 10000, \"distance\") SORT x.distance DESC LIMIT 3 RETURN x");
      assertEqual(expected, actual);
     
      let expected = [ {"distance" : "4487652.12954", "latitude" : -37, "longitude" : 26 }, { "distance" : "4485565.93668", "latitude" : -39, "longitude" : 20 }, { "distance" : "4484371.86154" , "latitude" : -38, "longitude" : 23 } ]; 
      let actual = runQuery("FOR x IN NEAR(" + locations.name() + ", -70, 70, null, \"distance\") SORT x.distance DESC LIMIT 3 RETURN x");
      assertEqual(expected, actual);
    },*/

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test near function
    ////////////////////////////////////////////////////////////////////////////////

    testNear4: function () {
      var expected = [{ "latitude": -40, "longitude": 40 }, { "latitude": -40, "longitude": 39 }, { "latitude": -40, "longitude": 38 }];
      var actual = runQuery("FOR x IN NEAR(" + locations.name() + ", -70, 70, null) SORT x.latitude, x.longitude DESC LIMIT 3 RETURN x");
      assertEqual(expected, actual);

      expected = [{ "latitude": -40, "longitude": 40 }, { "latitude": -40, "longitude": 39 }];
      actual = runQuery("FOR x IN NEAR(" + locations.name() + ", -70, 70, 2) SORT x.latitude, x.longitude DESC LIMIT 3 RETURN x");
      assertEqual(expected, actual);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test within function
    ////////////////////////////////////////////////////////////////////////////////

    /*testWithin1 : function () {
      var expected = [ { "distance" : 0, "latitude" : 0, "longitude" : 0 } ];
      var actual = runQuery("FOR x IN WITHIN(" + locations.name() + ", 0, 0, 10000, \"distance\") SORT x.latitude, x.longitude RETURN x");
      assertEqual(expected, actual);
    },*/

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test within function
    ////////////////////////////////////////////////////////////////////////////////

    /*testWithin2 : function () {
      var expected = [ { "distance" : "111194.92664", "latitude" : -1, "longitude" : 0 }, { "distance" : "111194.92664", "latitude" : 0, "longitude" : -1 }, { "distance" : 0, "latitude" : 0, "longitude" : 0 }, { "distance" : "111194.92664", "latitude" : 0, "longitude" : 1 }, { "distance" : "111194.92664", "latitude" : 1, "longitude" : 0 } ];
      var actual = runQuery("FOR x IN WITHIN(" + locations.name() + ", 0, 0, 150000, \"distance\") SORT x.latitude, x.longitude RETURN x");
      assertEqual(expected, actual);
    },*/

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test within function
    ////////////////////////////////////////////////////////////////////////////////

    testWithin3: function () {
      var expected = [{ "latitude": -10, "longitude": 25 }];
      var actual = runQuery("FOR x IN WITHIN(" + locations.name() + ", -10, 25, 10000) SORT x.latitude, x.longitude RETURN x");
      assertEqual(expected, actual);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test within function
    ////////////////////////////////////////////////////////////////////////////////

    testWithin4: function () {
      var expected = [
        { "latitude": -11, "longitude": 25 },
        { "latitude": -10, "longitude": 24 },
        { "latitude": -10, "longitude": 25 },
        { "latitude": -10, "longitude": 26 },
        { "latitude": -9, "longitude": 25 }
      ];

      var actual = runQuery("FOR x IN WITHIN(" + locations.name() + ", -10, 25, 150000) SORT x.latitude, x.longitude RETURN x");
      assertEqual(expected, actual);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test within function
    ////////////////////////////////////////////////////////////////////////////////

    testWithin5: function () {
      var expected = [];
      var actual = runQuery("FOR x IN WITHIN(" + locations.name() + ", -90, 90, 10000) SORT x.latitude, x.longitude RETURN x");
      assertEqual(expected, actual);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test without geo index available
    ////////////////////////////////////////////////////////////////////////////////

    /*testNonIndexed : function () {
      assertQueryError(errors.ERROR_QUERY_GEO_INDEX_MISSING.code, "RETURN NEAR(" + locationsNon.name() + ", 0, 0, 10)"); 
      assertQueryError(errors.ERROR_QUERY_GEO_INDEX_MISSING.code, "RETURN WITHIN(" + locationsNon.name() + ", 0, 0, 10)"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalid NEAR arguments count
////////////////////////////////////////////////////////////////////////////////

    testInvalidNearArgument : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NEAR(\"" + locationsNon.name() + "\", 0, 0, \"foo\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NEAR(\"" + locationsNon.name() + "\", 0, 0, true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN NEAR(\"" + locationsNon.name() + "\", 0, 0, 10, true)"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalid WITHIN arguments count
////////////////////////////////////////////////////////////////////////////////

    testInvalidWithinArgument : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN WITHIN(\"" + locationsNon.name() + "\", 0, 0, \"foo\")"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN WITHIN(\"" + locationsNon.name() + "\", 0, 0, true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN WITHIN(\"" + locationsNon.name() + "\", 0, 0, 0, true)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN WITHIN(\"" + locationsNon.name() + "\", 0, 0, 0, [ ])"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalid collection parameter
////////////////////////////////////////////////////////////////////////////////

    testInvalidCollectionArgument : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN WITHIN(1234, 0, 0, 10)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN WITHIN(false, 0, 0, 10)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN WITHIN(true, 0, 0, 10)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN WITHIN([ ], 0, 0, 10)"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN WITHIN({ }, 0, 0, 10)"); 
      assertQueryError(errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code, "RETURN WITHIN(@name, 0, 0, 10)", { name: "foobarbazcoll" }); 
      assertQueryError(errors.ERROR_QUERY_BIND_PARAMETER_MISSING.code, "RETURN WITHIN(@name, 0, 0, 10)"); 
    }*/

  };
}

function containsGeoTestSuite() {

  function runQuery(query) {
    var result1 = getQueryResults(query.string, query.bindVars || {}, false);
    //var result2 = getQueryResults(query.string, query.bindVars || {}, false, 
    //                              { optimizer: { rules: [ "-all"] } });
    assertEqual(query.expected, result1, query.string);
    //assertEqual(query.expected, result2, query.string);
  }

  let locations;
  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      var lat, lon;
      db._drop("UnitTestsContainsGeoTestSuite");

      locations = db._create("UnitTestsContainsGeoTestSuite");
      for (lat = -40; lat <= 40; ++lat) {
        for (lon = -40; lon <= 40; ++lon) {
          locations.save({ "lat": lat, "lng": lon });
        }
      }

      locations.ensureIndex({ type: "s2index", fields: ["lat", "lng"] });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      db._drop("UnitTestsContainsGeoTestSuite");
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple circle on sphere
    ////////////////////////////////////////////////////////////////////////////////

    testContainsCircle1: function () {
      runQuery({
        string: "FOR x IN @@cc FILTER GEO_DISTANCE([25, -10], [x.lng, x.lat]) <= 10000 RETURN x",
        bindVars: {
          "@cc": locations.name(),
        },
        expected: [{ "lat": -10, "lng": 25 }]
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple circle on sphere
    ////////////////////////////////////////////////////////////////////////////////

    testContainsCircle2: function () {
      runQuery({
        string: "FOR x IN @@cc FILTER GEO_DISTANCE([25, -10], [x.lng, x.lat]) <= 150000 SORT x.lat, x.lng RETURN x",
        bindVars: {
          "@cc": locations.name(),
        },
        expected: [
          { "lat": -11, "lng": 25 },
          { "lat": -10, "lng": 24 },
          { "lat": -10, "lng": 25 },
          { "lat": -10, "lng": 26 },
          { "lat": -9, "lng": 25 }
        ]
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple circle on sphere
    ////////////////////////////////////////////////////////////////////////////////

    testContainsCircle3: function () {
      runQuery({
        string: "FOR x IN @@cc FILTER GEO_DISTANCE([-90, -90], [x.lng, x.lat]) <= 10000 SORT x.lat, x.lng RETURN x",
        bindVars: {
          "@cc": locations.name(),
        },
        expected: []
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple annulus on sphere (a donut)
    ////////////////////////////////////////////////////////////////////////////////

    testContainsAnnulus1: function () {
      runQuery({
        string: `FOR x IN @@cc 
                   FILTER GEO_DISTANCE([25, -10], [x.lng, x.lat]) <= 150000 
                   FILTER GEO_DISTANCE([25, -10], [x.lng, x.lat]) > 108500 
                   SORT x.lat, x.lng RETURN x`,
        bindVars: {
          "@cc": locations.name(),
        },
        expected: [
          { "lat": -11, "lng": 25 },
          { "lat": -10, "lng": 24 },
          { "lat": -10, "lng": 26 },
          { "lat": -9, "lng": 25 }
        ]
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple annulus on sphere (a donut)
    ////////////////////////////////////////////////////////////////////////////////

    testContainsAnnulus2: function () {
      runQuery({
        string: `FOR x IN @@cc 
                   FILTER GEO_DISTANCE([25, -10], [x.lng, x.lat]) <= 150000 
                   FILTER GEO_DISTANCE([25, -10], [x.lng, x.lat]) > 109545 
                   SORT x.lat, x.lng RETURN x`,
        bindVars: {
          "@cc": locations.name(),
        },
        expected: [
          { "lat": -11, "lng": 25 },
          { "lat": -9, "lng": 25 }
        ]
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test simple annulus on sphere (a donut)
    ////////////////////////////////////////////////////////////////////////////////

    testContainsPolygon: function () {
      const polygon = {
        "type": "Polygon", 
        "coordinates": [[ [-11.5, 23.5], [-6, 26], [-10.5, 26.1], [-11.5, 23.5] ]]
      };

      runQuery({
        string: `FOR x IN @@cc 
                   FILTER GEO_CONTAINS(@poly, [x.lng, x.lat]) 
                   SORT x.lat, x.lng RETURN x`,
        bindVars: {
          "@cc": locations.name(),
          "poly": polygon
        },
        expected: [
          { "lat": 24, "lng": -11 },
          { "lat": 25, "lng": -10 },
          { "lat": 25, "lng": -9 },
          { "lat": 26, "lng": -10 },
          { "lat": 26, "lng": -9 },
          { "lat": 26, "lng": -8 },
          { "lat": 26, "lng": -7 }
        ]
      });
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlGeoTestSuite);
jsunity.run(containsGeoTestSuite);

return jsunity.done();

