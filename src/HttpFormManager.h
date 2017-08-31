/*
 *  HttpFormManager.h
 *  emptyExample
 *
 *  Created by Oriol Ferrer Mesià on 03/02/11.
 *  Copyright 2011 uri.cat. All rights reserved.
 *
 */

#pragma once

#include "ofMain.h"
#include "ofEvents.h"
#include <queue>
#include <stdio.h>
#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Net/HTMLForm.h"
#include "Poco/StreamCopier.h"
#include "Poco/Path.h"
#include "Poco/URI.h"
#include "Poco/Exception.h"
#include <iostream>

#include "HttpForm.h"
using namespace Poco::Net;
//using namespace Poco;
using Poco::Exception;
using Poco::Net::HTTPClientSession;
using namespace std;


struct HttpFormResponse{
	
	//summary, to quickly check if went ok
	bool						ok;
	
	//submitted form contents
	std::string						url;
	int							port;
	std::string						action;
	std::map<std::string,std::string>		formIdValues;
	std::map<std::string, FormContent> formBodyParts;

	//more detailed response info & session
	bool						ignoreReply;
	//bool						submissionCanceled;
	HTTPResponse::HTTPStatus	status; 			// return code for the response ie: 200 = OK
	std::string						reasonForStatus;	// text explaining the status
	std::string						responseBody;		// the actual response
	std::string						contentType;		// the mime type of the response
	Poco::Timestamp				timestamp;			// time of the response
	float						totalTime;			//time it took to get a response

    std::string                      identifier;
	std::string						toString();
	void						print();

	HttpFormResponse(){
		status = HTTPResponse::HTTP_NOT_FOUND;
		port = -1;
		totalTime = 0.0f;
	}

};


class HttpFormManager : public ofThread{
	
	public:

		HttpFormManager();
		~HttpFormManager();

	//  actions  //////////////////////////////////////////////////////////////

		//once your form is all set (you added all fields, etc), add it to the manager's queue to process									
		void submitForm( HttpForm form, bool ignoreReply = true, std::string identifier = "");	//enque, process in background thread (if ignoreReply==true, you will not get notified thorugh OFEvents when form is sent)
		HttpFormResponse submitFormBlocking( const HttpForm & form );		//blocking, stops main thread
	
		void draw(int x = 20, int y = 20);	//see queue progress on screen
		int	getQueueLength();

	//  settings  /////////////////////////////////////////////////////////////

		void setTimeOut( int seconds );
		void setVerbose(bool verbose);
		void setUserAgent( std::string newUserAgent );
		void setAcceptString( std::string newAcceptString );

		void setProxy(bool enableProxy, std::string host, int port, std::string username, std::string password);
    
		void setCredentials(std::string username, std::string password);
		
		ofEvent<HttpFormResponse> formResponseEvent;

	private:
		
		void threadedFunction();	//the queue runs here
		HTMLForm* createPocoFormFrom( HttpFormResponse * r );

		HttpFormResponse* createFormRespPtrFromForm( HttpForm f );
	
		bool executeForm( HttpFormResponse* form, bool sendResultThroughEvents );
	
		
		bool							debug;	//should we print lots of stuff?
		int								timeOut;
		std::string							userAgent;
		std::string							acceptString;
		std::queue<HttpFormResponse*>		q;		//the pending forms
		bool							timeToStop;

		//proxy config
		bool 							enableProxy;
		std::string							proxyHost;
		int								proxyPort;
		std::string							proxyUsername;
		std::string							proxyPassword;
    
		//credentials
		std::string							username;
		std::string							password;
		bool							usingCredentials;
	
};
