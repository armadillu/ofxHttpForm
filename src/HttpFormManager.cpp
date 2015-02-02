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


void HttpFormManager::submitForm( HttpForm f, bool ignoreReply, string identifier ){

 	HttpFormResponse *form = createFormRespPtrFromForm( f );
	//form->submissionCanceled = false;
	form->ignoreReply = ignoreReply;
	//form->session = NULL;
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
	form.formIds = f.formIds;
	form.formValues = f.formValues;
	form.formFiles = f.formFiles;
	//form.submissionCanceled = false;
	form.ignoreReply = false;
	//form.session = NULL;
	
	bool ok = executeForm( &form, false);
	if (!ok){
		ofLogError("HttpFormManager") << "executeForm() failed!";
		ofLogError("HttpFormManager") << "HttpStatus: " << form.status << " Reason: " << form.reasonForStatus;
		ofLogError("HttpFormManager") << "Server Reply: " << form.responseBody;
	}
	return form;
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
		try{
			string path = it->second.path;
			FilePartSource * file = new FilePartSource(  path, it->second.contentType );
			form->addPart( it->first, file );
		}catch(...){
			ofLog( OF_LOG_FATAL_ERROR, "HttpFormManager::createPocoFormFrom() form file not found! %s\n", it->second.path.c_str());
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
		std::string path(uri.getPathAndQuery());
		if (path.empty()) path = "/";
		resp->action = path;

		HTTPResponse res;
		HTTPRequest req(HTTPRequest::HTTP_POST, path, HTTPMessage::HTTP_1_1);

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

		istream * rs;
		if(uri.getScheme()=="https"){
			httpSession = new HTTPSClientSession(uri.getHost(), uri.getPort());//,context);
			httpSession->setTimeout( Poco::Timespan(timeOut,0) );
		}else{
			httpSession = new HTTPClientSession(uri.getHost(), uri.getPort());
			httpSession->setTimeout( Poco::Timespan(timeOut,0) );
		}

		if(enableProxy){
			httpSession->setProxy(proxyHost, proxyPort);
			httpSession->setProxyCredentials(proxyUsername, proxyPassword);
		}

		form->write(httpSession->sendRequest(req));
		rs = &httpSession->receiveResponse(res);

		if (debug){	//print all what's being sent through network (http headers)
			std::ostringstream ostr2;
			req.write(ostr2);
			std::string s = ostr2.str();
			std::cout << "HttpFormManager:: HTMLRequest follows >>" << endl;
			std::cout << s << endl;
		}

		//handle redirects?
//		if(res.getStatus() >= 300 && res.getStatus() < 400){
//			Poco::URI uri(req.getURI());
//			uri.resolve(res.get("Location"));
//			//uri.toString();
//		}

		//fill in the return object
		resp->status = res.getStatus();
		resp->timestamp = res.getDate();
		resp->reasonForStatus = res.getReasonForStatus( res.getStatus() );
		resp->contentType = res.getContentType();
		
		if (debug) ofLog(OF_LOG_NOTICE, "HttpFormManager::executeForm() >> server reports request status: (%d-%s)\n", resp->status, resp->reasonForStatus.c_str() );

		delete form;
		form = NULL;

		delete httpSession;
		httpSession = NULL;
		
		if (timeToStop) {
			ofLog(OF_LOG_NOTICE, "HttpFormManager::executeForm() >> time to stop! \n");
			return false;
		};
		
		try{

			StreamCopier::copyToString(*rs, resp->responseBody);	//copy the response data...

		}catch(Exception& exc){
			ofLog( OF_LOG_ERROR, "HttpFormManager::executeForm(%s) >> Exception while copyToString: %s\n", resp->action.c_str(), exc.displayText().c_str() );
			resp->ok = false;
			//clean up
			if(form) delete form;
			if(httpSession) delete httpSession;
			if(placeholderForm) delete placeholderForm;
			return false;
		}

		if (debug){
			ofLog(OF_LOG_NOTICE, "HttpFormManager::executeForm() >> server response: "
				   "\n\n######################## SERVER RESPONSE ########################\n\n"
				   "%s\n#################################################################\n\n",
				   resp->responseBody.c_str() );
		}

		if(debug) ofLog(OF_LOG_NOTICE, "HttpFormManager::executeForm() >> submitted form! (%s)\n", resp->action.c_str());
		
		resp->ok = true;
		if(resp->status != 200){
			resp->ok = false;
		}

		if (sendResultThroughEvents ){	
			if ( !resp->ignoreReply )
				if (timeToStop == false)	//see if we have been destructed!
					ofNotifyEvent( formResponseEvent, *resp, this );	
		}

	}catch(Exception& exc){
		ofLog( OF_LOG_ERROR, "HttpFormManager::executeForm(%s) >> Exception: %s\n", resp->action.c_str(), exc.displayText().c_str() );
		resp->ok = FALSE;
		//clean up
		if(form) delete form;
		if(httpSession) delete httpSession;
		if(placeholderForm) delete placeholderForm;
	}
	return resp->ok;
}

void HttpFormManager::threadedFunction(){

	if (debug) ofLog(OF_LOG_NOTICE, "\nHttpFormManager >> start threadedFunction\n");
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
	if (debug) ofLog(OF_LOG_NOTICE, "HttpFormManager >> exiting threadedFunction (queue %d)\n",  (int)q.size());
	
	if (!timeToStop){
		if (debug) ofLog(OF_LOG_NOTICE, "detaching HttpFormManager thread!\n");
	}
	
}
