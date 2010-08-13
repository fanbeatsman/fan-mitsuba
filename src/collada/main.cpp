/**
 * Mitsuba COLLADA -> XML converter
 * 
 * Takes a DAE file and turns it into a scene description and separate mesh files
 * using a compact binary format. All associated files are copied into newly created 
 * 'textures' and 'meshes' directories
 *
 * Currently supports the following subset of the COLLADA specification:
 * - Arbitrary polygonal meshes
 * - Lambert and Phong materials (allowed to be textured)
 * - Cameras
 * - Spot and Point and Ambient lights
 *
 * When exporting using Maya/FBX, be sure to have it convert all NURBS surfaces into
 * "Software Render Meshes". Triangulation is not required (the code below does this
 * automatically for arbitrary polygonal meshes). The Light and camera export options
 * should be activated, since they are off by default. While modeling the scene, it is
 * advisable to use light sources with an inverse square falloff. Otherwise, the 
 * illumination will be competely different when rendering in Mitsuba (the image might 
 * be pitch black). Note that most BRDFs in Mitsuba treat surfaces as one-sided, thus they
 * will appear black when seen from the back.
 *
 * The conversion barfs when it gets more than 10MB in one single XML string 
 * (error: xmlSAX2Characters: huge text node: out of memory). In this case, split the
 * mesh into smaller pieces or recompile libxml with a higher limit.
 *
 * Since Mitsuba does not support per-vertex colors and prefers textures, any vertex colors 
 * part of the input file are not converted and should instead be baked to textures beforehand 
 * (e.g. using Lighting/shading -> Batch bake in Maya).
 */

#include <xercesc/parsers/SAXParser.hpp>
#include <xercesc/dom/DOMException.hpp>
#include "converter.h"
#include <mitsuba/hw/glrenderer.h>
#include <mitsuba/core/fresolver.h>

XERCES_CPP_NAMESPACE_USE

class ConsoleColladaConverter : public ColladaConverter {
public:
	inline ConsoleColladaConverter() {
	}

	std::string locateResource(const std::string &resource) {
		return "";
	}
};

void help() {
	cout << "COLLADA 1.4 Importer, Version " MTS_VERSION ", Copyright (c) " MTS_YEAR " Wenzel Jakob" << endl
		<< "Syntax: mtsimport [options] <DAE source file> <XML destination file> [Adjustment file]" << endl
		<< "Options/Arguments:" << endl
		<<  "   -h          Display this help text" << endl << endl
		<<  "   -s          Assume that colors are in sRGB space." << endl << endl
		<< "Please see the documentation for more information." << endl;
}

int colladaMain(int argc, char **argv) {
	bool srgb = false;
	char optchar;
	
	optind = 1;

	while ((optchar = getopt(argc, argv, "sh")) != -1) {
		switch (optchar) {
			case 's':
				srgb = true;
				break;
			case 'h':
			default:
				help();
				return -1;
	}
	};

	if (argc-optind < 2) {
		help();
		return -1;
	}

	ConsoleColladaConverter converter;
	converter.setSRGB(srgb);
	converter.convert(argv[optind], "", argv[optind+1], argc > optind+2 ? argv[optind+2] : "");

	return 0;
}

int ubi_main(int argc, char **argv) {
	int retval;
	
	/* Initialize Xerces-C */
	try {
		XMLPlatformUtils::Initialize();
	} catch(const XMLException &toCatch) {
		fprintf(stderr, "Error during Xerces initialization: %s",
			XMLString::transcode(toCatch.getMessage()));
		return -1;
	}

	/* Initialize the core framework */
	Class::staticInitialization();
	Statistics::staticInitialization();
	Thread::staticInitialization();
	Logger::staticInitialization();
	Spectrum::staticInitialization();

	Thread::getThread()->getLogger()->setLogLevel(EInfo);

	FileResolver *resolver = FileResolver::getInstance();
#if defined(WIN32)
	char lpFilename[1024];
	if (GetModuleFileNameA(NULL,
		lpFilename, sizeof(lpFilename))) {
		resolver->addPathFromFile(lpFilename);
	} else {
		SLog(EWarn, "Could not determine the executable path");
	}
#elif defined(__LINUX__)
	char exePath[PATH_MAX];
	if (getcwd(exePath, PATH_MAX)) {
		resolver->addPathFromFile(exePath);
	} else {
		SLog(EWarn, "Could not determine the executable path");
	}
#else
	MTS_AUTORELEASE_BEGIN()
	resolver->addPath(__ubi_bundlepath());
	MTS_AUTORELEASE_END() 
#endif

	try {
		/* An OpenGL context may be required for the GLU tesselator */
		ref<Session> session = Session::create();
		ref<Device> device = Device::create(session);
		ref<Renderer> renderer = Renderer::create(session);

		session->init();
		device->init();
		renderer->init(device);

		device->makeCurrent(renderer);

		retval = colladaMain(argc, argv);

		if (retval != -1)
			cout << "Finished conversion" << endl;

		renderer->shutdown();
		device->shutdown();
		session->shutdown();
	} catch (const std::exception &e) {
		std::cerr << "Caught a critical exeption: " << e.what() << std::endl;
		retval = -1;
	} catch(const XMLException &toCatch) {
		SLog(EError, "Caught a Xerces exception: %s",
			XMLString::transcode(toCatch.getMessage()));
		retval = -1;
	} catch(const DOMException &toCatch) {
		SLog(EError, "Caught a Xerces exception: %s",
			XMLString::transcode(toCatch.getMessage()));
		retval = -1;
	} catch (...) {
		std::cerr << "Caught a critical exeption of unknown type!" << endl;
		retval = -1;
	}
	
	XMLPlatformUtils::Terminate();

	/* Shutdown the core framework */
	Spectrum::staticShutdown();
	Logger::staticShutdown();
	Thread::staticShutdown();
	Statistics::staticShutdown();
	Class::staticShutdown();
	
	return retval;
}

#if !defined(__OSX__)
int main(int argc, char **argv) {
	return ubi_main(argc, argv);
}
#endif

