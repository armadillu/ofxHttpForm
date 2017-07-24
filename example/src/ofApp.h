#ifndef _TEST_APP
#define _TEST_APP


#include "ofMain.h"
#include "HttpFormManager.h"

class ofApp : public ofBaseApp{

	public:
		void setup();
		void update(){};
		void draw();

		void keyPressed  (int key);

		//"form was submitted" callback
		void newResponse(HttpFormResponse &response);
	
		HttpFormManager fm;
};

#endif
