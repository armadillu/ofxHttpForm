#include "ofApp.h"


void ofApp::newResponse(HttpFormResponse &response){
	printf("form '%s' returned : %s\n", response.url.c_str(), response.ok ? "OK" : "KO" );
}


void ofApp::setup(){

	ofBackground(22, 0, 0);
	ofSetVerticalSync(true);
	ofSetFrameRate(60);

	//FormManager that will deal with the form, add a listener to get an answer when submitted
	fm.setVerbose(true);	//we want to see what's going on internally
	ofAddListener(fm.formResponseEvent, this, &ofApp::newResponse);	//add listener to formManager if u want to be notified when form was sent

	return;
}


void ofApp::draw(){
	fm.draw();
	float y = 30 + 20 * sinf( 0.1 * ofGetFrameNum() );
	ofCircle( 12, y, 5);
}


void ofApp::keyPressed(int key){

	//first, create and fill in a form
	HttpForm f = HttpForm( "http://uri.cat/fabrica/fileUploadTest.php" );
	//form field name, file name, mime type
	f.addFile("myFile", "hansi.jpg", "image/jpg");

	switch (key) {

		case '1':
			//starts a background thread with the upload.
			//ofApp::newResponse(...) will get called when done
			fm.submitForm( f, false );	//false == ignoreReply
			break;

		case '2':{
			//upload on this thread, this block until done.
			//you get a response when done.
			HttpFormResponse r = fm.submitFormBlocking( f );
			ofLogNotice() << "response: " << r.responseBody;
			break;
		}
		

		default:
			break;
	}
}

