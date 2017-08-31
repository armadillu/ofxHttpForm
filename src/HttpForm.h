
#pragma once
#include <iostream>
#include <vector>
#include <map>

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

		HttpForm( std::string url_, int port_ = -1 ){
			url = url_;
			port = port_;
		}
		
		~HttpForm(){
			clearFormFields();
		}

		void setURL(std::string url_){
			url = url_;			
		}

		void setPort(int port_){
			port = port_;			
		}
	
		void addFormField(std::string ID, std::string value){
			formIdValues[ID] = value;
		}
		
		void clearFormFields(){
			formIdValues.clear();
			formBodyParts.clear();
		}
		
		void addFile(std::string fieldName, std::string path, std::string mimeType = "text/plain"){
			FormContent f;
			f.type = FormContent::CONTENT_TYPE_FILE;
			f.path = ofToDataPath(path, true);
			f.contentType = mimeType;
			formBodyParts[fieldName] = f;
		}
	
		void addString(std::string fieldName, std::string content, std::string mimeType = "text/plain") {
			FormContent f;
			f.type = FormContent::CONTENT_TYPE_STRING;
			f.content = content;
			f.contentType = mimeType;
			formBodyParts[fieldName] = f;
		}

		std::string getFieldValue(std::string ID){
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

