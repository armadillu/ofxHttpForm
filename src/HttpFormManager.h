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
using namespace Poco;
using Poco::Exception;
using Poco::Net::HTTPClientSession;

struct HttpFormResponse{
	
	//summary, to quickly check if went ok
	bool						ok;
	
	//submitted form contents
	string						url;
	string						action;	
	vector <string>				formIds;
	vector <string>				formValues;
	std::map<string, FormContent> formFiles;

	//more detailed response info & session
	bool						ignoreReply;
	//bool						submissionCanceled;
	int							status; 			// return code for the response ie: 200 = OK
	string						reasonForStatus;	// text explaining the status
	string						responseBody;		// the actual response
	string						contentType;		// the mime type of the response
	Poco::Timestamp				timestamp;			// time of the response
	//HTTPClientSession *			session;
};


class HttpFormManager : public ofThread{
	
	public:

		HttpFormManager();
		~HttpFormManager();

	//  actions  //////////////////////////////////////////////////////////////

		//once your form is all set (you added all fields, etc), add it to the manager's queue to process									
		void submitForm( HttpForm form, bool ignoreReply = true );	//enque, process in background thread (if ignoreReply==true, you will not get notified thorugh OFEvents when form is sent)
		HttpFormResponse submitFormBlocking( HttpForm form );		//blocking, stops main thread
			
		void draw(int x = 20, int y = 20);	//see queue progress on screen
		int	getQueueLength();

	//  settings  /////////////////////////////////////////////////////////////

		void setTimeOut( int seconds );
		void setVerbose(bool verbose);
		void setUserAgent( string newUserAgent );
		void setAcceptString( string newAcceptString );
		
		ofEvent<HttpFormResponse> formResponseEvent;
	
	private:
		
		void threadedFunction();	//the queue runs here
		HTMLForm* createPocoFormFrom( HttpFormResponse * r );

		HttpFormResponse* createFormRespPtrFromForm( HttpForm f );
	
		bool executeForm( HttpFormResponse* form, bool sendResultThroughEvents );
	
		
		bool							debug;	//should we print lots of stuff?
		int								timeOut;
		string							userAgent;
		string							acceptString;
		queue<HttpFormResponse*>		q;		//the pending forms
		bool							timeToStop;

	
};
