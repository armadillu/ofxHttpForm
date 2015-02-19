
#pragma once
#include <iostream>
#include <vector>
#include <map>

struct FormContent{
	string path;
	string contentType;
};

struct HttpForm{

		HttpForm(){
			url = "";
			port = -1;
		}

		HttpForm( string url_, int port_ = -1 ){
			url = url_;
			port = port_;
		}
		
		~HttpForm(){
			clearFormFields();
		}

		void setURL(string url_){
			url = url_;			
		}

		void setPort(int port_){
			port = port_;			
		}
	
		void addFormField(string ID, string value){
			formIdValues[ID] = value;
		}
		
		void clearFormFields(){
			formIdValues.clear();
			formFiles.clear();
		}
		
		void addFile(string fieldName, string path, string mimeType = "text/plain"){
			FormContent f;
			f.path = ofToDataPath(path, true);
			f.contentType = mimeType;
			formFiles[fieldName] = f;
		}

		string getFieldValue(string ID){
			std::map<string, string>::iterator it = formIdValues.find(ID);
			if(it != formIdValues.end()){
				return it->second;
			}
			return "";
		}

		string toString(int cropValueLenTo = -1){
			string r = "HttpForm endpoint: " + url;
			if(port != -1) r += "  port: " + ofToString(port) + "\n";
			else r += "\n";

			std::map<string, string>::iterator it2 = formIdValues.begin();
			while( it2 != formIdValues.end() ){
				if(cropValueLenTo > 0){
					string val = it2->second;
					if(val.size() > cropValueLenTo) val = val.substr(0, cropValueLenTo) + "...";
					r+= it2->first + " : " + val + "\n";
				}else{
					r+= it2->first + " : " + it2->second + "\n";
				}
				++it2;
			}

			std::map<string, FormContent >::iterator it = formFiles.begin();
			while( it != formFiles.end()){
				r+= it->first + " : " + it->second.path + "(" + it->second.contentType + ")\n";
				++it;
			}
			return r;
		}

		string url;
		int port;
		std::map<string, string> formIdValues;
		std::map<string, FormContent > formFiles;
};

