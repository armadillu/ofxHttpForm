/*
 *  HttpFormManager.cpp
 *  emptyExample
 *
 *  Created by Oriol Ferrer Mesià on 03/02/11.
 *  Copyright 2011 uri.cat. All rights reserved.
 *
 */

#include "HttpFormManager.h"

#include "ofEvents.h"
#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Net/HTMLForm.h"
#include "Poco/StreamCopier.h"
#include "Poco/Path.h"
#include "Poco/URI.h"
#include "Poco/Exception.h"
#include "Poco/Mutex.h"
#include "Poco/Net/FilePartSource.h"

#include <sys/types.h>
#include <sys/stat.h>
//#include <unistd.h>

HttpFormManager::HttpFormManager(){
	timeOut = 10;
	debug = false;
	timeToStop = false;
	userAgent = "HttpFormManager (Poco Powered)";
	acceptString = "";
}

HttpFormManager::~HttpFormManager(){

	timeToStop = true;	//lets flag the thread so that it doesnt try access stuff while we delete things around
	cancelCurrentFormSubmission();
	
	//wait till thread is done with the current loop
	waitForThread(false);
	
	//printf( "HttpFormManager::~HttpFormManager() >> about to delete all queue (%d)...\n", getQueueLength() );
	while ( getQueueLength() > 0 ){
		lock();
			HttpFormResponse * r = q.front();
			delete r;
			q.pop();
		unlock();
	}
}

void HttpFormManager::setTimeOut(int seconds){
	timeOut = seconds;
}


void HttpFormManager::setVerbose(bool verbose){
	debug = verbose;
}


void HttpFormManager::setUserAgent( string newUserAgent ){
	userAgent = newUserAgent;
}


void HttpFormManager::setAcceptString( string newAcceptString ){
	acceptString = newAcceptString;
}


void HttpFormManager::draw(int x, int y){
	
	char aux[2048];
	lock();
		int n = q.size();
		if ( isThreadRunning() && n > 0 ){
			HttpFormResponse * r = q.front();
			sprintf(aux, "HttpFormManager: Submitting\n%s \nQueue Size: %d", r->url.c_str(),  n);
		}else{
			sprintf(aux, "HttpFormManager : Idle");
		}
		glColor3ub(255,0,0);
		ofDrawBitmapString(aux, x, y);
	unlock();	
}


HttpFormResponse* HttpFormManager::createFormRespPtrFromForm( HttpForm f ){

 	HttpFormResponse *form = new HttpFormResponse();
	form->url = f.url;
	form->formIds = f.formIds;
	form->formValues = f.formValues;
	form->formFiles = f.formFiles;
	return form;
}


void HttpFormManager::submitForm( HttpForm f, bool ignoreReply ){

 	HttpFormResponse *form = createFormRespPtrFromForm( f );
	form->submissionCanceled = false;
	form->ignoreReply = ignoreReply;
	form->submissionCanceled = false;
	form->session = NULL;
			
	lock();
		q.push(form);
	unlock();
	
	if ( !isThreadRunning() ){	//if the queue is not running, lets start it
		startThread(true, false);
	}	
}


HttpFormResponse HttpFormManager::submitFormBlocking( HttpForm  f ){
	
	HttpFormResponse form;
	form.url = f.url;
	form.formIds = f.formIds;
	form.formValues = f.formValues;
	form.formFiles = f.formFiles;
	form.submissionCanceled = false;
	form.ignoreReply = false;
	form.session = NULL;
	
	bool ok = executeForm( &form, false);
	if (!ok) ofLog(OF_LOG_ERROR, "HttpFormManager::submitFormBlocking executeForm() failed!");
	return form;	
}

void HttpFormManager::cancelCurrentFormSubmission(){

	lock();
		int n = q.size();
		if ( isThreadRunning() && n > 0 ){			
			HttpFormResponse * r = q.front();
			if (debug) printf( "HttpFormManager::cancelCurrentForm() >> about to stop form submission of %s...\n", r->url.c_str() );
			try{
				r->submissionCanceled = true;
				if ( r->session->connected() ) {
					r->session->abort();
				}
			}catch(Exception& exc){
				ofLog( OF_LOG_ERROR, "HttpFormManager::cancelCurrentForm(%s) >> Exception: %s\n", r->url.c_str(), exc.displayText().c_str() );
			}
		}
	unlock();
}

int HttpFormManager::getQueueLength(){
	int queueLen = 0;
	lock();
		queueLen = q.size();
	unlock();	
	return queueLen;
}


HTMLForm* HttpFormManager::createPocoFormFrom( HttpFormResponse * resp ){
	
	HTMLForm *form = new HTMLForm();
	
	if( resp->formFiles.size() > 0 )
		form->setEncoding(HTMLForm::ENCODING_MULTIPART);
	else
		form->setEncoding(HTMLForm::ENCODING_URL);
					
	// form values
	for( unsigned i = 0; i < resp->formIds.size(); i++ ){
		const std::string name = resp->formIds[i].c_str();
		const std::string val = resp->formValues[i].c_str();
		form->set(name, val);
	}

	map<string,FormContent>::iterator it;
	for( it = resp->formFiles.begin(); it != resp->formFiles.end(); it++ ){	
		FilePartSource * file = NULL;
		try{
			string path = it->second.path;
			file = new FilePartSource(  path, it->second.contentType );				
		}catch(...){
			ofLog( OF_LOG_FATAL_ERROR, "HttpFormManager::createPocoFormFrom() form file not found! %s\n", it->second.path.c_str());
			return NULL;
		}
		form->addPart( it->first, file );
	}

	return form;
}

