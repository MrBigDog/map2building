/*
 * building.cpp
 *
 *  Created on: March 22, 2014
 *      Author: Bjorn Blissing
 */
#include "building.h"

#include <osg/Notify>
#include <osg/Geometry>
#include <osgUtil/Tessellator>
#include <osgUtil/SmoothingVisitor>

#include "materiallibrary.h"

void Building::load(pugi::xml_node buildingNode)
{
	std::string type = buildingNode.attribute("type").as_string("");

	if (pugi::xml_node basementNode = buildingNode.child("basement")) {
		m_basementHeight = basementNode.attribute("height").as_double(0.0);
	}

	if (pugi::xml_node roofNode = buildingNode.child("roof")) {
		m_roofHeight = roofNode.attribute("height").as_double(0.0);
	}

	if (pugi::xml_node windowNode = buildingNode.child("window")) {
		m_windowOffset = windowNode.attribute("offset").as_double(0.0);
	}

	if (pugi::xml_node materialsNode = buildingNode.child("materials")) {
		// Load material sets
		for (pugi::xml_node materialNode = materialsNode.child("material"); materialNode; materialNode = materialNode.next_sibling("material")) {
			osg::ref_ptr<MaterialSet> materialSet = new MaterialSet;
			materialSet->setWallMaterialId(materialNode.attribute("wall").as_uint(0));
			materialSet->setWindowMaterialId(materialNode.attribute("window").as_uint(0));
			materialSet->setRoofMaterialId(materialNode.attribute("roof").as_uint(0));
			materialSet->setBaseMaterialId(materialNode.attribute("basement").as_uint(0));
			m_materials.push_back(materialSet);
		}
	}

	osg::notify(osg::DEBUG_INFO) << "Building Type:     " << type << std::endl;
	osg::notify(osg::DEBUG_INFO) << "Basement height:   " << m_basementHeight << std::endl;
	osg::notify(osg::DEBUG_INFO) << "Roof height:       " << m_roofHeight << std::endl;
	osg::notify(osg::DEBUG_INFO) << "Window offset:     " << m_windowOffset << std::endl;
	osg::notify(osg::DEBUG_INFO) << "Material sets:     " << m_materials.size() << std::endl;
	osg::notify(osg::DEBUG_INFO) << std::endl;
}

osg::ref_ptr<osg::Group> Building::createFromPolygon(osg::ref_ptr<Polygon> polygon, osg::Vec2 baseCoordinate)
{
	osg::ref_ptr<MaterialSet> materialSet = getRandomMaterialSet(polygon->center());
	osg::ref_ptr<osg::Group> building = new osg::Group;
	// Build basement
	building->addChild(buildBasement(polygon, baseCoordinate, materialSet));
	// Build walls
	building->addChild(buildWalls(polygon, baseCoordinate, materialSet));
	// Build windows
	// Build roof
	building->addChild(buildRoof(polygon, baseCoordinate, materialSet));
	return building;
}

osg::ref_ptr<osg::Geode> Building::buildBasement(osg::ref_ptr<Polygon> polygon, osg::Vec2 baseCoordinate, osg::ref_ptr<MaterialSet> materialSet)
{
	osg::ref_ptr<osg::Geode> basementGeode = new osg::Geode;
	osg::Vec4 basementColor = osg::Vec4(1.0, 1.0, 1.0, 1.0);
	osg::ref_ptr<osg::Geometry> basementGeometry = extrudePolygon(polygon, baseCoordinate, -m_basementHeight, basementColor);
	// Apply material
	osg::ref_ptr<osg::StateSet> stateSet = MaterialLibrary::instance().materialFromId(materialSet->baseMaterialId());
	basementGeometry->setStateSet(stateSet);
	basementGeode->addDrawable(basementGeometry);
	return basementGeode;
}

