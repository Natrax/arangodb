////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "GeoJson.h"

#include <string>
#include <vector>

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <s2/s2loop.h>
#include <s2/s2point_region.h>
#include <s2/s2polygon.h>
#include <s2/s2polyline.h>

#include "Basics/VelocyPackHelper.h"
#include "Geo/ShapeContainer.h"
#include "Geo/S2/S2MultiPointRegion.h"
#include "Geo/S2/S2MultiPolyline.h"
#include "Logger/Logger.h"

namespace {
// Have one of these values:
const std::string kTypeStringPoint = "Point";
const std::string kTypeStringLineString = "LineString";
const std::string kTypeStringPolygon = "Polygon";
const std::string kTypeStringMultiPoint = "MultiPoint";
const std::string kTypeStringMultiLineString = "MultiLineString";
const std::string kTypeStringMultiPolygon = "MultiPolygon";
const std::string kTypeStringGeometryCollection = "GeometryCollection";
}  // namespace

namespace {
inline bool sameCharIgnoringCase(char a, char b) {
  return std::toupper(a) == std::toupper(b);
}
}  // namespace

namespace {
bool sameIgnoringCase(std::string const& s1, std::string const& s2) {
  return s1.size() == s2.size() &&
         std::equal(s1.begin(), s1.end(), s2.begin(), ::sameCharIgnoringCase);
}
}  // namespace

namespace {
arangodb::Result verifyClosedLoop(std::vector<S2Point> const& vertices) {
  using arangodb::Result;

  if (vertices.empty()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Empty loop");
  } else if (vertices.front() != vertices.back()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Loop not closed");
  }
  return TRI_ERROR_NO_ERROR;
}
}  // namespace

namespace {
void removeAdjacentDuplicates(std::vector<S2Point>& vertices) {
  TRI_ASSERT(!vertices.empty());
  for (size_t i = 0; i < vertices.size() - 1; i++) {
    if (vertices[i] == vertices[i + 1]) {
      vertices.erase(vertices.begin() + i + 1);
      i--;
    }
  }
  TRI_ASSERT(!vertices.empty());
}
}  // namespace

namespace arangodb {
namespace geo {
namespace geojson {

/// @brief parse GeoJSON Type
Type type(VPackSlice const& geoJSON) {
  if (!geoJSON.isObject()) {
    return Type::UNKNOWN;
  }

  VPackSlice type = geoJSON.get(Fields::kType);
  if (!type.isString()) {
    return Type::UNKNOWN;
  }

  std::string typeStr = type.copyString();
  if (::sameIgnoringCase(typeStr, ::kTypeStringPoint)) {
    return Type::POINT;
  } else if (::sameIgnoringCase(typeStr, ::kTypeStringLineString)) {
    return Type::LINESTRING;
  } else if (::sameIgnoringCase(typeStr, ::kTypeStringPolygon)) {
    return Type::POLYGON;
  } else if (::sameIgnoringCase(typeStr, ::kTypeStringMultiPoint)) {
    return Type::MULTI_POINT;
  } else if (::sameIgnoringCase(typeStr, ::kTypeStringMultiLineString)) {
    return Type::MULTI_LINESTRING;
  } else if (::sameIgnoringCase(typeStr, ::kTypeStringMultiPolygon)) {
    return Type::MULTI_POLYGON;
  } else if (::sameIgnoringCase(typeStr, ::kTypeStringGeometryCollection)) {
    return Type::GEOMETRY_COLLECTION;
  }
  return Type::UNKNOWN;
};

Result parseRegion(VPackSlice const& geoJSON, ShapeContainer& region) {
  Result badParam(TRI_ERROR_BAD_PARAMETER, "Invalid GeoJSON Geometry Object.");
  if (!geoJSON.isObject()) {
    return badParam;
  }

  Type t = type(geoJSON);
  switch (t) {
    case Type::POINT: {
      S2LatLng ll;
      Result res = parsePoint(geoJSON, ll);
      if (res.ok()) {
        region.reset(new S2PointRegion(ll.ToPoint()),
                     ShapeContainer::Type::S2_POINT);
      }
      return res;
    }
    case Type::MULTI_POINT: {
      std::vector<S2Point> vertices;
      Result res = parsePoints(geoJSON, true, vertices);
      if (res.ok()) {
        region.reset(new S2MultiPointRegion(&vertices),
                     ShapeContainer::Type::S2_MULTIPOINT);
      }
      return res;
    }
    case Type::LINESTRING: {
      auto line = std::make_unique<S2Polyline>();
      Result res = parseLinestring(geoJSON, *line.get());
      if (res.ok()) {
        region.reset(std::move(line), ShapeContainer::Type::S2_POLYLINE);
      }
      return res;
    }
    case Type::MULTI_LINESTRING: {
      std::vector<S2Polyline> polylines;
      Result res = parseMultiLinestring(geoJSON, polylines);
      if (res.ok()) {
        region.reset(new S2MultiPolyline(std::move(polylines)),
                     ShapeContainer::Type::S2_MULTIPOLYLINE);
      }
      return res;
    }
    case Type::POLYGON: {
      return parsePolygon(geoJSON, region);
    }
    case Type::MULTI_POLYGON:
    case Type::GEOMETRY_COLLECTION:
      return Result(TRI_ERROR_NOT_IMPLEMENTED, "GeoJSON type is not supported");
    case Type::UNKNOWN: {
      return badParam;
    }
  }

  return badParam;
}

/// @brief create s2 latlng
Result parsePoint(VPackSlice const& geoJSON, S2LatLng& latLng) {
  VPackSlice coordinates = geoJSON.get(Fields::kCoordinates);

  if (coordinates.isArray() && coordinates.length() == 2) {
    latLng = S2LatLng::FromDegrees(coordinates.at(1).getNumber<double>(),
                                   coordinates.at(0).getNumber<double>())
                 .Normalized();
    return TRI_ERROR_NO_ERROR;
  }
  return TRI_ERROR_BAD_PARAMETER;
}

/// https://tools.ietf.org/html/rfc7946#section-3.1.6
/// First Loop represent the outer bound, subsequent loops must be holes
/// { "type": "Polygon",
///   "coordinates": [
///    [ [100.0, 0.0], [101.0, 0.0], [101.0, 1.0], [100.0, 1.0], [100.0, 0.0] ],
///    [ [100.2, 0.2], [100.8, 0.2], [100.8, 0.8], [100.2, 0.8], [100.2, 0.2] ]
///   ]
/// }
Result parsePolygon(VPackSlice const& geoJSON, ShapeContainer& region) {
  VPackSlice coordinates = geoJSON;
  if (geoJSON.isObject()) {
    coordinates = geoJSON.get(Fields::kCoordinates);
    TRI_ASSERT(type(geoJSON) == Type::POLYGON);
  }
  if (!coordinates.isArray()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "coordinates missing");
  }
  size_t n = coordinates.length();
  
