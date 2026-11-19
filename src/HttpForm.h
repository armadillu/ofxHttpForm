
#pragma once
#include <iostream>
#include <vector>
#include <map>
#include <string>

struct FormContent{
	enum Type {
		CONTENT_TYPE_FILE,
		CONTENT_TYPE_STRING
	} type;
	std::string content;
	std::string path;
	std::string contentType;
};

struct HttpForm{

		HttpForm(){
			url = "";
			port = -1;
		}

		HttpForm( const std::string& url_, int port_ = -1 ){
			url = url_;
			port = port_;
		}
		
		~HttpForm(){
			clearFormFields();
		}

		void setURL(const std::string& url_){
			url = url_;			
		}

		void setPort(int port_){
			port = port_;			
		}
	
		void addFormField(const std::string& ID, const std::string& value){
			formIdValues[ID] = value;
		}
		
		void clearFormFields(){
			formIdValues.clear();
			formBodyParts.clear();
		}
		
		void addFile(const std::string& fieldName, const std::string& path, const std::string& mimeType = "text/plain"){
			formBodyParts[fieldName] = FormContent{FormContent::CONTENT_TYPE_FILE, {}, ofToDataPath(path, true), mimeType};
		}
	
		void addString(const std::string& fieldName, const std::string& content, const std::string& mimeType = "text/plain") {
			formBodyParts[fieldName] = FormContent{FormContent::CONTENT_TYPE_STRING, content, {}, mimeType};
		}

		std::string getFieldValue(const std::string& ID){
			std::map<std::string, std::string>::iterator it = formIdValues.find(ID);
			if(it != formIdValues.end()){
				return it->second;
			}
			return "";
		}

		std::string toString(int cropValueLenTo = -1) const{
			std::string r = "HttpForm endpoint: " + url;
			if(port != -1) r += "  port: " + ofToString(port) + "\n";
			else r += "\n";

			auto it2 = formIdValues.begin();
			while( it2 != formIdValues.end() ){
				if(cropValueLenTo > 0){
					std::string val = it2->second;
					if(val.size() > cropValueLenTo) val = val.substr(0, cropValueLenTo) + "...";
					r+= it2->first + " : \"" + val + "\"\n";
				}else{
					r+= it2->first + " : \"" + it2->second + "\"\n";
				}
				++it2;
			}

			auto it = formBodyParts.begin();
			while( it != formBodyParts.end()){
				if (it->second.type == FormContent::CONTENT_TYPE_FILE) {
					r+= it->first + " (file) : " + it->second.path + " (" + it->second.contentType + ")\n";
				}
				else {
					r+= it->first + " (string) : " + it->second.content + "\n";
				}
				++it;
			}
			if(r.size() && r[r.size()-1] == '\n') r = r.substr(0, r.size() - 1); //remove last \n
			return r;
		}

		std::string url;
		int port;
		std::map<std::string, std::string> formIdValues;
		std::map<std::string, FormContent > formBodyParts;
};

