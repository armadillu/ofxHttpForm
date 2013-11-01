#include "testApp.h"

#include "ofMain.h"

void testApp::newResponse(HttpFormResponse &response){
	printf("form '%s' returned : %s\n", response.url.c_str(), response.ok ? "OK" : "KO" );
}


void testApp::setup(){

	ofBackground(22, 0, 0);
	ofSetVerticalSync(true);
	ofSetFrameRate(60);
	//FormManager that will deal with the form, add a listener to get an answer when submitted
	fm.setVerbose(true);	//we want to see what's going on internally
	ofAddListener(fm.formResponseEvent, this, &testApp::newResponse);	//add listener to formManager if u want to be notified when form was sent

	return;
}


void testApp::update(){

}


void testApp::draw(){
	fm.draw();
	
	float y = 30 + 20 * sinf( 0.1 * ofGetFrameNum() );
	ofCircle( 12, y, 5);
}


void testApp::keyPressed(int key){

	//first, create and fill in a form
	HttpForm f = HttpForm( "http://uri.cat/fabrica/fileUploadTest.php" );

	f.addFile("myFile", "hansi.jpg", "image/jpg");

	switch (key) {
			
		case '1':{
			
			//FormManager that will deal with the form, add a listener to get an answer when submitted
			fm.submitForm( f, false );	//false == ignoreReply
			break;
		}

		case '2':{
			
			//FormManager that will deal with the form, add a listener to get an answer when submitted
			HttpFormResponse r = fm.submitFormBlocking( f );		
			printf("response: %s\n", r.responseBody.c_str() );
			break;

		}
		
		case '3':{			
			fm.cancelCurrentFormSubmission();		//this causes sigpipe sometimes?
			break;
		}
			
		default:
			break;
	}
}

