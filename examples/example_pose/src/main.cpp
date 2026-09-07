#include "ofMain.h"
#include "ofApp.h"

int main() {
	ofGLFWWindowSettings settings;
	settings.setSize(1024, 768);
	settings.numSamples = 4;
	settings.windowMode = OF_WINDOW;
	ofCreateWindow(settings);

	ofRunApp(new ofApp());
}