  // Coordinates of a Polygon are an array of LinearRing coordinate arrays.
  // The first element in the array represents the exterior ring. Any subsequent
  // elements
  // represent interior rings (or holes).
  // - A linear ring is a closed LineString with four or more positions.
  // -  The first and last positions are equivalent, and they MUST contain
  //    identical values; their representation SHOULD also be identical.
  std::vector<std::unique_ptr<S2Loop>> loops;
  for (VPackSlice loopVertices : VPackArrayIterator(coordinates)) {
    std::vector<S2Point> vtx;
    Result res = parsePoints(loopVertices, /*geoJson*/ true, vtx);
    if (res.fail()) {
      return res;
    }
    res = ::verifyClosedLoop(vtx);  // check last vertices are same
    if (res.fail()) {
      return res;
    }
    ::removeAdjacentDuplicates(vtx);  // s2loop doesn't like duplicates
    
    if (vtx.size() < 3+1) { // last vertex must be same as first
      return Result(TRI_ERROR_BAD_PARAMETER, "Invalid loop in polygon, "
                    "must have at least 3 distinct vertices");
    }
    vtx.resize(vtx.size() - 1);  // remove redundant last vertex
    
    if (n == 1) { // rectangle detection
      if (vtx.size() == 1) {
        S2LatLng v0(vtx[0]);
        region.reset(std::make_unique<S2LatLngRect>(v0, v0),
                     ShapeContainer::Type::S2_LATLNGRECT);
        return TRI_ERROR_NO_ERROR;
      } else if (vtx.size() == 4) {
        S2LatLng v0(vtx[0]), v1(vtx[1]), v2(vtx[2]), v3(vtx[3]);
        if (v0.lat() == v1.lat() && v1.lng() == v2.lng() &&
            v2.lat() == v3.lat() && v3.lng() == v0.lng()) {
          region.reset(std::make_unique<S2LatLngRect>(v0, v3),
                       ShapeContainer::Type::S2_LATLNGRECT);
          return TRI_ERROR_NO_ERROR;
        }
      }
    }
    
    loops.push_back(std::make_unique<S2Loop>(vtx));
    if (!loops.back()->IsValid()) {  // will check first and last for us
      return Result(TRI_ERROR_BAD_PARAMETER, "Invalid loop in polygon");
    }
    S2Loop* loop = loops.back().get();
    loop->Normalize();

    // Any subsequent loops must be holes within first loop
    if (loops.size() > 1 && !loops.front()->Contains(loop)) {
      return Result(TRI_ERROR_BAD_PARAMETER,
                    "Subsequent loop not a hole in polygon");
    }
  }