osg::ref_ptr<osg::Geode> Building::buildWalls(osg::ref_ptr<Polygon> polygon, osg::Vec2 baseCoordinate, osg::ref_ptr<MaterialSet> materialSet)
{
	osg::ref_ptr<osg::Geode> wallGeode = new osg::Geode;
	osg::Vec4 wallColor = osg::Vec4(1.0, 1.0, 1.0, 1.0);
	osg::ref_ptr<osg::Geometry> wallGeometry = extrudePolygon(polygon, baseCoordinate, m_roofHeight, wallColor);
	// Apply material
	osg::ref_ptr<osg::StateSet> stateSet = MaterialLibrary::instance().materialFromId(materialSet->wallMaterialId());
	wallGeometry->setStateSet(stateSet);
	wallGeode->addDrawable(wallGeometry);
	return wallGeode;
}

osg::ref_ptr<osg::Geode> Building::buildRoof(osg::ref_ptr<Polygon> polygon, osg::Vec2 baseCoordinate, osg::ref_ptr<MaterialSet> materialSet)
{
	osg::ref_ptr<osg::Geode> roofGeode = new osg::Geode;
	osg::Vec4 roofColor = osg::Vec4(1.0, 1.0, 1.0, 1.0);
	osg::ref_ptr<osg::Geometry> roofGeometry = new osg::Geometry();
	// Calculate height of building
	double height = polygon->height() + m_roofHeight;
	// Add vertices to roof
	osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
	osg::ref_ptr<osg::Vec3Array> normalArray = new osg::Vec3Array();
	size_t numberOfPoints = polygon->points()->size();

	for (size_t p = 0; p < (numberOfPoints - 1); ++p) {
		osg::Vec2 point = polygon->points()->at(p) - baseCoordinate;
		vertices->push_back(osg::Vec3(point.x(), point.y(), height));
		normalArray->push_back(osg::Vec3(0.0, 0.0, 1.0));
	}

	// Add first point to close loop
	osg::Vec2 point = polygon->points()->at(0) - baseCoordinate;
	vertices->push_back(osg::Vec3(point.x(), point.y(), height));
	normalArray->push_back(osg::Vec3(0.0, 0.0, 1.0));
	// Pass the created vertex array to the points geometry object.
	roofGeometry->setVertexArray(vertices);
	roofGeometry->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::POLYGON, 0, numberOfPoints));
	roofGeometry->setNormalArray(normalArray);
	roofGeometry->setNormalBinding(osg::Geometry::BIND_PER_VERTEX);
	osg::ref_ptr<osg::Vec4Array> colorArray = new osg::Vec4Array;
	colorArray->push_back(roofColor);
	roofGeometry->setColorArray(colorArray);
	roofGeometry->setColorBinding(osg::Geometry::BIND_OVERALL);
	// Tessellate roof polygon
	osg::ref_ptr<osgUtil::Tessellator> tessellator = new osgUtil::Tessellator();
	tessellator->setTessellationType(osgUtil::Tessellator::TESS_TYPE_GEOMETRY);
	tessellator->setBoundaryOnly(false);
	tessellator->setWindingType( osgUtil::Tessellator::TESS_WINDING_ODD);
	tessellator->setTessellationNormal(osg::Vec3(0.0, 0.0, 1.0));
	tessellator->retessellatePolygons( *roofGeometry );
	osg::ref_ptr<osg::StateSet> stateSet = MaterialLibrary::instance().materialFromId(materialSet->roofMaterialId());
	roofGeometry->setStateSet(stateSet);
	roofGeode->addDrawable(roofGeometry);
	return roofGeode;
}

