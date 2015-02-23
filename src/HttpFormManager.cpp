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
#include "Poco/Net/SSLManager.h"
#include "Poco/Net/HTTPSClientSession.h"
#include "Poco/Net/HTTPBasicCredentials.h"
#include "Poco/Net/ConsoleCertificateHandler.h"
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
	enableProxy = false;
	proxyPort = 80;
	usingCredentials = false;
}

HttpFormManager::~HttpFormManager(){

	timeToStop = true;	//lets flag the thread so that it doesnt try access stuff while we delete things around

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

void HttpFormManager::setProxy(bool enabled, string host, int port, string username, string password){
	enableProxy = enabled;
	proxyHost = host;
	proxyPort = port;
	proxyUsername = username;
	proxyPassword = password;
}

void HttpFormManager::setCredentials(string newUsername, string newPassword){
	username = newUsername;
	password = newPassword;
	usingCredentials = true;
}

void HttpFormManager::draw(int x, int y){
	
	char aux[2048];
	ofMutex::ScopedLock Lock( mutex );
		int n = q.size();
		if ( isThreadRunning() && n > 0 ){
			HttpFormResponse * r = q.front();
			sprintf(aux, "HttpFormManager: Submitting\n%s \nQueue Size: %d", r->url.c_str(),  n);
		}else{
			sprintf(aux, "HttpFormManager : Idle");
		}
		glColor3ub(255,0,0);
		ofDrawBitmapString(aux, x, y);
}


HttpFormResponse* HttpFormManager::createFormRespPtrFromForm( HttpForm f ){

 	HttpFormResponse *form = new HttpFormResponse();
	form->url = f.url;
	form->port = f.port;
	form->formIdValues = f.formIdValues;
	form->formFiles = f.formFiles;
	return form;
}


void HttpFormManager::submitForm( HttpForm f, bool ignoreReply, string identifier ){

 	HttpFormResponse *form = createFormRespPtrFromForm( f );
	//form->submissionCanceled = false;
	form->ignoreReply = ignoreReply;
    form->identifier = identifier;

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
	form.formIdValues = f.formIdValues;
	form.formFiles = f.formFiles;
	//form.submissionCanceled = false;
	form.ignoreReply = false;

	
	bool ok = executeForm( &form, false);
	if (!ok){
		ofLogError("HttpFormManager") << "ExecuteForm() failed! ";
		ofLogError("HttpFormManager") << "\n" << f.toString(100);
		ofLogError("HttpFormManager") << "HttpStatus: " << form.status;
		ofLogError("HttpFormManager") << "Reason: " << form.reasonForStatus;
		ofLogError("HttpFormManager") << "Server Reply: '" << form.responseBody << "'";
		ofLogError("HttpFormManager") << "Time Elapsed: " << form.totalTime << " sec";
	}
	return form;
}

int HttpFormManager::getQueueLength(){
	ofMutex::ScopedLock Lock( mutex );
	int queueLen = 0;
	queueLen = q.size();
	return queueLen;
}


HTMLForm* HttpFormManager::createPocoFormFrom( HttpFormResponse * resp ){
	
	HTMLForm *form = new HTMLForm();
	
	if( resp->formFiles.size() > 0 )
		form->setEncoding(HTMLForm::ENCODING_MULTIPART);
	else
		form->setEncoding(HTMLForm::ENCODING_URL);
					
	// form values
	{
		map<string,string>::iterator it = resp->formIdValues.begin();
		while(it != resp->formIdValues.end()){
			const std::string name = it->first.c_str();
			const std::string val = it->second.c_str();
			form->set(name, val);
			++it;
		}
	}

	map<string,FormContent>::iterator it;
	for( it = resp->formFiles.begin(); it != resp->formFiles.end(); it++ ){	
		try{
			string path = it->second.path;
			FilePartSource * file = new FilePartSource(  path, it->second.contentType );
			form->addPart( it->first, file );
		}catch(...){
			ofLogError("HttpFormManager") << "createPocoFormFrom() form file not found! " << it->second.path;
			delete form;
			return NULL;
		}

	}
	return form;
}


bool HttpFormManager::executeForm( HttpFormResponse* resp, bool sendResultThroughEvents ){  

	HTMLForm *form = NULL;
	HTMLForm *placeholderForm = NULL;
	HTTPClientSession * httpSession = NULL;

	try{
		Poco::URI uri( resp->url );
		if(resp->port != -1) uri.setPort(resp->port);
		std::string path(uri.getPathAndQuery());
		if (path.empty()) path = "/";
		resp->action = path;

		float t = ofGetElapsedTimef();
		HTTPResponse res;
		HTTPRequest req(HTTPRequest::HTTP_POST, path, HTTPMessage::HTTP_1_1);

		req.set( "User-Agent", userAgent.c_str() );
		if (acceptString.length() > 0){
			req.set( "Accept", acceptString.c_str() );
		}

		if(usingCredentials){
			Poco::Net::HTTPBasicCredentials cred(username, password);
			cred.authenticate(req);
		}

		//long story short of why we fill in two forms:
		//we need to specify exact lenght of the data in the form (file s headers), but we can't really measure it untill its been sent
		//so we create 2 indetical forms, one just to measure its size, other to use for sending thorugh the request
		form = createPocoFormFrom(resp);
		if (form == NULL) return false;
		placeholderForm = createPocoFormFrom(resp);

		form->prepareSubmit(req);
		placeholderForm->prepareSubmit(req);

		req.setChunkedTransferEncoding(false);
		//req.setKeepAlive(true);
		//req.set("Accept" , "*/*" );
		//req.set("Origin", "http://" + uri.getHost() + ":" + ofToString(uri.getPort()) );
		//req.set("Host",  uri.getHost() + ":" + ofToString(uri.getPort()) );

		//lets find out the length of the total data we are sending and report it				
		std::ostringstream formDumpContainer;
		placeholderForm->write(formDumpContainer);
		delete placeholderForm;
		placeholderForm = NULL;
		req.setContentLength( formDumpContainer.str().length() );	//finally we can specify exact content length in the request

		if(uri.getScheme()=="https"){
			httpSession = new HTTPSClientSession(uri.getHost(), uri.getPort());//,context);
		}else{
			httpSession = new HTTPClientSession(uri.getHost(), uri.getPort());
		}

		httpSession->setTimeout( Poco::Timespan(timeOut,0) );
		if(enableProxy){
			httpSession->setProxy(proxyHost, proxyPort);
			httpSession->setProxyCredentials(proxyUsername, proxyPassword);
		}//else{
		//	ofLogNotice("HttpFormManager") << "NO PROXY USED! " << resp->action;
		//}

		form->write(httpSession->sendRequest(req));
		istream * rsP = NULL;
		try{
			istream & rs = httpSession->receiveResponse(res);
			resp->totalTime = ofGetElapsedTimef() - t;
			//ofLogNotice("HttpFormManager") << "Response Took: " << resp->totalTime << " seconds";
			rsP = &rs;
		}catch(Exception& exc){
			resp->status = res.getStatus();
			resp->reasonForStatus = res.getReasonForStatus( res.getStatus() );
			ofLogError("HttpFormManager") << exc.displayText() ;
			float responseTime = ofGetElapsedTimef() - t;
			if(responseTime > timeOut){
				ofLogError("HttpFormManager") << "most likely a timeOut! took " << responseTime << " seconds";
				resp->status = HTTPResponse::HTTP_REQUEST_TIMEOUT;
				resp->reasonForStatus = res.getReasonForStatus( (HTTPResponse::HTTPStatus)resp->status);
				resp->totalTime = ofGetElapsedTimef() - t;
			}
			if(rsP == NULL){
				//httpSession->abort();
				//cleanup and return early
				if(form) delete form;
				if(httpSession) delete httpSession;
				if(placeholderForm) delete placeholderForm;
				resp->ok = false;
				return false;
			}else{
				ofLogError("HttpFormManager") << "??";
			}
		}

		try{
			StreamCopier::copyToString(*rsP, resp->responseBody);	//copy the response data...
		}catch(Exception& exc){
			ofLogError("HttpFormManager") << "cant copy stream!";
		}

		if (debug){	//print all what's being sent through network (http headers)
			std::ostringstream ostr2;
			req.write(ostr2);
			std::string s = ostr2.str();
			ofLogNotice("HttpFormManager") << "HTMLRequest follows >> ";
			ofLogNotice("HttpFormManager") << s;
		}

		//fill in the return object
		resp->status = res.getStatus();
		resp->reasonForStatus = res.getReasonForStatus( res.getStatus() );
		resp->timestamp = res.getDate();
		resp->contentType = res.getContentType();
		resp->totalTime = ofGetElapsedTimef() - t;

		//delete createdobjects
		if (form) delete form;
		form = NULL;
		if (placeholderForm) delete placeholderForm;
		placeholderForm = NULL;
		if(httpSession)delete httpSession;
		httpSession = NULL;

		//handle redirects?
//		if(res.getStatus() >= 300 && res.getStatus() < 400){
//			Poco::URI uri(req.getURI());
//			uri.resolve(res.get("Location"));
//			//uri.toString();
//		}

		if (debug) {
			ofLogNotice("HttpFormManager") << "executeForm() >> server reports request status: (" << resp->status << " - " << resp->reasonForStatus << ")";
		}

		if (timeToStop) {
			ofLogNotice("HttpFormManager") << "executeForm() >> time to stop!";
			return false;
		};
		
		try{

			StreamCopier::copyToString(*rsP, resp->responseBody);	//copy the response data...

		}catch(Exception& exc){
			ofLogError("HttpFormManager") << "executeForm(" << resp->action <<  ") >> Exception while copyToString: " << exc.displayText();
			resp->ok = false;
			return false;
		}

		if (debug){
			ofLogNotice("HttpFormManager") << "executeForm() >> submitted form! ("<< resp->action << ")";
			ofLogNotice("HttpFormManager") << endl <<
			"\n\n######################## SERVER RESPONSE ########################\n\n" <<
			  resp->responseBody <<
			"\n#################################################################\n\n";
		}

		switch(resp->status){
			case HTTPResponse::HTTP_OK:
			case HTTPResponse::HTTP_CREATED:
				resp->ok = true; break;
			default:
				resp->ok = false; break;
		}

		if (sendResultThroughEvents ){	
			if ( !resp->ignoreReply )
				if (timeToStop == false)	//see if we have been destructed!
					ofNotifyEvent( formResponseEvent, *resp, this );	
		}

	}catch(Exception& exc){
		ofLogError("HttpFormManager") << "executeForm(" << resp->action << ") >> Exception: " << exc.displayText();
		resp->ok = FALSE;
		//clean up
		if(form) delete form;
		if(httpSession) delete httpSession;
		if(placeholderForm) delete placeholderForm;
	}
	return resp->ok;
}

void HttpFormManager::threadedFunction(){

	if (debug) ofLogNotice("HttpFormManager") << "start threadedFunction";
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
	if (debug) ofLogNotice("HttpFormManager") << "HttpFormManager >> exiting threadedFunction (queue " << (int)q.size() <<")";
	
	if (!timeToStop){
		if (debug) ofLogNotice("HttpFormManager") << "detaching HttpFormManager thread!";
	}
}


void HttpFormResponse::print(){
	ofLogNotice("HttpFormManager") << toString();
}


string HttpFormResponse::toString(){
	stringstream ss;

	ss << "HttpFormManager: " << identifier << " : " << url << endl;
	ss << "    action: " << url << endl;
	{
		map<string,string>::iterator it = formIdValues.begin();
		while( it != formIdValues.end()){
			ss << "    ID: '" << it->first << "'  Value: '" << it->second << "'" << endl;
			++it;
		}
	}
	std::map<string, FormContent>::iterator it = formFiles.begin();
	while(it != formFiles.end()){
		ss << "    FileID: '" << it->first << "'  Path: '" << it->second.path << "'  CntType:  '" << it->second.contentType << "'" << endl;
		++it;
	}
	ss << "    status: " << status << " (" << reasonForStatus << ")" << endl;
	ss << "    response: '" << responseBody << "'" << endl;
	return ss.str();
}

