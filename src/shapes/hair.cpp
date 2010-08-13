#include <mitsuba/render/shape.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/fresolver.h>
#include <fstream>

MTS_NAMESPACE_BEGIN

/**
 * The 'Hair' primitive consists of a list of hair segments, which are
 * rasterized into cylinders and spheres. The file format is simply a
 * list of lines of the form "x y z" where an empty line indicates the beginning
 * of a new hair.
 */
class Hair : public Shape {
	struct HairSegment {
		Point start, end;

		inline HairSegment(Point start, Point end) 
		 : start(start), end(end) {
		}
	};

	Float m_radius;
	std::vector<HairSegment> m_segments;
public:
	Hair(const Properties &props) : Shape(props) {
		std::string filename = props.getString("filename");
		m_radius = (Float) props.getFloat("radius", 0.05f);
		m_name = FileResolver::getInstance()->resolve(filename);

		Log(EInfo, "Loading hair geometry from \"%s\" ..", m_name.c_str());

		std::ifstream is(m_name.c_str());
		if (is.fail())
			Log(EError, "Could not open \"%s\"!", m_name.c_str());

		std::string line;
		int segments = 0;
		Point p, prev;
		while (is.good()) {
			std::getline(is, line);
			if (line.length() > 0 && line[0] == '#')
				continue;
			if (line.length() == 0) {
				segments = 0;
			} else {
				std::istringstream iss(line);
				iss >> p.x >> p.y >> p.z;

				if (segments++ > 0) 
					m_segments.push_back(HairSegment(prev, p));

				prev = p;
			}
		}

		Log(EDebug, "Read %i hair segments.", m_segments.size());
	}

	Hair(Stream *stream, InstanceManager *manager) : Shape(stream, manager) {
		m_radius = stream->readFloat();

		size_t segmentCount = stream->readUInt();
		m_segments.reserve(segmentCount);
		for (size_t i=0; i<segmentCount; ++i) 
			m_segments.push_back(HairSegment(Point(stream), Point(stream)));
	}

	void serialize(Stream *stream, InstanceManager *manager) const {
		Shape::serialize(stream, manager);

		stream->writeFloat(m_radius);
		stream->writeUInt(m_segments.size());
		for (size_t i=0; i<m_segments.size(); ++i) {
			m_segments[i].start.serialize(stream);
			m_segments[i].end.serialize(stream);
		}
	}

	bool isCompound() const {
		return true;
	}

	Shape *getElement(int _index) {
		unsigned int index = _index / 2;
		if (index >= m_segments.size())
			return NULL;

		Point start = m_segments[index].start;
		Point end = m_segments[index].end;
		Float length = (end-start).length();

		Vector axis = normalize(end-start);
		Vector rotAxis = normalize(cross(Vector(0,0,1), axis));
		Float rotAngle = radToDeg(std::acos(axis.z));

		Transform trafo = 
			m_objectToWorld
			* Transform::translate(start)
			* Transform::rotate(rotAxis, rotAngle);

		if ((_index % 2) == 0) {
			Properties sphereProperties("sphere");
			sphereProperties.setFloat("radius", m_radius);
			sphereProperties.setTransform("toWorld", trafo);
			Shape *sphere = static_cast<Shape *> (PluginManager::getInstance()->
				createObject(Shape::m_theClass, sphereProperties));
			sphere->addChild("bsdf", m_bsdf);
			sphere->configure();
			return sphere;
		} else {
			Properties cylinderProperties("cylinder");
			cylinderProperties.setFloat("radius", m_radius);
			cylinderProperties.setFloat("length", length);
			cylinderProperties.setTransform("toWorld", trafo);
			Shape *cylinder = static_cast<Shape *> (PluginManager::getInstance()->
				createObject(Shape::m_theClass, cylinderProperties));
			cylinder->addChild("bsdf", m_bsdf);
			cylinder->configure();

			return cylinder;
		}
	}

	MTS_DECLARE_CLASS()
};

MTS_IMPLEMENT_CLASS_S(Hair, false, Shape)
MTS_EXPORT_PLUGIN(Hair, "Hair geometry");
MTS_NAMESPACE_END