osg::ref_ptr<osg::Geometry> Building::extrudePolygon(osg::ref_ptr<Polygon> polygon, osg::Vec2 baseCoordinate, double height, osg::Vec4 color)
{
	// Collect the verticies
	osg::ref_ptr<osg::Vec3Array> vertexArray = new osg::Vec3Array;
	osg::ref_ptr<osg::Vec3Array> normalArray = new osg::Vec3Array;
	osg::ref_ptr<osg::Vec2Array> textureArray = new osg::Vec2Array;
	osg::Vec2Array::iterator it;
	// Height from low to high
	double h1 = osg::minimum(polygon->height(), polygon->height() + height);
	double h2 = osg::maximum(polygon->height(), polygon->height() + height);
	// Add vertex and normals from points
	size_t numberOfPoints = polygon->points()->size();
	double startU = 0.0;
	for (size_t p = 0; p < (numberOfPoints - 1); ++p) {
		osg::Vec2 point1 = polygon->points()->at(p) - baseCoordinate;
		osg::Vec2 point2 = polygon->points()->at(p+1) - baseCoordinate;
		createVertexAndNormal(point1, point2, h1, h2, vertexArray, normalArray, textureArray, startU);
	}

	// Add first point to close loop
	osg::Vec2 point1 = polygon->points()->at(numberOfPoints-1) - baseCoordinate;
	osg::Vec2 point2 = polygon->points()->at(0) - baseCoordinate;
	createVertexAndNormal(point1, point2, h1, h2, vertexArray, normalArray, textureArray, startU);
	osg::ref_ptr<osg::Geometry> wallGeometry = new osg::Geometry;
	wallGeometry->setVertexArray(vertexArray);
	wallGeometry->addPrimitiveSet( new osg::DrawArrays(GL_QUADS, 0, vertexArray->size()) );
	wallGeometry->setNormalArray(normalArray);
	wallGeometry->setNormalBinding(osg::Geometry::BIND_PER_VERTEX);
	osg::ref_ptr<osg::Vec4Array> colorArray = new osg::Vec4Array;
	colorArray->push_back(color);
	wallGeometry->setColorArray(colorArray);
	wallGeometry->setColorBinding(osg::Geometry::BIND_OVERALL);

	wallGeometry->setTexCoordArray(0, textureArray);
	return wallGeometry;
}

void Building::createVertexAndNormal(osg::Vec2 point1, osg::Vec2 point2,
	double h1, double h2,
	osg::ref_ptr<osg::Vec3Array> vertexArray,
	osg::ref_ptr<osg::Vec3Array> normalArray,
	osg::ref_ptr<osg::Vec2Array> textureArray,
	double& startU) 
{
	osg::Vec3 vertex1 = osg::Vec3(point1.x(), point1.y(), h1);
	osg::Vec3 vertex2 = osg::Vec3(point1.x(), point1.y(), h2);
	osg::Vec3 vertex3 = osg::Vec3(point2.x(), point2.y(), h2);
	osg::Vec3 vertex4 = osg::Vec3(point2.x(), point2.y(), h1);

	double endV = fabs(h1 - h2);
	osg::Vec2 textureCoord1 = osg::Vec2(startU, 0);
	osg::Vec2 textureCoord2 = osg::Vec2(startU, endV);

	startU += (point2 - point1).length();

	osg::Vec2 textureCoord3 = osg::Vec2(startU, endV);
	osg::Vec2 textureCoord4 = osg::Vec2(startU, 0);
	// Vertexs
	vertexArray->push_back(vertex1);
	vertexArray->push_back(vertex2);
	vertexArray->push_back(vertex3);
	vertexArray->push_back(vertex4);
	// Normals
	osg::Vec3 normal = (vertex2 - vertex1)^(vertex4 - vertex1);
	normal.normalize();
	normalArray->push_back(normal);
	normalArray->push_back(normal);
	normalArray->push_back(normal);
	normalArray->push_back(normal);

	// Texture Coordinates
	textureArray->push_back(textureCoord1);
	textureArray->push_back(textureCoord2);
	textureArray->push_back(textureCoord3);
	textureArray->push_back(textureCoord4);
}


osg::ref_ptr<MaterialSet> Building::getRandomMaterialSet(osg::Vec2 point)
{
	if (m_materials.empty()) {
		return 0;
	}

	unsigned int materialId = ((int) point.x() + (int) point.y())%m_materials.size();
	//osg::notify(osg::ALWAYS) << "MaterialID: " << materialId << std::endl;
	return m_materials.at(materialId);
}