  std::unique_ptr<S2Polygon> poly;
  if (loops.size() == 1) {
    poly = std::make_unique<S2Polygon>(std::move(loops[0]));
  } else if (loops.size() > 1) {
    poly = std::make_unique<S2Polygon>(std::move(loops));
  }
  if (poly) {
    TRI_ASSERT(poly->IsValid());
    region.reset(std::move(poly), ShapeContainer::Type::S2_POLYGON);
    return TRI_ERROR_NO_ERROR;
  }
  return Result(TRI_ERROR_BAD_PARAMETER, "Empty polygons are not allowed");
}

/// https://tools.ietf.org/html/rfc7946#section-3.1.4
/// from the rfc {"type":"LineString","coordinates":[[100.0, 0.0],[101.0,1.0]]}
Result parseLinestring(VPackSlice const& geoJson, S2Polyline& linestring) {
  VPackSlice coordinates = geoJson;
  if (geoJson.isObject()) {
    TRI_ASSERT(type(geoJson) == Type::LINESTRING);
    coordinates = geoJson.get(Fields::kCoordinates);
  }
  if (!coordinates.isArray()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Coordinates missing");
  }

  std::vector<S2Point> vertices;
  Result res = parsePoints(geoJson, /*geoJson*/ true, vertices);
  if (res.ok()) {
    ::removeAdjacentDuplicates(vertices);  // no need for duplicates
    if (vertices.size() < 2) {
      return Result(TRI_ERROR_BAD_PARAMETER,
                    "Invalid LineString, adjacent "
                    "vertices must not be identical or antipodal.");
    }
    linestring.Init(vertices);
    TRI_ASSERT(linestring.IsValid());
  }
  return res;
};

/// MultiLineString contain an array of LineString coordinate arrays:
///  {"type": "MultiLineString",
///   "coordinates": [[[170.0, 45.0], [180.0, 45.0]],
///                   [[-180.0, 45.0], [-170.0, 45.0]]] }
Result parseMultiLinestring(VPackSlice const& geoJson,
                            std::vector<S2Polyline>& ll) {
  if (!geoJson.isObject()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Invalid MultiLineString");
  }
  TRI_ASSERT(type(geoJson) == Type::MULTI_LINESTRING);
  VPackSlice coordinates = geoJson.get(Fields::kCoordinates);
  if (!coordinates.isArray()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Coordinates missing");
  }

  for (VPackSlice linestring : VPackArrayIterator(coordinates)) {
    if (!linestring.isArray()) {
      return Result(TRI_ERROR_BAD_PARAMETER, "Invalid MultiLineString");
    }
    ll.emplace_back(S2Polyline());
    // can handle linestring array
    Result res = parseLinestring(linestring, ll.back());
    if (res.fail()) {
      return res;
    }
  }
  return TRI_ERROR_NO_ERROR;
}

// parse geojson coordinates into s2 points
Result parsePoints(VPackSlice const& geoJSON, bool geoJson,
                   std::vector<S2Point>& vertices) {
  vertices.clear();
  VPackSlice coordinates = geoJSON;
  if (geoJSON.isObject()) {
    coordinates = geoJSON.get("coordinates");
  } else if (!coordinates.isArray()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Coordinates missing");
  }

  for (VPackSlice pt : VPackArrayIterator(coordinates)) {
    if (!pt.isArray() || pt.length() < 2) {
      return Result(TRI_ERROR_BAD_PARAMETER, "Bad coordinate " + pt.toJson());
    }

    VPackSlice lat = pt.at(geoJson ? 1 : 0);
    VPackSlice lon = pt.at(geoJson ? 0 : 1);
    if (!lat.isNumber() || !lon.isNumber()) {
      return Result(TRI_ERROR_BAD_PARAMETER, "Bad coordinate " + pt.toJson());
    }
    vertices.push_back(
        S2LatLng::FromDegrees(lat.getNumber<double>(), lon.getNumber<double>())
            .ToPoint());
  }
  return TRI_ERROR_NO_ERROR;
}

Result parseLoop(velocypack::Slice const& coords, bool geoJson, S2Loop& loop) {
  if (!coords.isArray()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "Coordinates missing");
  }

  std::vector<S2Point> vertices;
  Result res = parsePoints(coords, geoJson, vertices);
  if (res.fail()) {
    return res;
  }

  /* TODO enable this check after removing deprecated IS_IN_POLYGON fn
      res = ::verifyClosedLoop(vertices);  // check last vertices are same
      if (res.fail()) {
        return res;
      }*/

  ::removeAdjacentDuplicates(vertices);  // s2loop doesn't like duplicates
  if (vertices.front() == vertices.back()) {
    vertices.resize(vertices.size() - 1);  // remove redundant last vertex
  }
  loop.Init(vertices);
  if (!loop.IsValid()) {  // will check first and last for us
    return Result(TRI_ERROR_BAD_PARAMETER, "Invalid GeoJSON loop");
  }
  loop.Normalize();

  return TRI_ERROR_NO_ERROR;
}

}  // namespace geojson
}  // namespace geo
}  // namespace arangodb