bool HttpFormManager::executeForm( HttpFormResponse* resp, bool sendResultThroughEvents ){  

	try{

		Poco::URI uri( resp->url );
		std::string path(uri.getPathAndQuery());
		if (path.empty()) path = "/";
		resp->action = path;

		HTTPClientSession session(uri.getHost(), uri.getPort());
		HTTPRequest req(HTTPRequest::HTTP_POST, path, HTTPMessage::HTTP_1_1);		
		resp->session = &session;
		
		session.setTimeout( Poco::Timespan(timeOut,0) );
		
		req.set( "User-Agent", userAgent.c_str() );
		if (acceptString.length() > 0){
			req.set( "Accept", acceptString.c_str() );
		}

		//auth, todo later?
		//	Poco::Net::HTTPBasicCredentials cred(_username, _password);
		//	cred.authenticate(req);

		//long story short of why we fill in two forms:
		//we need to specify exact lenght of the data in the form (file s headers), but we can't really measure it untill its been sent
		//so we create 2 indetical forms, one just to measure its size, other to use for sending thorugh the request
		HTMLForm *form = createPocoFormFrom(resp);
		if (form == NULL) return false;
		HTMLForm *form2 = createPocoFormFrom(resp);
					
		form->prepareSubmit(req);
		form2->prepareSubmit(req);
		
		req.setChunkedTransferEncoding(false);
		//req.setKeepAlive(true);
		//req.set("Accept" , "*/*" );
		//req.set("Origin", "http://" + uri.getHost() + ":" + ofToString(uri.getPort()) );
		//req.set("Host",  uri.getHost() + ":" + ofToString(uri.getPort()) );

		//lets find out the length of the total data we are sending and report it				
		std::ostringstream formDumpContainer;
		form2->write(formDumpContainer);
		req.setContentLength( formDumpContainer.str().length() );	//finally we can specify exact content length in the request

		try{
			//send the form data through the http session
			 std::ostream & ostr = session.sendRequest(req);
			form->write( ostr );
		}catch(Exception& exc){
			ofLog( OF_LOG_ERROR, "HttpFormManager::executeForm(%s) >> Exception while sending form \n", resp->action.c_str() );
			delete form2;
			delete form;
			//throw("Exception");
			return false;
		}

		if (resp->submissionCanceled){	
			if(debug) printf("HttpFormManager::executeForm() >> form submission (%s) canceled!\n", resp->action.c_str() );
			delete form2;
			delete form;
			return false;
		}

		if (debug){	//print all what's being sent through network (http headers)
			std::ostringstream ostr2;
			req.write(ostr2);
			std::string s = ostr2.str();
			std::cout << "HttpFormManager:: HTMLRequest follows >>" << endl;
			std::cout << s << endl;
		}
				
		HTTPResponse res;
		istream& rs = session.receiveResponse(res);

		//fill in the return object
		resp->status = res.getStatus();
		resp->timestamp = res.getDate();
		resp->reasonForStatus = res.getReasonForStatus( res.getStatus() );
		resp->contentType = res.getContentType();
		
		if (debug) printf("HttpFormManager::executeForm() >> server reports request staus: (%d-%s)\n", resp->status, resp->reasonForStatus.c_str() );
		
		delete form2;	//we might be leaking here if an exception rises before we get to this point! TODO! uri
		delete form;

		
		if (timeToStop) {
			printf("HttpFormManager::executeForm() >> time to stop! \n");
			return false;
		};
		
		try{
			StreamCopier::copyToString(rs, resp->responseBody);	//copy the data...		
		}catch(Exception& exc){
			ofLog( OF_LOG_ERROR, "HttpFormManager::executeForm(%s) >> Exception while copyToString: %s\n", resp->action.c_str(), exc.displayText().c_str() );
			delete form2;	//we might be leaking here if an exception rises before we get to this point! TODO! uri
			delete form;
			return false;
		}

		if (debug) printf("HttpFormManager::executeForm() >> server response: \n\n######################## SERVER RESPONSE ########################\n\n%s\n#################################################################\n\n", resp->responseBody.c_str() );
		printf("Successfully commited form %s!\n",  resp->action.c_str());
		
		if (resp->submissionCanceled){
			if(debug) printf("HttpFormManager::executeForm() >> submit (%s) canceled!\n", resp->action.c_str());
			return false;
		}
		
		if(debug) printf("HttpFormManager::executeForm() >> submitted form! (%s)\n", resp->action.c_str());
		
		resp->ok = true;
		
		if (sendResultThroughEvents ){	
			if ( !resp->ignoreReply )
				if (timeToStop == false)	//see if we have been destructed!
					ofNotifyEvent( formResponseEvent, *resp, this );	
		}

	}catch(Exception& exc){
		ofLog( OF_LOG_ERROR, "HttpFormManager::executeForm(%s) >> Exception: %s\n", resp->action.c_str(), exc.displayText().c_str() );
		resp->ok = FALSE;
	}
	return resp->ok;
}

void HttpFormManager::threadedFunction(){

	if (debug) printf("\nHttpFormManager >> start threadedFunction\n");
	int pending = 0;
	
	lock();
	pending = q.size();
	unlock();
	
	while( pending > 0 && timeToStop == false){
		
		lock();
			HttpFormResponse * r = q.front();
		unlock();
		
		executeForm(r, true);
		lock();
			delete r;
			q.pop();
			pending = q.size();
		unlock();
	}
	//if no more pending requests, let the thread die...
	if (debug) printf("HttpFormManager >> exiting threadedFunction (queue %d)\n",  (int)q.size());
	
	if (!timeToStop){
		if (debug) printf("detaching HttpFormManager thread!\n");
#ifdef TARGET_OSX
		pthread_detach(pthread_self());
#endif
/*
		detach();		//why? cos this is a 1-off thread, once the task is finished, this thread is to be cleared.
						//If not detached or joined with, it takes resources... neat, uh?
 */
	}

	
}