#include "testApp.h"


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


void testApp::draw(){
	fm.draw();
	float y = 30 + 20 * sinf( 0.1 * ofGetFrameNum() );
	ofCircle( 12, y, 5);
}


void testApp::keyPressed(int key){

	//first, create and fill in a form
	HttpForm f = HttpForm( "http://uri.cat/fabrica/fileUploadTest.php" );
	//form field name, file name, mime type
	f.addFile("myFile", "hansi.jpg", "image/jpg");

	switch (key) {

		case '1':
			//starts a background thread with the upload.
			//testApp::newResponse(...) will get called when done
			fm.submitForm( f, false );	//false == ignoreReply
			break;

		case '2':{
			//upload on this thread, this block until done.
			//you get a response when done.
			HttpFormResponse r = fm.submitFormBlocking( f );
			printf("response: %s\n", r.responseBody.c_str() );
			break;
		}
		
		case '3':
			fm.cancelCurrentFormSubmission();		//this causes sigpipe! //TODO!
			break;

		default:
			break;
	}
}

