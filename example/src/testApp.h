#ifndef _TEST_APP
#define _TEST_APP


#include "ofMain.h"
#include "HttpFormManager.h"

class testApp : public ofBaseApp{

	public:
		void setup();
		void update();
		void draw();

		void keyPressed  (int key);

		void newResponse(HttpFormResponse &response); //form was submitted callback
	
		HttpFormManager fm;

		void exit(){ printf("exit!!!!\n"); }
};

#endif
